#pragma once

#include <expected>
#include <string_view>

#include <extendedPrecache/domain/resourceTypes.hpp>

namespace extendedPrecache {

[[nodiscard]] auto validateResourcePath(std::string_view path, CatalogLimits limits) noexcept
    -> std::expected<void, ResourceError>;

} // namespace extendedPrecache
