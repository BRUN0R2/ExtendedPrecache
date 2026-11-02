#pragma once

#include "modelRelocation.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <vector>

namespace extendedPrecache::platform::windows {

// Moves the selected swds.dll's model tables for the life of the process. Maps precache before any
// negotiation, so the tables must already be in place when the first one loads.
class ServerModelTables final {
  public:
    // Call before the first map; a configuration within the native limit changes nothing.
    [[nodiscard]] static auto install(std::byte *image, std::uint32_t entries)
        -> std::expected<std::unique_ptr<ServerModelTables>, RewriteRefusal>;

    ServerModelTables(const ServerModelTables &) = delete;
    auto operator=(const ServerModelTables &) -> ServerModelTables & = delete;

    [[nodiscard]] auto entriesInUse() const noexcept -> std::uint32_t;
    [[nodiscard]] auto capacityEntries() const noexcept -> std::uint32_t {
        return slots.count() - 1;
    }
    [[nodiscard]] auto movedBytes() const noexcept -> std::size_t;
    void clear() noexcept;

  private:
    ServerModelTables(std::byte *image, ModelSlots slots);

    std::byte *image;
    ModelSlots slots;
    std::vector<const void *> names;
    std::vector<const void *> objects;
    std::vector<std::uint8_t> flags;
};

} // namespace extendedPrecache::platform::windows
