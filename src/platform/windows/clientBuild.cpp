#include "clientBuild.hpp"

#include <array>

namespace extendedPrecache::platform::windows::client {
namespace {
// The Steam game after its 25th anniversary update. The hashes identify it; the name is a label.
constexpr auto anniversaryModelReferences = std::to_array<TableReference>({
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
constexpr auto anniversaryModelBounds = std::to_array<CapacityBound>({
    {0x1a2fad, BoundKind::slotCount},
    {0x1a4547, BoundKind::slotCount},
    {0x1a4616, BoundKind::slotCount},
    {0x247247, BoundKind::slotCount},
    {0x24b994, BoundKind::slotCount},
});
constexpr auto anniversaryDescriptorReferences = std::to_array<TableReference>({
    {0x1f0d27, 0},
    {0x23e51d, 0xb0},
    {0x23e635, 0},
    {0x23e6a1, 0},
    {0x2403b6, 0},
    {0x241634, 0},
    {0x2416fa, 0x40},
    {0x241844, 0},
});
constexpr auto anniversaryCrcReferences = std::to_array<TableReference>({
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

constexpr auto builds = std::array{
    ClientBuild{
        .executable = L"hl.exe",
        .executableSha256 = "be7c061a2d36a3517c98412a06642f98985ab746e3d65c8d105d25b6ac77369c",
        .engineSha256 = "9ba9a2db5e07598fd59afa35507a98c86162e4e15b3835177b78c11842cd2295",
        .engine =
            {
                .serviceTable = 0x31a238,
                .serverInfoParser = 0x1aa2d0,
                .registerUserMessageParser = 0x1aa090,
                .engineFunctions = 0x319b08,
                .variableRegistration = 0x195b30,
                .frameEntry = 0x145efc4,
                .messageData = 0x1245fe8,
                .messageSize = 0x1245ff0,
                .readCursor = 0x124c984,
                .secureServer = 0x140ea00,
            },
        .modelTable = {.table = {0x13f6410, anniversaryModelReferences},
                       .bounds = anniversaryModelBounds},
        .modelDescriptors = {.descriptors = {0xdd5e40, anniversaryDescriptorReferences},
                             .crcRecords = {0x9860f0, anniversaryCrcReferences},
                             .cellCountBoundRva = 0x23e68e},
        // The verified game loads 49 descriptors by itself.
        .slotsWithNativeDescriptors = 900,
    },
};
} // namespace

auto knownClientBuilds() noexcept -> std::span<const ClientBuild> {
    return builds;
}

auto identifyClientBuild(const std::filesystem::path &engineFile)
    -> std::expected<const ClientBuild *, BuildRefusal> {
    for (const auto &build : builds) {
        const auto verified = verifyFileBuild(engineFile, build.engineSha256);
        if (verified) {
            return &build;
        }
        if (verified.error() == BuildRefusal::unreadable) {
            return std::unexpected{BuildRefusal::unreadable};
        }
    }
    return std::unexpected{BuildRefusal::unknownBuild};
}

} // namespace extendedPrecache::platform::windows::client
