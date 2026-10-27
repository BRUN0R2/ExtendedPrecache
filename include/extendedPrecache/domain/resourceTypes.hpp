#pragma once

#include <cstdint>
#include <expected>
#include <limits>

namespace extendedPrecache {

enum class ResourceKind : std::uint8_t { model, sound, generic, event, decal };

enum class ResourceError : std::uint8_t {
    invalidCapacity,
    invalidPathLimit,
    invalidPath,
    pathTooLong,
    capacityReached,
    catalogFrozen,
    invalidBitWidth
};

struct ResourceCapacity final {
    std::uint32_t value;
};

struct ResourcePathByteLimit final {
    std::uint32_t value;
};

class CatalogLimits final {
  public:
    [[nodiscard]] static constexpr auto create(ResourceCapacity entryCapacity,
                                               ResourcePathByteLimit maxPathBytes) noexcept
        -> std::expected<CatalogLimits, ResourceError> {
        if (entryCapacity.value == 0) {
            return std::unexpected{ResourceError::invalidCapacity};
        }
        if (maxPathBytes.value == 0) {
            return std::unexpected{ResourceError::invalidPathLimit};
        }
        return CatalogLimits{entryCapacity, maxPathBytes};
    }

    [[nodiscard]] constexpr auto entryCapacity() const noexcept -> std::uint32_t {
        return entryCapacityValue;
    }

    [[nodiscard]] constexpr auto maxPathBytes() const noexcept -> std::uint32_t {
        return maxPathBytesValue;
    }

  private:
    constexpr CatalogLimits(ResourceCapacity entryCapacity,
                            ResourcePathByteLimit maxPathBytes) noexcept
        : entryCapacityValue{entryCapacity.value}, maxPathBytesValue{maxPathBytes.value} {}

    std::uint32_t entryCapacityValue;
    std::uint32_t maxPathBytesValue;
};

template <ResourceKind kind> class ResourceCatalog;

template <ResourceKind kind> class ResourceIndex final {
    static_assert(kind >= ResourceKind::model && kind <= ResourceKind::decal);

  public:
    [[nodiscard]] constexpr auto value() const noexcept -> std::uint32_t {
        return rawValue;
    }

    [[nodiscard]] constexpr auto operator==(const ResourceIndex &) const noexcept -> bool = default;

  private:
    friend class ResourceCatalog<kind>;

    explicit constexpr ResourceIndex(std::uint32_t value) noexcept : rawValue{value} {}

    std::uint32_t rawValue;
};

class IndexEncoding final {
  public:
    [[nodiscard]] static constexpr auto create(std::uint32_t bitCount) noexcept
        -> std::expected<IndexEncoding, ResourceError> {
        if (bitCount == 0 || bitCount > storageBits) {
            return std::unexpected{ResourceError::invalidBitWidth};
        }
        return IndexEncoding{static_cast<std::uint8_t>(bitCount)};
    }

    [[nodiscard]] constexpr auto maxValue() const noexcept -> std::uint32_t {
        return std::numeric_limits<std::uint32_t>::max() >> (storageBits - bitCountValue);
    }

    [[nodiscard]] constexpr auto canRepresent(std::uint32_t value) const noexcept -> bool {
        return value <= maxValue();
    }

    template <ResourceKind kind>
    [[nodiscard]] constexpr auto canRepresent(ResourceIndex<kind> index) const noexcept -> bool {
        return canRepresent(index.value());
    }

  private:
    static constexpr std::uint32_t storageBits = std::numeric_limits<std::uint32_t>::digits;

    explicit constexpr IndexEncoding(std::uint8_t bitCount) noexcept : bitCountValue{bitCount} {}

    std::uint8_t bitCountValue;
};

} // namespace extendedPrecache
