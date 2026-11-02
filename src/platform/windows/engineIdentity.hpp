#pragma once

#include "rehldsApi.hpp"

#include <cstdint>
#include <expected>
#include <string_view>

namespace extendedPrecache::platform::windows {

enum class EngineRefusal : std::uint8_t { notDedicated, unreadable, unknownBuild, noApi };

// Compares the file a loaded module came from against the build it was verified on.
[[nodiscard]] auto verifyEngineBuild(void *module, std::string_view sha256)
    -> std::expected<void, EngineRefusal>;
// Opens the API only of the exact engine build these layouts were verified against.
[[nodiscard]] auto openSelectedEngine() -> std::expected<rehlds::Api, EngineRefusal>;
[[nodiscard]] auto describe(EngineRefusal refusal) noexcept -> const char *;

} // namespace extendedPrecache::platform::windows
