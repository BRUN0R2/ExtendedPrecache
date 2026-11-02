#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string_view>

namespace extendedPrecache::platform::windows {

enum class BuildRefusal : std::uint8_t { unreadable, unknownBuild };

// Compares a file against the exact build its layouts were verified on.
[[nodiscard]] auto verifyFileBuild(const std::filesystem::path &file, std::string_view sha256)
    -> std::expected<void, BuildRefusal>;
// The same, for the file that a loaded module came from.
[[nodiscard]] auto verifyModuleBuild(void *module, std::string_view sha256)
    -> std::expected<void, BuildRefusal>;

} // namespace extendedPrecache::platform::windows
