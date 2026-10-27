#pragma once

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <extendedPrecache/domain/resourcePath.hpp>
#include <extendedPrecache/domain/resourceTypes.hpp>

namespace extendedPrecache {

struct ResourcePathHash final {
    using is_transparent = void;

    [[nodiscard]] auto operator()(std::string_view value) const noexcept -> std::size_t {
        return std::hash<std::string_view>{}(value);
    }
};

template <ResourceKind kind> class ResourceCatalog final {
  public:
    explicit ResourceCatalog(CatalogLimits limits) : catalogLimits{limits} {}

    [[nodiscard]] auto intern(std::string_view path)
        -> std::expected<ResourceIndex<kind>, ResourceError> {
        if (const auto validation = validateResourcePath(path, catalogLimits); !validation) {
            return std::unexpected{validation.error()};
        }
        if (const auto existing = find(path)) {
            return *existing;
        }
        if (frozen) {
            return std::unexpected{ResourceError::catalogFrozen};
        }
        if (entries.size() >= catalogLimits.entryCapacity()) {
            return std::unexpected{ResourceError::capacityReached};
        }

        const auto nextIndex = ResourceIndex<kind>{static_cast<std::uint32_t>(entries.size() + 1)};
        const auto result = entries.try_emplace(std::string{path}, nextIndex);
        return result.first->second;
    }

    [[nodiscard]] auto find(std::string_view path) const -> std::optional<ResourceIndex<kind>> {
        const auto entry = entries.find(path);
        if (entry == entries.end()) {
            return std::nullopt;
        }
        return entry->second;
    }

    void freeze() noexcept {
        frozen = true;
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return entries.size();
    }

  private:
    CatalogLimits catalogLimits;
    std::unordered_map<std::string, ResourceIndex<kind>, ResourcePathHash, std::equal_to<>> entries;
    bool frozen{false};
};

} // namespace extendedPrecache
