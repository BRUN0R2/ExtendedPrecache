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
} // namespace

ClientModelDescriptors::ClientModelDescriptors(std::uint32_t cells,
                                               const ModelDescriptorLayout &layout)
    : layout{&layout}, cells{cells},
      descriptors{std::make_unique<std::byte[]>(cells * descriptorBytes)},
      crcRecords{std::make_unique<std::byte[]>(cells * crcRecordBytes)} {}

auto ClientModelDescriptors::reserve(std::uint32_t cells, const ModelDescriptorLayout &layout)
    -> std::unique_ptr<ClientModelDescriptors> {
    if (cells <= nativeCells) {
        return nullptr;
    }
    return std::unique_ptr<ClientModelDescriptors>{
        new ClientModelDescriptors{std::min(cells, provenCells), layout}};
}

auto ClientModelDescriptors::moveInto(std::byte *image) noexcept -> bool {
    try {
        const auto placements = std::array{
            TablePlacement{layout->descriptors, descriptors.get()},
            TablePlacement{layout->crcRecords, crcRecords.get()},
        };
        auto plan = std::vector<OperandRewrite>{};
        appendTableMoves(image, placements, plan);
        plan.push_back({layout->cellCountBoundRva, nativeCells, cells});
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
