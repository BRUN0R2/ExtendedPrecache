#include <extendedPrecache/transport/clientNegotiator.hpp>

#include <span>
#include <tuple>
#include <utility>

namespace extendedPrecache::transport {
namespace {
using session::ConnectionPolicy;
using session::Direction;
using session::MessageKind;
using session::MonotonicTime;
using session::NegotiationSession;
using session::ReceiveDisposition;
using session::RejectCode;
using session::Rejection;
using session::SessionState;

auto commandBytes(const std::string &command) noexcept -> std::span<const std::byte> {
    return std::as_bytes(std::span{command});
}
} // namespace

auto ClientNegotiator::context(MonotonicTime now) const noexcept -> session::SessionContext {
    return {binding, now, channel->state()};
}

auto ClientNegotiator::observeRegistration(ObservedRegistration registration, MonotonicTime now)
    -> std::expected<void, Rejection> {
    const auto allocated = ControlMessageId::create(registration.id);
    if (!allocated || registration.size != variableRegistrationSize ||
        (registered && registered != allocated)) {
        return refuse({RejectCode::transportUnavailable}, now);
    }
    registered = allocated;
    return {};
}

auto ClientNegotiator::open(MonotonicTime now) -> std::expected<void, Rejection> {
    auto created = sessions->open(context(now));
    if (!created) {
        return refuse(created.error(), now);
    }
    session.emplace(std::move(*created));
    currentPhase = NegotiatorPhase::negotiating;
    return {};
}

auto ClientNegotiator::receiveFrame(std::span<const std::byte> payload, MonotonicTime now)
    -> std::expected<void, Rejection> {
    if (currentPhase == NegotiatorPhase::refused) {
        return {};
    }
    const auto frame = session::decodeFrame(payload);
    if (!frame) {
        return refuse(frame.error(), now);
    }
    const auto isOffer = session::messageKind(frame->payload) == MessageKind::serverOffer;
    if (!session) {
        if (!isOffer) {
            return {};
        }
        if (const auto opened = open(now); !opened) {
            return opened;
        }
    }
    return deliver(payload, isOffer, now);
}

auto ClientNegotiator::deliver(std::span<const std::byte> payload, bool isOffer, MonotonicTime now)
    -> std::expected<void, Rejection> {
    if (!session) {
        return {};
    }
    const auto received = session->receive(Direction::server, payload, context(now));
    if (!received) {
        return refuse(received.error(), now);
    }
    if (*received != ReceiveDisposition::staleNonce || !isOffer) {
        return {};
    }
    // The old agreement ended with the map or connection it belonged to.
    session.reset();
    if (const auto opened = open(now); !opened) {
        return opened;
    }
    return deliver(payload, false, now);
}

auto ClientNegotiator::pump(MonotonicTime now) -> std::expected<void, Rejection> {
    if (currentPhase != NegotiatorPhase::negotiating || !session) {
        return {};
    }
    auto &live = *session;
    if (const auto polled = live.poll(context(now)); !polled) {
        return refuse(polled.error(), now);
    }
    return sendOutbound(live, now);
}

auto ClientNegotiator::sendOutbound(NegotiationSession &live, MonotonicTime now)
    -> std::expected<void, Rejection> {
    for (;;) {
        const auto prepared = live.prepareOutbound(context(now));
        if (!prepared) {
            return refuse(prepared.error(), now);
        }
        const auto &frame = *prepared;
        if (!frame || !channel->queue(commandBytes(session::encodeClientCommand(*frame)))) {
            return {};
        }
        if (const auto committed = live.commitOutbound(*frame, context(now)); !committed) {
            return refuse(committed.error(), now);
        }
    }
}

auto ClientNegotiator::admitServerInfo(session::ProtocolVersion protocol, session::SpawnCount spawn,
                                       MonotonicTime now) -> std::expected<void, Rejection> {
    if (currentPhase == NegotiatorPhase::idle) {
        if (policy == ConnectionPolicy::requireExtended) {
            return refuse({RejectCode::legacyIncompatible}, now);
        }
        return {};
    }
    if (currentPhase != NegotiatorPhase::negotiating || !session ||
        session->state() != SessionState::armed) {
        return refuse({RejectCode::unexpectedMessage}, now);
    }
    if (const auto confirmed = session->confirmServerInfo(protocol, spawn, context(now));
        !confirmed) {
        return refuse(confirmed.error(), now);
    }
    currentPhase = NegotiatorPhase::confirmed;
    return {};
}

void ClientNegotiator::connectionLost() noexcept {
    session.reset();
    registered.reset();
    currentPhase = NegotiatorPhase::idle;
}

void ClientNegotiator::sendTerminalFrame(NegotiationSession &live, MonotonicTime now) {
    const auto frame = live.prepareOutbound(context(now)).value_or(std::nullopt);
    if (frame && channel->queue(commandBytes(session::encodeClientCommand(*frame)))) {
        // The session already failed; a failed commit cannot change what happens next.
        std::ignore = live.commitOutbound(*frame, context(now));
    }
}

auto ClientNegotiator::refuse(Rejection reason, MonotonicTime now) -> std::unexpected<Rejection> {
    currentPhase = NegotiatorPhase::refused;
    if (!session) {
        return std::unexpected{reason};
    }
    auto &live = *session;
    live.abort(reason);
    const auto cause = live.failure().value_or(reason);
    sendTerminalFrame(live, now);
    session.reset();
    return std::unexpected{cause};
}

} // namespace extendedPrecache::transport
