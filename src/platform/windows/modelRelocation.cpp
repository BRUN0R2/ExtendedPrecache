#include "modelRelocation.hpp"

#include <bit>

namespace extendedPrecache::platform::windows {
namespace {
using session::CapabilityField;
using session::MetadataBytes;
using session::SessionAgreement;
using session::SpawnCount;

// Operands hold absolute 32-bit addresses, which only an x86 process can compare or replace.
static_assert(sizeof(void *) == sizeof(std::uint32_t));

auto addressOf(const void *pointer) noexcept -> std::uint32_t {
    return std::bit_cast<std::uint32_t>(pointer);
}

constexpr auto boundValue(BoundKind kind, ModelSlots slots) noexcept -> std::uint32_t {
    switch (kind) {
    case BoundKind::slotCount:
        return slots.count();
    case BoundKind::lastSlot:
        return slots.count() - 1;
    case BoundKind::pointerTableBytes:
        return slots.count() * sizeof(std::uint32_t);
    }
    return 0;
}

class ModelLease final : public session::PreparedSession {
  public:
    ModelLease(MetadataBytes peak, SpawnCount spawn, std::uint32_t entries) noexcept
        : peak{peak}, spawn{spawn}, entries{entries} {}

    [[nodiscard]] auto peakMetadataBytes() const noexcept -> MetadataBytes override {
        return peak;
    }
    [[nodiscard]] auto stillValid(const SessionAgreement &agreement) const noexcept
        -> bool override {
        return agreement.map.spawn == spawn && requiredModelEntries(agreement) == entries;
    }

  private:
    MetadataBytes peak;
    SpawnCount spawn;
    std::uint32_t entries;
};
} // namespace

void appendTableMoves(const std::byte *image, std::span<const TablePlacement> placements,
                      std::vector<OperandRewrite> &rewrites) {
    for (const auto &[table, storage] : placements) {
        const auto from = addressOf(image + table.rva);
        const auto to = addressOf(storage);
        for (const auto &[operandRva, offset] : table.references) {
            rewrites.push_back({operandRva, from + offset, to + offset});
        }
    }
}

auto planModelRelocation(const std::byte *image, const ModelRelocation &relocation)
    -> std::vector<OperandRewrite> {
    auto rewrites = std::vector<OperandRewrite>{};
    appendTableMoves(image, relocation.placements, rewrites);
    for (const auto &[operandRva, kind] : relocation.bounds) {
        rewrites.push_back({operandRva, boundValue(kind, ModelSlots::native()),
                            boundValue(kind, relocation.slots)});
    }
    return rewrites;
}

auto requiredModelEntries(const SessionAgreement &agreement) noexcept -> std::uint32_t {
    return agreement.map.requirements.vector().get<CapabilityField::modelEntries>().value;
}

auto leaseModelTables(const session::Configuration &configuration,
                      const SessionAgreement &agreement, std::size_t bytes)
    -> std::unique_ptr<session::PreparedSession> {
    const auto peak =
        MetadataBytes{session::minimumSessionMetadataBytes(configuration).value + bytes};
    return std::make_unique<ModelLease>(peak, agreement.map.spawn, requiredModelEntries(agreement));
}

} // namespace extendedPrecache::platform::windows
