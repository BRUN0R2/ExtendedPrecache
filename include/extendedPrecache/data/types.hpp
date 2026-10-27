#pragma once

#include <extendedPrecache/data/wireLayout.hpp>
#include <extendedPrecache/session/capabilities.hpp>
#include <extendedPrecache/session/crypto.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <variant>
#include <vector>

namespace extendedPrecache::data {

using session::ByteCount;
using session::Digest;
using session::DigestProvider;
using session::MapEpoch;
using session::MetadataBytes;
using session::SessionNonce;
using Bytes = std::vector<std::byte>;
template <typename Tag, typename T> using Id = session::Quantity<Tag, T>;
using DomainId = Id<struct DomainTag, std::uint32_t>;
using ResourceOrdinal = Id<struct ResourceOrdinalTag, std::uint32_t>;
using AssetOrdinal = Id<struct AssetOrdinalTag, std::uint32_t>;
using SchemaId = Id<struct SchemaTag, std::uint16_t>;
using FieldId = Id<struct FieldTag, std::uint16_t>;
using ModuleId = Id<struct ModuleTag, std::uint16_t>;
using ModMessageId = Id<struct ModMessageTag, std::uint16_t>;
using EffectId = Id<struct EffectTag, std::uint16_t>;
using StateFrameId = Id<struct StateFrameTag, std::uint32_t>;
using ObjectId = Id<struct ObjectTag, std::uint32_t>;
using EntityId = Id<struct EntityTag, std::uint16_t>;
using PackedEntity = Id<struct PackedEntityTag, std::uint16_t>;
using Coord = Id<struct CoordTag, std::int16_t>;
using Coords = std::array<Coord, 3>;
using Vec3 = std::array<float, 3>;
using TimeMicros = Id<struct TimeMicrosTag, std::uint64_t>;
using FileOffset = Id<struct FileOffsetTag, std::uint64_t>;

inline constexpr auto maxMessageBytes = std::uint32_t{65536};
inline constexpr auto chunkOverheadBytes = maxChunkOverheadBytes;
inline constexpr auto maxSequence = std::uint32_t{0xfffffffe};
inline constexpr auto noSubject = std::uint32_t{0xffffffff};
inline constexpr auto maxMetadataBytes = std::uint64_t{2147483647};
inline constexpr auto maxFileBytes = std::uint32_t{16777215};
inline constexpr auto maxEntityEntries = std::uint32_t{2048};
inline constexpr auto maxClientEntries = std::uint32_t{32};
inline constexpr auto maxNameBytes = std::uint32_t{63};
inline constexpr auto maxDecalNameBytes = std::uint32_t{15};
inline constexpr auto nativeUserMessageBytes = std::uint32_t{192};

enum class ErrorCode : std::uint8_t {
    malformed = 1,
    unsupportedContract,
    byteBudget,
    staleContext,
    wrongPhase,
    digestMismatch,
    capacity,
    assetFailure,
    consistencyFailure,
    deadline,
    invalidSchema,
    unsupportedModule,
    sequence,
    transportUnavailable,
    storageUnavailable
};
struct Failure final {
    ErrorCode code{ErrorCode::malformed};
    std::uint32_t subjectSequence{noSubject};
    std::uint32_t fieldId{noSubject};
    std::uint64_t offset{};
    auto operator==(const Failure &) const -> bool = default;
};
template <typename T> using Result = std::expected<T, Failure>;

enum class ResourceKind : std::uint8_t { model = 1, sound, generic, event, decal };
enum class ResourceType : std::uint8_t {
    model = 1,
    sprite,
    world,
    inlineModel,
    sound,
    generic,
    event,
    decal
};
enum class Direction : std::uint8_t { serverToClient, clientToServer };
enum class Delivery : std::uint8_t { unreliable, reliable };
enum class Phase : std::uint8_t { negotiation, bootstrap, loading, gameplay };

struct ResourceReference final {
    ResourceKind kind{ResourceKind::model};
    DomainId id{};
    auto operator==(const ResourceReference &) const -> bool = default;
};
struct ByteString final {
    Bytes bytes;
    auto operator==(const ByteString &) const -> bool = default;
};
using Atom = std::variant<std::uint8_t, std::int8_t, std::uint16_t, std::int16_t, std::uint32_t,
                          std::int32_t, float, ByteString, ResourceReference, EntityId,
                          PackedEntity, Coord, Coords, Vec3>;
struct FieldValue final {
    std::vector<Atom> elements;
    auto operator==(const FieldValue &) const -> bool = default;
};
using ValueSet = std::vector<FieldValue>;

struct ObjectLimits final {
    std::uint32_t entityEntries{1};
    std::uint32_t clientEntries{1};
    std::uint32_t weaponEntries{};
    std::uint32_t staticEntries{};
    auto operator==(const ObjectLimits &) const -> bool = default;
};
struct RecordBudget final {
    ByteCount messageBytes{maxMessageBytes};
    MetadataBytes ownedBytes{maxMetadataBytes};
};

[[nodiscard]] constexpr auto validKind(ResourceKind kind) noexcept -> bool {
    return kind >= ResourceKind::model && kind <= ResourceKind::decal;
}
[[nodiscard]] constexpr auto kindIndex(ResourceKind kind) noexcept -> std::size_t {
    return static_cast<std::size_t>(kind) - 1;
}
[[nodiscard]] auto sha256(std::span<const std::byte> input, const DigestProvider &provider) noexcept
    -> Result<Digest>;

} // namespace extendedPrecache::data
