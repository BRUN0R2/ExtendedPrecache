#include "clientModelDescriptors.hpp"

#include "modelRelocation.hpp"

#include <array>
#include <new>
#include <utility>
#include <vector>

namespace extendedPrecache::platform::windows::client {
namespace {
constexpr std::uint32_t nativeCells = 1024;
constexpr std::uint32_t provenCells = 2048;
constexpr std::size_t descriptorBytes = session::modelDescriptorBytes;
constexpr std::size_t crcRecordBytes = 12;

constexpr std::uint32_t descriptorsRva = 0xdd5e40;
constexpr auto descriptorReferences = std::to_array<TableReference>({
    {0x1f0d27, 0},
    {0x23e51d, 0xb0},
    {0x23e635, 0},
    {0x23e6a1, 0},
    {0x2403b6, 0},
    {0x241634, 0},
    {0x2416fa, 0x40},
    {0x241844, 0},
});
constexpr std::uint32_t crcRecordsRva = 0x9860f0;
constexpr auto crcReferences = std::to_array<TableReference>({
    {0x23e6d5, 8},
    {0x23e6e0, 4},
    {0x23e6eb, 0},
    {0x2403cc, 0},
    {0x24041b, 4},
    {0x240429, 4},
    {0x240434, 8},
    {0x2404d6, 8},
    {0x241652, 0},
    {0x24185f, 4},
    {0x24186c, 8},
});
constexpr std::uint32_t cellCountBoundRva = 0x23e68e;
} // namespace

ClientModelDescriptors::ClientModelDescriptors(std::uint32_t cells)
    : cells{cells}, descriptors{std::make_unique<std::byte[]>(cells * descriptorBytes)},
      crcRecords{std::make_unique<std::byte[]>(cells * crcRecordBytes)} {}

auto ClientModelDescriptors::reserve(std::uint32_t cells)
    -> std::unique_ptr<ClientModelDescriptors> {
    if (cells <= nativeCells) {
        return nullptr;
    }
    return std::unique_ptr<ClientModelDescriptors>{
        new ClientModelDescriptors{std::min(cells, provenCells)}};
}

auto ClientModelDescriptors::moveInto(std::byte *image) noexcept -> bool {
    try {
        const auto placements = std::array{
            TablePlacement{{descriptorsRva, descriptorReferences}, descriptors.get()},
            TablePlacement{{crcRecordsRva, crcReferences}, crcRecords.get()},
        };
        auto plan = std::vector<OperandRewrite>{};
        appendTableMoves(image, placements, plan);
        plan.push_back({cellCountBoundRva, nativeCells, cells});
        auto rewrite = ImageRewrite::apply(image, std::move(plan));
        if (!rewrite) {
            return false;
        }
        std::move(*rewrite).keepUntilProcessEnds();
        return true;
    } catch (const std::bad_alloc &) {
        return false;
    }
}

} // namespace extendedPrecache::platform::windows::client
