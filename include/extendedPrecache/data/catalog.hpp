#pragma once
#include <extendedPrecache/data/schema.hpp>

#include <memory>

namespace extendedPrecache::data {
enum class ConsistencyMode : std::uint8_t { exactDigest = 1, exactBounds, withinBounds };
struct Bounds final {
    Vec3 mins;
    Vec3 maxs;
    auto operator==(const Bounds &) const -> bool = default;
};
struct Resource final {
    ResourceOrdinal ordinal;
    ResourceReference reference;
    ResourceType type{ResourceType::model};
    AssetOrdinal asset;
    std::optional<ResourceReference> parent;
    std::string name;
    auto operator==(const Resource &) const -> bool = default;
};
struct Asset final {
    AssetOrdinal ordinal;
    ByteCount fileBytes;
    Digest digest;
    bool downloadAllowed{};
    std::string path;
    auto operator==(const Asset &) const -> bool = default;
};
struct ConsistencyRule final {
    ResourceOrdinal resource;
    ConsistencyMode mode{ConsistencyMode::exactDigest};
    std::optional<Bounds> bounds;
    auto operator==(const ConsistencyRule &) const -> bool = default;
};
struct CatalogData final {
    std::array<std::uint32_t, 5> counts{};
    Digest schemaSetHash;
    std::vector<Resource> resources;
    std::vector<Asset> assets;
    std::vector<ConsistencyRule> rules;
    auto operator==(const CatalogData &) const -> bool = default;
};
struct CatalogContext final {
    const session::MapRequirements &requirements;
    const session::CapabilityLimits &selected;
    Digest schemaSetHash;
    MetadataBytes ownedBytes{maxMetadataBytes};
};
class Catalog final : public ReferenceValidator {
  public:
    [[nodiscard]] static auto create(CatalogData data, CatalogContext context,
                                     const DigestProvider &provider) noexcept -> Result<Catalog>;
    [[nodiscard]] static auto decode(std::span<const std::byte> bytes, CatalogContext context,
                                     Digest expectedCatalogHash,
                                     const DigestProvider &provider) noexcept -> Result<Catalog>;
    [[nodiscard]] auto data() const noexcept -> const CatalogData &;
    [[nodiscard]] auto bytes() const noexcept -> std::span<const std::byte>;
    [[nodiscard]] auto digest() const noexcept -> Digest;
    [[nodiscard]] auto resolve(ResourceReference reference) const noexcept -> const Resource *;
    [[nodiscard]] auto validate(ResourceReference reference, bool nullable) const noexcept
        -> Result<void> override;

  private:
    struct Storage;
    explicit Catalog(std::shared_ptr<const Storage> input) : storage{std::move(input)} {}
    std::shared_ptr<const Storage> storage;
};
struct CatalogBegin final {
    ByteCount totalBytes;
    std::uint32_t resourceCount{};
    std::uint32_t assetCount{};
    std::uint32_t consistencyCount{};
    auto operator==(const CatalogBegin &) const -> bool = default;
};
struct ChunkView final {
    ByteCount offset;
    std::span<const std::byte> bytes;
};
[[nodiscard]] auto encodeCatalogBegin(CatalogBegin begin) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeCatalogBegin(std::span<const std::byte> bytes) noexcept
    -> Result<CatalogBegin>;
[[nodiscard]] auto encodeChunk(ChunkView chunk, ByteCount maxPayload, RecordBudget budget) noexcept
    -> Result<Bytes>;
[[nodiscard]] auto decodeChunk(std::span<const std::byte> bytes, ByteCount maxPayload,
                               RecordBudget budget) noexcept -> Result<ChunkView>;

class ChunkAssembler final {
  public:
    [[nodiscard]] static auto create(ByteCount total, ByteCount maximum,
                                     MetadataBytes memory) noexcept -> Result<ChunkAssembler>;
    [[nodiscard]] auto append(ChunkView chunk) noexcept -> Result<void>;
    [[nodiscard]] auto finish(Digest expected, const DigestProvider &provider) const noexcept
        -> Result<std::span<const std::byte>>;

  private:
    explicit ChunkAssembler(ByteCount total) : totalBytes{total} {}
    ByteCount totalBytes;
    Bytes buffer;
    bool failed{};
};
} // namespace extendedPrecache::data
