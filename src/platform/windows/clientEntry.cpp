#include "buildIdentity.hpp"
#include "clientBuild.hpp"
#include "clientEngine.hpp"
#include "clientLoadStatus.hpp"
#include "clientModelDescriptors.hpp"
#include "clientModelTables.hpp"
#include "clientTransport.hpp"
#include "modelRelocation.hpp"

#include <windows.h>

#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <utility>

namespace {
using namespace extendedPrecache::platform::windows;

constexpr auto engineModuleName = L"hw.dll";
constexpr ULONG dllLoadedReason = 1;
constexpr DWORD longestPath = 32768;

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

template <typename Function> auto ntdllEntry(const char *name) noexcept -> Function {
    return reinterpret_cast<Function>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), name));
}

// Never destroyed: the engine's code keeps pointing at the descriptors until the process ends.
struct EngineWatch final {
    const client::ClientBuild *build;
    std::unique_ptr<client::ClientProfile> profile;
    std::unique_ptr<client::ClientModelDescriptors> descriptors;
    HANDLE loaded{};
    void *cookie{};
    bool descriptorsMoved{};
};

// Runs under the loader lock, on the thread loading the module and before any of its code runs.
void NTAPI onDllEvent(ULONG reason, const DllEvent *event, void *context) {
    if (reason != dllLoadedReason) {
        return;
    }
    const auto &name = *event->baseName;
    const auto characters = static_cast<int>(name.length) / static_cast<int>(sizeof(wchar_t));
    if (CompareStringOrdinal(name.text, characters, engineModuleName, -1, TRUE) != CSTR_EQUAL) {
        return;
    }
    auto &watch = *static_cast<EngineWatch *>(context);
    if (watch.descriptors) {
        watch.descriptorsMoved =
            watch.descriptors->moveInto(reinterpret_cast<std::byte *>(event->base));
    }
    SetEvent(watch.loaded);
}

auto attach(EngineWatch &watch) -> client::AttachResult {
    using client::AttachResult;
    auto *const engine = GetModuleHandleW(engineModuleName);
    if (engine == nullptr) {
        return AttachResult::engineUnreadable;
    }
    const auto &build = *watch.build;
    if (const auto verified = verifyModuleBuild(engine, build.engineSha256); !verified) {
        return verified.error() == BuildRefusal::unknownBuild ? AttachResult::engineUnknownBuild
                                                              : AttachResult::engineUnreadable;
    }
    if (watch.descriptors && !watch.descriptorsMoved) {
        return AttachResult::unexpectedTables;
    }
    const auto slotLimit = watch.descriptors ? wireModelSlots : build.slotsWithNativeDescriptors;
    const auto clientEngine = client::ClientEngine{engine, build.engine};
    const auto installed = client::installClientTransport(
        clientEngine, std::move(watch.profile),
        client::ClientModelTables{clientEngine.image(), build.modelTable, slotLimit});
    if (!installed) {
        return installed.error() == client::TransportRefusal::noBinding
                   ? AttachResult::noBinding
                   : AttachResult::unexpectedTables;
    }
    return AttachResult::installed;
}

auto WINAPI attachThread(void *context) -> DWORD {
    auto &watch = *static_cast<EngineWatch *>(context);
    // It stays blocked in a game that never loads this engine.
    WaitForSingleObject(watch.loaded, INFINITE);
    ntdllEntry<UnregisterDllEvents>("LdrUnregisterDllNotification")(watch.cookie);
    CloseHandle(watch.loaded);
    client::publishAttachResult([&] {
        try {
            return attach(watch);
        } catch (const std::bad_alloc &) {
            return client::AttachResult::outOfMemory;
        }
    }());
    return 0;
}

auto engineFile() -> std::filesystem::path {
    auto text = std::wstring(longestPath, L'\0');
    text.resize(GetModuleFileNameW(nullptr, text.data(), longestPath));
    return std::filesystem::path{text}.parent_path() / engineModuleName;
}

// Registers before the engine can load and returns to the game only when that is done, which is
// what lets the descriptors move before the engine makes its first one.
auto watchEngine() -> client::AttachResult {
    using client::AttachResult;
    auto profile = client::approvedClientProfile();
    if (!profile) {
        return AttachResult::noApprovedProfile;
    }
    const auto build = client::identifyClientBuild(engineFile());
    if (!build) {
        return build.error() == BuildRefusal::unknownBuild ? AttachResult::engineUnknownBuild
                                                           : AttachResult::engineUnreadable;
    }
    const auto registerEvents = ntdllEntry<RegisterDllEvents>("LdrRegisterDllNotification");
    const auto unregisterEvents = ntdllEntry<UnregisterDllEvents>("LdrUnregisterDllNotification");
    auto *const loaded = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (registerEvents == nullptr || unregisterEvents == nullptr || loaded == nullptr) {
        return AttachResult::engineUnreadable;
    }
    auto descriptors = client::ClientModelDescriptors::reserve(profile->knownModelEntries(),
                                                               (*build)->modelDescriptors);
    auto *const watch = new EngineWatch{*build, std::move(profile), std::move(descriptors), loaded};
    if (registerEvents(0, onDllEvent, watch, &watch->cookie) != 0) {
        return AttachResult::engineUnreadable;
    }
    // Not under the launcher, the engine may be there already, and then it is too late to move.
    if (GetModuleHandleW(engineModuleName) != nullptr) {
        SetEvent(loaded);
    }
    auto *const thread = CreateThread(nullptr, 0, attachThread, watch, 0, nullptr);
    if (thread == nullptr) {
        return AttachResult::engineUnreadable;
    }
    CloseHandle(thread);
    return AttachResult::pending;
}

// Runs on the game's main thread, ahead of its entry point and with the loader lock released.
void WINAPI prepareGame(ULONG_PTR) {
    const auto result = [] {
        try {
            return watchEngine();
        } catch (const std::bad_alloc &) {
            return client::AttachResult::outOfMemory;
        }
    }();
    if (result != client::AttachResult::pending) {
        client::publishAttachResult(result);
    }
}
} // namespace

auto WINAPI DllMain(HINSTANCE module, DWORD reason, void *) -> BOOL {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        QueueUserAPC(prepareGame, GetCurrentThread(), 0);
    }
    return TRUE;
}
