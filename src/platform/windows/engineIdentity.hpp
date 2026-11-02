#pragma once

#include "rehldsApi.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>

namespace extendedPrecache::platform::windows {

enum class EngineRefusal : std::uint8_t { notDedicated, unreadable, unknownBuild, noApi };

struct SelectedEngine final {
    rehlds::Api api;
    std::byte *image;
};

// Opens only the exact engine build these layouts were verified against.
[[nodiscard]] auto openSelectedEngine() -> std::expected<SelectedEngine, EngineRefusal>;
[[nodiscard]] auto describe(EngineRefusal refusal) noexcept -> const char *;

} // namespace extendedPrecache::platform::windows
