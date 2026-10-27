#pragma once
#include <cstdint>

namespace extendedPrecache::data {
// Fixed wire widths from data contract 2; independent of C++ object layouts.
inline constexpr auto envelopeBytes = std::uint32_t{40};
inline constexpr auto catalogHeaderBytes = std::uint32_t{72};
inline constexpr auto resourceHeaderBytes = std::uint32_t{28};
inline constexpr auto assetHeaderBytes = std::uint32_t{44};
inline constexpr auto maxChunkOverheadBytes = std::uint32_t{48};
inline constexpr auto schemaHeaderBytes = std::uint32_t{8};
inline constexpr auto fieldHeaderBytes = std::uint32_t{12};
inline constexpr auto frameBeginBytes = std::uint32_t{24};
inline constexpr auto frameObjectHeaderBytes = std::uint32_t{16};
inline constexpr auto frameEndBytes = std::uint32_t{40};
inline constexpr auto fullObjectHeaderBytes = std::uint32_t{12};
inline constexpr auto consistencyPageHeaderBytes = std::uint32_t{8};
inline constexpr auto consistencyDigestEntryBytes = std::uint32_t{40};
inline constexpr auto consistencyBoundsEntryBytes = std::uint32_t{64};
inline constexpr auto soundBytes = std::uint32_t{20};
inline constexpr auto eventHeaderBytes = std::uint32_t{16};
inline constexpr auto effectHeaderBytes = std::uint32_t{8};
inline constexpr auto modMessageHeaderBytes = std::uint32_t{8};
inline constexpr auto fatalBytes = std::uint32_t{16};
inline constexpr auto demoHeaderBytes = std::uint32_t{256};
inline constexpr auto demoBlockHeaderBytes = std::uint32_t{56};
inline constexpr auto demoPacketHeaderBytes = std::uint32_t{12};
inline constexpr auto demoLocalHeaderBytes = std::uint32_t{12};
inline constexpr auto demoEndBytes = std::uint32_t{48};
inline constexpr auto catalogBeginBytes = std::uint32_t{16};
inline constexpr auto digestBytes = std::uint32_t{32};
inline constexpr auto frameAckBytes = std::uint32_t{4} + digestBytes;
inline constexpr auto assetsReadyBytes = 3 * digestBytes;
inline constexpr auto gameStartBytes = frameAckBytes + digestBytes;
inline constexpr auto consistencyEndBytes = std::uint32_t{4} + digestBytes;
inline constexpr auto decalNameHeaderBytes = std::uint32_t{6};
inline constexpr auto demoArtifactHeaderBytes = std::uint32_t{8};
inline constexpr auto demoBootstrapHeaderBytes = std::uint32_t{12};
} // namespace extendedPrecache::data
