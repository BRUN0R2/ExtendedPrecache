#include "serverModelTables.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <utility>

namespace extendedPrecache::platform::windows {
namespace {
constexpr std::uint32_t namesRva = 0x3cd940;
constexpr auto nameReferences = std::to_array<TableReference>({
    {0x2b9ec, 0},
    {0x2b9f2, 0},
    {0x2eb03, 0},
    {0x2eb94, 0},
    {0x2ec43, 0},
    {0x2ece3, 0},
    {0x330f3, 0},
    {0x33138, 0},
    {0x43468, 4},
    {0x437e3, 0},
    {0x43e66, 4},
    {0x45434, 4},
    {0x45468, 0},
});
constexpr std::uint32_t objectsRva = 0x3ce140;
constexpr auto objectReferences = std::to_array<TableReference>({
    {0x23c0c, 0}, {0x2ba26, 0}, {0x2baa8, 0}, {0x2c442, 0}, {0x2c82a, 0}, {0x2eb7b, 0},
    {0x2ecc4, 0}, {0x3315c, 0}, {0x337bd, 0}, {0x3566b, 0}, {0x356e3, 0}, {0x3587d, 0},
    {0x35925, 0}, {0x35af1, 0}, {0x35b87, 0}, {0x35c18, 0}, {0x35cec, 0}, {0x419ab, 0},
    {0x419dd, 0}, {0x43e8b, 0}, {0x452af, 4}, {0x55e8c, 0}, {0x55f44, 0}, {0x5f04f, 0},
    {0x5f60f, 0}, {0x5f993, 0}, {0x602f9, 0}, {0x61182, 4}, {0x68e50, 0},
});
constexpr std::uint32_t flagsRva = 0x3ce940;
constexpr auto flagReferences = std::to_array<TableReference>({
    {0x2eb87, 0},
    {0x2eccc, 0},
    {0x33167, 0},
    {0x43499, 0},
    {0x45426, 1},
});
constexpr auto bounds = std::to_array<CapacityBound>({
    {0x23c03, BoundKind::lastSlot},  {0x337b3, BoundKind::lastSlot},
    {0x419a2, BoundKind::lastSlot},  {0x419d4, BoundKind::lastSlot},
    {0x5f042, BoundKind::lastSlot},  {0x5f602, BoundKind::lastSlot},
    {0x5f986, BoundKind::lastSlot},  {0x602ec, BoundKind::lastSlot},
    {0x68e47, BoundKind::lastSlot},  {0x2b9fa, BoundKind::slotCount},
    {0x2eb29, BoundKind::slotCount}, {0x2ebb1, BoundKind::slotCount},
    {0x2ebe7, BoundKind::slotCount}, {0x2ec65, BoundKind::slotCount},
    {0x2ecfc, BoundKind::slotCount}, {0x2ed2c, BoundKind::slotCount},
    {0x3310e, BoundKind::slotCount}, {0x3312b, BoundKind::slotCount},
    {0x3320f, BoundKind::slotCount}, {0x434b2, BoundKind::slotCount},
    {0x43806, BoundKind::slotCount}, {0x4404b, BoundKind::pointerTableBytes},
});
// The calls that zero the engine's whole server state. Rel32 operands, so they are relative to the
// end of their own instruction.
constexpr std::uint32_t zeroingFunctionRva = 0x1000;
constexpr auto serverStateZeroings = std::array<std::uint32_t, 3>{0x13f53, 0x44ce8, 0x44d53};
constexpr std::uint32_t operandBytes = sizeof(std::uint32_t);

// These engine call sites carry no context, so their contract forces this single module-wide owner.
constinit ServerModelTables *zeroedWithServerState{};

void zeroServerState(void *state, std::int32_t value, std::size_t bytes) {
    std::memset(state, value, bytes);
    zeroedWithServerState->clear();
}

auto callOperand(std::uint32_t operandRva, const std::byte *image, const void *target) noexcept
    -> std::uint32_t {
    return std::bit_cast<std::uint32_t>(target) -
           std::bit_cast<std::uint32_t>(image + operandRva + operandBytes);
}
} // namespace

ServerModelTables::ServerModelTables(std::byte *image, ModelSlots slots)
    : image{image}, slots{slots}, names(slots.exceedNative() ? slots.count() + 1 : 0),
      objects(names.size()), flags(names.size()) {}

auto ServerModelTables::install(std::byte *image, std::uint32_t entries)
    -> std::expected<std::unique_ptr<ServerModelTables>, RewriteRefusal> {
    const auto slots = ModelSlots::atMostEntries(entries);
    auto tables = std::unique_ptr<ServerModelTables>{new ServerModelTables{image, slots}};
    if (!slots.exceedNative()) {
        return tables;
    }
    const auto placements = std::array{
        TablePlacement{{namesRva, nameReferences}, static_cast<const void *>(tables->names.data())},
        TablePlacement{{objectsRva, objectReferences},
                       static_cast<const void *>(tables->objects.data())},
        TablePlacement{{flagsRva, flagReferences}, tables->flags.data()},
    };
    auto plan = planModelRelocation(image, {placements, bounds, slots});
    const auto *const zeroing = image + zeroingFunctionRva;
    const auto *const replacement = reinterpret_cast<const void *>(&zeroServerState);
    for (const auto operandRva : serverStateZeroings) {
        plan.push_back({operandRva, callOperand(operandRva, image, zeroing),
                        callOperand(operandRva, image, replacement)});
    }
    auto rewrite = ImageRewrite::apply(image, std::move(plan));
    if (!rewrite) {
        return std::unexpected{rewrite.error()};
    }
    std::move(*rewrite).keepUntilProcessEnds();
    zeroedWithServerState = tables.get();
    return tables;
}

auto ServerModelTables::entriesInUse() const noexcept -> std::uint32_t {
    const auto *const table =
        names.empty() ? reinterpret_cast<const void *const *>(image + namesRva) : names.data();
    // The engine fills slots in order, and the reserved first slot is filled as well.
    const auto used = std::ranges::find(table, table + slots.count(), nullptr) - table;
    return used == 0 ? 0 : static_cast<std::uint32_t>(used) - 1;
}

auto ServerModelTables::movedBytes() const noexcept -> std::size_t {
    return names.size() * sizeof(names[0]) + objects.size() * sizeof(objects[0]) +
           flags.size() * sizeof(flags[0]);
}

void ServerModelTables::clear() noexcept {
    std::ranges::fill(names, nullptr);
    std::ranges::fill(objects, nullptr);
    std::ranges::fill(flags, std::uint8_t{});
}

} // namespace extendedPrecache::platform::windows
