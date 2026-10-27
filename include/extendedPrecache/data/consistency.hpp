#pragma once
#include <extendedPrecache/data/catalog.hpp>

namespace extendedPrecache::data {
struct ConsistencyEntry final {
    ResourceOrdinal resource;
    ConsistencyMode mode{ConsistencyMode::exactDigest};
    Digest actualAsset;
    std::optional<Bounds> bounds;
    auto operator==(const ConsistencyEntry &) const -> bool = default;
};
struct ConsistencyPage final {
    std::uint32_t ruleOffset{};
    std::vector<ConsistencyEntry> entries;
    auto operator==(const ConsistencyPage &) const -> bool = default;
};
struct ConsistencyEnd final {
    std::uint32_t ruleCount{};
    Digest reportHash;
    auto operator==(const ConsistencyEnd &) const -> bool = default;
};
[[nodiscard]] auto fitConsistencyEntries(std::span<const ConsistencyEntry> entries,
                                         RecordBudget budget) noexcept -> Result<std::size_t>;
[[nodiscard]] auto encodeConsistencyPage(const ConsistencyPage &page,
                                         RecordBudget budget = {}) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeConsistencyPage(std::span<const std::byte> bytes,
                                         RecordBudget budget = {}) noexcept
    -> Result<ConsistencyPage>;
[[nodiscard]] auto encodeConsistencyEnd(ConsistencyEnd end) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeConsistencyEnd(std::span<const std::byte> bytes) noexcept
    -> Result<ConsistencyEnd>;
[[nodiscard]] auto consistencyHash(std::span<const ConsistencyEntry> entries, Digest catalogHash,
                                   const DigestProvider &provider, MetadataBytes memory) noexcept
    -> Result<Digest>;
class ConsistencyReport final {
  public:
    ConsistencyReport(Catalog catalog, MetadataBytes memory)
        : catalogValue{std::move(catalog)}, memoryLimit{memory} {}
    [[nodiscard]] auto append(const ConsistencyPage &page) noexcept -> Result<void>;
    [[nodiscard]] auto finish(ConsistencyEnd end, const DigestProvider &provider) noexcept
        -> Result<Digest>;

  private:
    Catalog catalogValue;
    MetadataBytes memoryLimit;
    std::vector<ConsistencyEntry> entries;
    bool terminal{};
};
} // namespace extendedPrecache::data
