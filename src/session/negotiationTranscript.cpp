#include <extendedPrecache/session/negotiation.hpp>

#include "negotiationOrder.hpp"

namespace extendedPrecache::session {
auto NegotiationSession::hash(MessageKind kind) const noexcept -> std::expected<Digest, Rejection> {
    constexpr char offerLabel[] = "ExtendedPrecache/offer/1";
    constexpr char clientLabel[] = "ExtendedPrecache/client/1";
    constexpr char selectionLabel[] = "ExtendedPrecache/selection/1";
    std::array<std::byte, transcriptHashWorkspaceBytes> bytes{};
    std::size_t used{};
    const auto append = [&bytes, &used](std::span<const std::byte> source) {
        std::ranges::copy(source, bytes.begin() + static_cast<std::ptrdiff_t>(used));
        used += source.size();
    };
    Direction direction;
    std::uint32_t first{};
    std::uint32_t count{};
    if (kind == MessageKind::offerEnd) {
        append(std::as_bytes(std::span{offerLabel}));
        direction = Direction::server;
        count = detail::offerHashFrameCount;
    } else if (kind == MessageKind::clientCapabilitiesEnd) {
        append(std::as_bytes(std::span{clientLabel}));
        append(offerHash.bytes);
        direction = Direction::client;
        count = detail::clientHashFrameCount;
    } else if (kind == MessageKind::selectionEnd) {
        append(std::as_bytes(std::span{selectionLabel}));
        append(offerHash.bytes);
        append(clientHash.bytes);
        direction = Direction::server;
        first = detail::firstSelectionSequence;
        count = pageCount;
    } else {
        return std::unexpected{Rejection{RejectCode::unexpectedMessage}};
    }
    const auto index = detail::directionIndex(direction);
    if (counts[index] < first + count) {
        return std::unexpected{Rejection{RejectCode::unexpectedSequence}};
    }
    for (auto sequence = first; sequence < first + count; ++sequence) {
        append(transcript[index][sequence].bytes());
    }
    const auto result = digests->sha256(std::span{bytes}.first(used));
    if (!result) {
        return std::unexpected{Rejection{RejectCode::transportUnavailable}};
    }
    return *result;
}
} // namespace extendedPrecache::session
