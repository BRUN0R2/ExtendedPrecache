#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace extendedPrecache::session {

template <typename Tag, typename Representation> struct Quantity final {
    Representation value{};
    auto operator<=>(const Quantity &) const = default;
};

using ByteCount = Quantity<struct ByteCountTag, std::uint32_t>;
using MetadataBytes = Quantity<struct MetadataBytesTag, std::uint64_t>;
using DurationMs = Quantity<struct DurationMsTag, std::uint32_t>;
using MonotonicTime = Quantity<struct MonotonicTimeTag, std::uint64_t>;
using FrameSequence = Quantity<struct FrameSequenceTag, std::uint32_t>;
using MapEpoch = Quantity<struct MapEpochTag, std::uint64_t>;
using LocalGeneration = Quantity<struct LocalGenerationTag, std::uint64_t>;
using SpawnCount = Quantity<struct SpawnCountTag, std::uint32_t>;
using DataContractId = Quantity<struct DataContractIdTag, std::uint32_t>;
using ModelCacheEntries = Quantity<struct ModelCacheEntriesTag, std::uint32_t>;
using LocalModelReserve = Quantity<struct LocalModelReserveTag, std::uint32_t>;
using SoundDescriptors = Quantity<struct SoundDescriptorsTag, std::uint32_t>;
using LocalSoundReserve = Quantity<struct LocalSoundReserveTag, std::uint32_t>;
using HudSpriteEntries = Quantity<struct HudSpriteEntriesTag, std::uint32_t>;

template <typename Tag, std::size_t size> struct IdentityBytes final {
    std::array<std::byte, size> bytes{};
    [[nodiscard]] constexpr auto isZero() const noexcept -> bool {
        return std::ranges::all_of(bytes, [](std::byte value) { return value == std::byte{}; });
    }
    auto operator<=>(const IdentityBytes &) const = default;
};

using Digest = IdentityBytes<struct DigestTag, 32>;
using SessionNonce = IdentityBytes<struct SessionNonceTag, 16>;
using ClientNonce = IdentityBytes<struct ClientNonceTag, 16>;
using ConnectionToken = IdentityBytes<struct ConnectionTokenTag, 16>;
using PeerToken = IdentityBytes<struct PeerTokenTag, 16>;

enum class EndpointRole : std::uint8_t { server, client };
enum class ConnectionPolicy : std::uint8_t {
    legacyOnly = 0,
    preferExtended = 1,
    requireExtended = 2
};
enum class Direction : std::uint8_t { server, client };

enum class CapabilityField : std::uint8_t {
    modelEntries = 1,
    soundEntries,
    genericEntries,
    eventEntries,
    decalEntries,
    resourceEntries,
    consistencyEntries,
    manifestBytes,
    chunkBytes,
    inFlightBytes,
    fileBytes,
    reliableMessageBytes
};

inline constexpr std::size_t capabilityCount = 12;
inline constexpr auto capabilityFields =
    std::array{CapabilityField::modelEntries,       CapabilityField::soundEntries,
               CapabilityField::genericEntries,     CapabilityField::eventEntries,
               CapabilityField::decalEntries,       CapabilityField::resourceEntries,
               CapabilityField::consistencyEntries, CapabilityField::manifestBytes,
               CapabilityField::chunkBytes,         CapabilityField::inFlightBytes,
               CapabilityField::fileBytes,          CapabilityField::reliableMessageBytes};
inline constexpr auto capabilityNames = std::array<std::string_view, capabilityCount>{
    "modelEntries", "soundEntries",    "genericEntries",     "eventEntries",
    "decalEntries", "resourceEntries", "consistencyEntries", "manifestBytes",
    "chunkBytes",   "inFlightBytes",   "fileBytes",          "reliableMessageBytes"};

template <CapabilityField field> struct CapabilityTag;
template <CapabilityField field>
using CapabilityValue = Quantity<CapabilityTag<field>, std::uint32_t>;

enum class RejectCode : std::uint8_t {
    malformedFrame = 1,
    unsupportedVersion,
    unexpectedMessage,
    unexpectedSequence,
    unsupportedContract,
    capacityInsufficient,
    invalidConfiguration,
    storageUnavailable,
    transcriptMismatch,
    deadlineExceeded,
    mapChanged,
    legacyIncompatible,
    transportUnavailable,
    budgetExceeded
};

struct Rejection final {
    RejectCode code{RejectCode::malformedFrame};
    std::uint8_t fieldId{};
    std::uint32_t offered{};
    std::uint32_t required{};
    auto operator==(const Rejection &) const -> bool = default;
};

inline constexpr std::uint32_t maxConfigurationBytes = 65536;
inline constexpr std::uint32_t minTimeoutMs = 1000;
inline constexpr std::uint32_t maxTimeoutMs = 60000;
inline constexpr std::uint32_t normalTranscriptBytes = 1544;
inline constexpr std::uint32_t maxTranscriptBytes = 4096;
inline constexpr std::uint32_t maxFramesPerDirection = 32;
inline constexpr std::uint32_t legacyProtocolVersion = 48;

[[nodiscard]] constexpr auto validField(CapabilityField field) noexcept -> bool {
    return field >= CapabilityField::modelEntries && field <= CapabilityField::reliableMessageBytes;
}
[[nodiscard]] constexpr auto fieldIndex(CapabilityField field) noexcept -> std::size_t {
    return static_cast<std::size_t>(field) - 1;
}
[[nodiscard]] auto parseDigest(std::string_view hex) noexcept -> std::expected<Digest, Rejection>;
[[nodiscard]] auto encodeHex(std::span<const std::byte> bytes) -> std::string;
[[nodiscard]] auto decodeHex(std::string_view hex)
    -> std::expected<std::vector<std::byte>, Rejection>;

} // namespace extendedPrecache::session
