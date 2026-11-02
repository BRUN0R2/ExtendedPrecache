#include "binary.hpp"
#include <extendedPrecache/data/codec.hpp>

namespace extendedPrecache::data {
auto SchemaSetReceiver::accept(const Schema &schema) noexcept -> Result<void> {
    auto result = detail::protect([&] {
        detail::require(!failed && !finished, ErrorCode::wrongPhase);
        detail::require(received < schemas.size() && schemas[received] == schema,
                        ErrorCode::invalidSchema);
        ++received;
    });
    if (!result) {
        failed = true;
    }
    return result;
}
auto SchemaSetReceiver::finish(SchemaSetEnd end) noexcept -> Result<void> {
    auto result = detail::protect([&] {
        detail::require(!failed && !finished && received == schemas.size(), ErrorCode::wrongPhase);
        detail::require(end.digest == setHash, ErrorCode::digestMismatch);
        finished = true;
    });
    if (!result) {
        failed = true;
    }
    return result;
}
} // namespace extendedPrecache::data
