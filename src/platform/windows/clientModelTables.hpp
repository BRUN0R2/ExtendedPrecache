#pragma once

#include "modelRelocation.hpp"

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <vector>

namespace extendedPrecache::platform::windows::client {

// Moves the selected hw.dll's model table, and only while a negotiated map needs it: the engine's
// code stays original on any other server, secure ones above all.
class ClientModelTables final {
  public:
    // The limit says how many slots the descriptors of known models can back.
    ClientModelTables(std::byte *image, std::uint32_t slotLimit) noexcept
        : image{image}, slotLimit{slotLimit} {}

    // Prepares what the next map asks for; a map within the native limit needs nothing.
    [[nodiscard]] auto reserve(std::uint32_t entries) -> std::expected<void, session::Rejection>;
    [[nodiscard]] auto reservedBytes() const noexcept -> std::size_t;
    // Call right before the engine parses a serverinfo, which starts that map from an empty state.
    [[nodiscard]] auto enterMap() noexcept -> bool;
    void leave() noexcept;

  private:
    using Table = std::unique_ptr<const void *[]>;
    struct Reservation final {
        Table table;
        std::vector<OperandRewrite> plan;
    };
    // Destroyed in reverse order, so the code stops using the table before it is freed.
    struct Active final {
        Table table;
        ImageRewrite rewrite;
    };

    std::byte *image;
    std::uint32_t slotLimit;
    std::optional<Reservation> reserved;
    std::optional<Active> active;
};

} // namespace extendedPrecache::platform::windows::client
