#include "clientModelTables.hpp"

#include <array>
#include <utility>

namespace extendedPrecache::platform::windows::client {
namespace {
using session::CapabilityField;
using session::RejectCode;
using session::Rejection;

// The resource list carries 12-bit indices and the native parser stores them unchecked, so a table
// with every cell they can name cannot be left by anything on the wire.
constexpr std::size_t wireIndexCells = 4096;

constexpr std::uint32_t modelTableRva = 0x13f6410;
constexpr auto modelTableReferences = std::to_array<TableReference>({
    {0x196128, 4},
    {0x196146, 4},
    {0x19f6d2, 0},
    {0x1a2fba, 0},
    {0x1a3029, 0},
    {0x1a3046, 0},
    {0x1a4670, 0},
    {0x1a467a, 0},
    {0x1a85f6, 4},
    {0x1a863d, 4},
    {0x1a86d4, 0},
    {0x1a86de, 0},
    {0x1c77f3, 0},
    {0x1d0973, 0},
    {0x247033, 0},
    {0x24b804, 0},
});
constexpr auto modelTableBounds = std::to_array<CapacityBound>({
    {0x1a2fad, BoundKind::slotCount},
    {0x1a4547, BoundKind::slotCount},
    {0x1a4616, BoundKind::slotCount},
    {0x247247, BoundKind::slotCount},
    {0x24b994, BoundKind::slotCount},
});
} // namespace

auto ClientModelTables::reserve(std::uint32_t entries) -> std::expected<void, Rejection> {
    reserved.reset();
    const auto slots = ModelSlots::forEntries(entries, slotLimit);
    if (!slots) {
        return std::unexpected{Rejection{.code = RejectCode::capacityInsufficient,
                                         .fieldId = CapabilityField::modelEntries}};
    }
    if (!slots->exceedNative()) {
        return {};
    }
    auto table = std::make_unique<const void *[]>(wireIndexCells);
    const auto placement = std::array{TablePlacement{{modelTableRva, modelTableReferences},
                                                     static_cast<const void *>(table.get())}};
    auto plan = planModelRelocation(image, {placement, modelTableBounds, *slots});
    reserved.emplace(std::move(table), std::move(plan));
    return {};
}

auto ClientModelTables::reservedBytes() const noexcept -> std::size_t {
    return reserved ? wireIndexCells * sizeof(const void *) : 0;
}

auto ClientModelTables::enterMap() noexcept -> bool {
    active.reset();
    if (!reserved) {
        return true;
    }
    auto reservation = std::move(*reserved);
    reserved.reset();
    auto rewrite = ImageRewrite::apply(image, std::move(reservation.plan));
    if (!rewrite) {
        return false;
    }
    active.emplace(std::move(reservation.table), std::move(*rewrite));
    return true;
}

void ClientModelTables::leave() noexcept {
    active.reset();
    reserved.reset();
}

} // namespace extendedPrecache::platform::windows::client
