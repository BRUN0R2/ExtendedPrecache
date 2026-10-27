#include <extendedPrecache/domain/resourcePath.hpp>

namespace extendedPrecache {

auto validateResourcePath(std::string_view path, CatalogLimits limits) noexcept
    -> std::expected<void, ResourceError> {
    if (path.empty()) {
        return std::unexpected{ResourceError::invalidPath};
    }
    if (path.size() > limits.maxPathBytes()) {
        return std::unexpected{ResourceError::pathTooLong};
    }

    constexpr auto firstPrintable = 0x20U;
    constexpr auto deleteCharacter = 0x7FU;
    for (const char character : path) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < firstPrintable || byte == deleteCharacter || character == '\\' ||
            character == ':') {
            return std::unexpected{ResourceError::invalidPath};
        }
    }

    constexpr auto separator = '/';
    auto remaining = path;
    while (true) {
        const auto separatorPosition = remaining.find(separator);
        const auto segment = remaining.substr(0, separatorPosition);
        if (segment.empty() || segment == "." || segment == "..") {
            return std::unexpected{ResourceError::invalidPath};
        }
        if (separatorPosition == std::string_view::npos) {
            return {};
        }
        remaining.remove_prefix(separatorPosition + 1);
    }
}

} // namespace extendedPrecache
