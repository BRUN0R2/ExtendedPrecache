#include "clientEngine.hpp"
#include "clientTransport.hpp"
#include "engineIdentity.hpp"

#include <windows.h>

#include <new>
#include <string_view>
#include <utility>

namespace {
using namespace extendedPrecache::platform::windows;

constexpr auto engineModuleName = L"hw.dll";
constexpr auto selectedEngineSha256 =
    std::string_view{"9ba9a2db5e07598fd59afa35507a98c86162e4e15b3835177b78c11842cd2295"};

// Returned to whoever loaded this module, because the engine's console may not exist yet.
enum class AttachResult : std::uint8_t {
    installed,
    noApprovedProfile,
    engineNotLoaded,
    engineUnreadable,
    engineUnknownBuild,
    noBinding,
    unexpectedTables,
    outOfMemory,
    cannotPin
};

// The engine's tables will point into this module, which must then outlive its loader's intent.
auto pinThisModule() noexcept -> bool {
    auto module = HMODULE{};
    const auto flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN;
    return GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(&engineModuleName), &module) !=
           FALSE;
}

auto attach() -> AttachResult {
    auto profile = client::approvedClientProfile();
    if (!profile) {
        return AttachResult::noApprovedProfile;
    }
    auto *const module = GetModuleHandleW(engineModuleName);
    if (module == nullptr) {
        return AttachResult::engineNotLoaded;
    }
    if (const auto verified = verifyEngineBuild(module, selectedEngineSha256); !verified) {
        return verified.error() == EngineRefusal::unknownBuild ? AttachResult::engineUnknownBuild
                                                               : AttachResult::engineUnreadable;
    }
    if (!pinThisModule()) {
        return AttachResult::cannotPin;
    }
    const auto installed =
        client::installClientTransport(client::ClientEngine{module}, std::move(profile));
    if (!installed) {
        return installed.error() == client::TransportRefusal::noBinding
                   ? AttachResult::noBinding
                   : AttachResult::unexpectedTables;
    }
    return AttachResult::installed;
}
} // namespace

// Runs outside the loader lock, on the thread its caller chooses, before the first connection.
extern "C" auto extendedPrecacheClientAttach() noexcept -> std::int32_t {
    try {
        return std::to_underlying(attach());
    } catch (const std::bad_alloc &) {
        return std::to_underlying(AttachResult::outOfMemory);
    }
}
