#include "engineIdentity.hpp"

#include <extendedPrecache/platform/windows/cngDigest.hpp>

#include <windows.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

namespace extendedPrecache::platform::windows {
namespace {
constexpr auto engineModuleName = L"swds.dll";
constexpr auto selectedEngineSha256 =
    std::string_view{"f6c552ca46e933c6d02ee4fb888cc99bbe8f8aab04a4afbe9ff45d74f896cd02"};
constexpr auto apiInterfaceName = "VREHLDS_HLDS_API_VERSION001";
constexpr auto apiFactoryName = "CreateInterface";
constexpr DWORD longestPath = 32768;

auto modulePath(HMODULE module) -> std::optional<std::filesystem::path> {
    auto text = std::wstring(longestPath, L'\0');
    const auto length = GetModuleFileNameW(module, text.data(), longestPath);
    if (length == 0 || length == longestPath) {
        return std::nullopt;
    }
    text.resize(length);
    return std::filesystem::path{text};
}

auto readFile(const std::filesystem::path &path) -> std::optional<std::vector<std::byte>> {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::nullopt;
    }
    const auto characters =
        std::vector<char>{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (input.bad()) {
        return std::nullopt;
    }
    const auto bytes = std::as_bytes(std::span{characters});
    return std::vector<std::byte>{bytes.begin(), bytes.end()};
}
} // namespace

auto verifyEngineBuild(void *module, std::string_view sha256)
    -> std::expected<void, EngineRefusal> {
    const auto path = modulePath(static_cast<HMODULE>(module));
    const auto contents = path ? readFile(*path) : std::nullopt;
    if (!contents) {
        return std::unexpected{EngineRefusal::unreadable};
    }
    const auto digest = CngDigest{}.sha256(*contents);
    const auto selected = session::parseDigest(sha256);
    if (!digest || !selected) {
        return std::unexpected{EngineRefusal::unreadable};
    }
    if (*digest != *selected) {
        return std::unexpected{EngineRefusal::unknownBuild};
    }
    return {};
}

auto openSelectedEngine() -> std::expected<rehlds::Api, EngineRefusal> {
    auto *const module = GetModuleHandleW(engineModuleName);
    if (module == nullptr) {
        return std::unexpected{EngineRefusal::notDedicated};
    }
    if (const auto verified = verifyEngineBuild(module, selectedEngineSha256); !verified) {
        return std::unexpected{verified.error()};
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
