#pragma once

#include "rehldsApi.hpp"

#include <cstdint>
#include <expected>

namespace extendedPrecache::platform::windows {

enum class EngineRefusal : std::uint8_t { notDedicated, unreadable, unknownBuild, noApi };

// Opens the API only of the exact engine build these layouts were verified against.
[[nodiscard]] auto openSelectedEngine() -> std::expected<rehlds::Api, EngineRefusal>;
[[nodiscard]] auto describe(EngineRefusal refusal) noexcept -> const char *;

} // namespace extendedPrecache::platform::windows
