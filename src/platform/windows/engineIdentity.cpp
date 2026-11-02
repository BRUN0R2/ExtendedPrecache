#include "engineIdentity.hpp"

#include "buildIdentity.hpp"

#include <windows.h>

#include <string_view>

namespace extendedPrecache::platform::windows {
namespace {
constexpr auto engineModuleName = L"swds.dll";
constexpr auto selectedEngineSha256 =
    std::string_view{"f6c552ca46e933c6d02ee4fb888cc99bbe8f8aab04a4afbe9ff45d74f896cd02"};
constexpr auto apiInterfaceName = "VREHLDS_HLDS_API_VERSION001";
constexpr auto apiFactoryName = "CreateInterface";
} // namespace

auto openSelectedEngine() -> std::expected<rehlds::Api, EngineRefusal> {
    auto *const module = GetModuleHandleW(engineModuleName);
    if (module == nullptr) {
        return std::unexpected{EngineRefusal::notDedicated};
    }
    if (const auto verified = verifyModuleBuild(module, selectedEngineSha256); !verified) {
        return std::unexpected{verified.error() == BuildRefusal::unknownBuild
                                   ? EngineRefusal::unknownBuild
                                   : EngineRefusal::unreadable};
    }
    using InterfaceFactory = void *(*)(const char *name, int *status);
    const auto factory = reinterpret_cast<InterfaceFactory>(GetProcAddress(module, apiFactoryName));
    if (factory == nullptr) {
        return std::unexpected{EngineRefusal::noApi};
    }
    auto status = 0;
    const auto api = rehlds::Api::acquire(factory(apiInterfaceName, &status));
    if (!api) {
        return std::unexpected{EngineRefusal::noApi};
    }
    return *api;
}

auto describe(EngineRefusal refusal) noexcept -> const char * {
    switch (refusal) {
    case EngineRefusal::notDedicated:
        return "transport not installed: this is not the dedicated server engine";
    case EngineRefusal::unreadable:
        return "transport not installed: the engine file could not be verified";
    case EngineRefusal::unknownBuild:
        return "transport not installed: this engine build has not been verified";
    case EngineRefusal::noApi:
        return "transport not installed: the engine API is missing or incompatible";
    }
    return "transport not installed: unknown engine refusal";
}

} // namespace extendedPrecache::platform::windows
