#include "buildIdentity.hpp"
#include "clientEngine.hpp"
#include "clientLoadStatus.hpp"
#include "clientTransport.hpp"

#include <windows.h>

#include <new>
#include <utility>

namespace {
using namespace extendedPrecache::platform::windows;

constexpr auto engineModuleName = L"hw.dll";
constexpr ULONG dllLoadedReason = 1;

// ntdll exports these entries without declaring them in a public header.
struct CountedText final {
    USHORT length;
    USHORT capacity;
    const wchar_t *text;
};
struct DllEvent final {
    ULONG flags;
    const CountedText *fullName;
    const CountedText *baseName;
    void *base;
    ULONG size;
};
using DllEventHandler = void(NTAPI *)(ULONG reason, const DllEvent *event, void *context);
using RegisterDllEvents = LONG(NTAPI *)(ULONG flags, DllEventHandler handler, void *context,
                                        void **cookie);
using UnregisterDllEvents = LONG(NTAPI *)(void *cookie);

// Runs under the loader lock, so it only compares a name and signals.
void NTAPI onDllEvent(ULONG reason, const DllEvent *event, void *context) {
    if (reason != dllLoadedReason) {
        return;
    }
    const auto &name = *event->baseName;
    const auto characters = static_cast<int>(name.length) / static_cast<int>(sizeof(wchar_t));
    if (CompareStringOrdinal(name.text, characters, engineModuleName, -1, TRUE) == CSTR_EQUAL) {
        SetEvent(static_cast<HANDLE>(context));
    }
}

template <typename Function> auto ntdllEntry(const char *name) noexcept -> Function {
    return reinterpret_cast<Function>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), name));
}

// Blocks until the engine is mapped; it stays blocked in a game that never loads this engine.
auto waitForEngine() noexcept -> HMODULE {
    const auto registerEvents = ntdllEntry<RegisterDllEvents>("LdrRegisterDllNotification");
    const auto unregisterEvents = ntdllEntry<UnregisterDllEvents>("LdrUnregisterDllNotification");
    auto *const loaded = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    void *cookie = nullptr;
    const auto watching = registerEvents != nullptr && unregisterEvents != nullptr &&
                          loaded != nullptr && registerEvents(0, onDllEvent, loaded, &cookie) == 0;
    if (watching && GetModuleHandleW(engineModuleName) == nullptr) {
        WaitForSingleObject(loaded, INFINITE);
    }
    if (watching) {
        unregisterEvents(cookie);
    }
    if (loaded != nullptr) {
        CloseHandle(loaded);
    }
    return GetModuleHandleW(engineModuleName);
}

auto attach(HMODULE engine) -> client::AttachResult {
    using client::AttachResult;
    auto profile = client::approvedClientProfile();
    if (!profile) {
        return AttachResult::noApprovedProfile;
    }
    if (engine == nullptr) {
        return AttachResult::engineUnreadable;
    }
    if (const auto verified = verifyModuleBuild(engine, client::selectedEngineSha256); !verified) {
        return verified.error() == BuildRefusal::unknownBuild ? AttachResult::engineUnknownBuild
                                                              : AttachResult::engineUnreadable;
    }
    const auto installed =
        client::installClientTransport(client::ClientEngine{engine}, std::move(profile));
    if (!installed) {
        return installed.error() == client::TransportRefusal::noBinding
                   ? AttachResult::noBinding
                   : AttachResult::unexpectedTables;
    }
    return AttachResult::installed;
}

auto attachWhenLoaded() noexcept -> client::AttachResult {
    try {
        return attach(waitForEngine());
    } catch (const std::bad_alloc &) {
        return client::AttachResult::outOfMemory;
    }
}

auto WINAPI attachThread(void *) -> DWORD {
    client::publishAttachResult(attachWhenLoaded());
    return 0;
}
} // namespace

// Everything else runs on its own thread, which starts once the loader lock is released.
auto WINAPI DllMain(HINSTANCE module, DWORD reason, void *) -> BOOL {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        auto *const thread = CreateThread(nullptr, 0, attachThread, nullptr, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
