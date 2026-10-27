#include <extendedPrecache/data/types.hpp>

namespace extendedPrecache::data {
auto sha256(std::span<const std::byte> input, const DigestProvider &provider) noexcept
    -> Result<Digest> {
    const auto result = provider.sha256(input);
    if (!result) {
        return std::unexpected{Failure{ErrorCode::storageUnavailable}};
    }
    return *result;
}
} // namespace extendedPrecache::data
