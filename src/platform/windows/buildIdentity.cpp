#include "buildIdentity.hpp"

#include <extendedPrecache/platform/windows/cngDigest.hpp>

#include <windows.h>

#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

namespace extendedPrecache::platform::windows {
namespace {
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

auto verifyFileBuild(const std::filesystem::path &file, std::string_view sha256)
    -> std::expected<void, BuildRefusal> {
    const auto contents = readFile(file);
    if (!contents) {
        return std::unexpected{BuildRefusal::unreadable};
    }
    const auto digest = CngDigest{}.sha256(*contents);
    const auto selected = session::parseDigest(sha256);
    if (!digest || !selected) {
        return std::unexpected{BuildRefusal::unreadable};
    }
    if (*digest != *selected) {
        return std::unexpected{BuildRefusal::unknownBuild};
    }
    return {};
}

auto verifyModuleBuild(void *module, std::string_view sha256) -> std::expected<void, BuildRefusal> {
    const auto path = modulePath(static_cast<HMODULE>(module));
    if (!path) {
        return std::unexpected{BuildRefusal::unreadable};
    }
    return verifyFileBuild(*path, sha256);
}

} // namespace extendedPrecache::platform::windows
