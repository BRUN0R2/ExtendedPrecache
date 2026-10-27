#pragma once

#include <extendedPrecache/session/frameCodec.hpp>

namespace extendedPrecache::session::detail {
struct ProtocolStep final {
    Direction direction;
    MessageKind kind;
    PageOrdinal page{};
};
inline constexpr auto order =
    std::array{ProtocolStep{Direction::server, MessageKind::serverOffer},
               ProtocolStep{Direction::server, MessageKind::applicationContract},
               ProtocolStep{Direction::server, MessageKind::dataContract},
               ProtocolStep{Direction::server, MessageKind::catalogIdentity},
               ProtocolStep{Direction::server, MessageKind::requirementsPage, {0}},
               ProtocolStep{Direction::server, MessageKind::requirementsPage, {1}},
               ProtocolStep{Direction::server, MessageKind::requirementsPage, {2}},
               ProtocolStep{Direction::server, MessageKind::serverLimitsPage, {0}},
               ProtocolStep{Direction::server, MessageKind::serverLimitsPage, {1}},
               ProtocolStep{Direction::server, MessageKind::serverLimitsPage, {2}},
               ProtocolStep{Direction::server, MessageKind::offerEnd},
               ProtocolStep{Direction::client, MessageKind::clientHello},
               ProtocolStep{Direction::client, MessageKind::offerReference},
               ProtocolStep{Direction::client, MessageKind::applicationContract},
               ProtocolStep{Direction::client, MessageKind::dataContract},
               ProtocolStep{Direction::client, MessageKind::clientLimitsPage, {0}},
               ProtocolStep{Direction::client, MessageKind::clientLimitsPage, {1}},
               ProtocolStep{Direction::client, MessageKind::clientLimitsPage, {2}},
               ProtocolStep{Direction::client, MessageKind::clientCapabilitiesEnd},
               ProtocolStep{Direction::server, MessageKind::selectionPage, {0}},
               ProtocolStep{Direction::server, MessageKind::selectionPage, {1}},
               ProtocolStep{Direction::server, MessageKind::selectionPage, {2}},
               ProtocolStep{Direction::server, MessageKind::selectionEnd},
               ProtocolStep{Direction::client, MessageKind::clientReady},
               ProtocolStep{Direction::server, MessageKind::serverCommit},
               ProtocolStep{Direction::client, MessageKind::clientCommitAck}};
inline constexpr std::size_t offerFrameCount = 11;
inline constexpr std::size_t capabilitiesEndStep = 18;
inline constexpr std::size_t selectionEndStep = 22;
inline constexpr std::size_t readyStep = 23;
inline constexpr std::size_t commitStep = 24;
inline constexpr std::uint32_t offerHashFrameCount = 10;
inline constexpr std::uint32_t clientHashFrameCount = 7;
inline constexpr std::uint32_t firstSelectionSequence = 11;
[[nodiscard]] constexpr auto directionIndex(Direction direction) noexcept -> std::size_t {
    return direction == Direction::server ? 0 : 1;
}
template <MessageKind kind>
auto page(const CapabilityVector &values, PageOrdinal ordinal) -> FramePayload {
    LimitPageBody<kind> result{ordinal, {}};
    std::ranges::copy(
        std::span{values.wireValues()}.subspan(ordinal.value * limitsPerPage, limitsPerPage),
        result.wireValues.begin());
    return result;
}
template <MessageKind kind>
auto gather(const FramePayload &payload, PageOrdinal expected,
            std::array<std::uint32_t, capabilityCount> &values) -> std::expected<void, Rejection> {
    const auto &body = std::get<LimitPageBody<kind>>(payload);
    if (body.ordinal != expected) {
        return std::unexpected{Rejection{RejectCode::unexpectedMessage}};
    }
    std::ranges::copy(
        body.wireValues,
        std::span{values}.subspan(body.ordinal.value * limitsPerPage, limitsPerPage).begin());
    return {};
}
} // namespace extendedPrecache::session::detail
