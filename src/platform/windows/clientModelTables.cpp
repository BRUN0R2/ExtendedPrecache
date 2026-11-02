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
    const auto placement =
        std::array{TablePlacement{layout->table, static_cast<const void *>(table.get())}};
    auto plan = planModelRelocation(image, {placement, layout->bounds, *slots});
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
