#pragma once

#include "imageRewrite.hpp"

#include <extendedPrecache/session/negotiation.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace extendedPrecache::platform::windows {

// Slot zero of a model table is reserved, so the contract counts one entry fewer than the slots.
inline constexpr std::uint32_t nativeModelSlots = 512;
// The largest table verified end to end. Both engines keep 1,024 descriptors of known models, which
// this relocation does not move; on the client they also hold sprites, 49 in the verified game.
inline constexpr std::uint32_t provenModelSlots = 900;

class ModelSlots final {
  public:
    // Empty when the entries exceed what has been verified.
    [[nodiscard]] static constexpr auto forEntries(std::uint32_t entries) noexcept
        -> std::optional<ModelSlots> {
        if (entries >= provenModelSlots) {
            return std::nullopt;
        }
        return ModelSlots{entries + 1};
    }
    // A table the engine already fills in its code: it never shrinks, and never exceeds the proof.
    [[nodiscard]] static constexpr auto atMostEntries(std::uint32_t entries) noexcept
        -> ModelSlots {
        return ModelSlots{std::clamp(entries, nativeModelSlots - 1, provenModelSlots - 1) + 1};
    }
    [[nodiscard]] static constexpr auto native() noexcept -> ModelSlots {
        return ModelSlots{nativeModelSlots};
    }
    [[nodiscard]] constexpr auto count() const noexcept -> std::uint32_t {
        return slots;
    }
    [[nodiscard]] constexpr auto exceedNative() const noexcept -> bool {
        return slots > nativeModelSlots;
    }

  private:
    explicit constexpr ModelSlots(std::uint32_t slots) noexcept : slots{slots} {}
    std::uint32_t slots;
};

// How an immediate in the engine's code expresses the size of a model table.
enum class BoundKind : std::uint8_t { slotCount, lastSlot, pointerTableBytes };
struct CapacityBound final {
    std::uint32_t operandRva;
    BoundKind kind;
};
struct TableReference final {
    std::uint32_t operandRva;
    std::uint32_t offset;
};
struct EngineTable final {
    std::uint32_t rva;
    std::span<const TableReference> references;
};
struct TablePlacement final {
    EngineTable table;
    const void *storage;
};
struct ModelRelocation final {
    std::span<const TablePlacement> placements;
    std::span<const CapacityBound> bounds;
    ModelSlots slots;
};

// The rewrites that move each table to its storage and size every bound for the slots.
[[nodiscard]] auto planModelRelocation(const std::byte *image, const ModelRelocation &relocation)
    -> std::vector<OperandRewrite>;

[[nodiscard]] auto requiredModelEntries(const session::SessionAgreement &agreement) noexcept
    -> std::uint32_t;

// Lets a negotiation account for the durable tables its map uses, and notice if the map moved on.
[[nodiscard]] auto leaseModelTables(const session::Configuration &configuration,
                                    const session::SessionAgreement &agreement, std::size_t bytes)
    -> std::unique_ptr<session::PreparedSession>;

} // namespace extendedPrecache::platform::windows
