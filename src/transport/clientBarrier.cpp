#include <extendedPrecache/transport/clientBarrier.hpp>

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>

namespace extendedPrecache::transport {
namespace {
using session::ChannelState;
using session::Direction;
using session::MonotonicTime;
using session::NegotiationSession;
using session::RejectCode;
using session::Rejection;
using session::SessionState;

constexpr auto connectionCommand = std::string_view{"new"};
constexpr auto resourcesCommand = std::string_view{"sendres"};
constexpr auto spawnCommand = std::string_view{"spawn"};

// The engine finds these commands ignoring case, so the barrier must as well.
constexpr auto equalIgnoringCase(std::string_view left, std::string_view right) noexcept -> bool {
    const auto lower = [](char value) {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
    };
    return std::ranges::equal(left, right, {}, lower, lower);
}
} // namespace

auto classifySignonCommand(std::string_view name) noexcept -> std::optional<SignonCommand> {
    if (equalIgnoringCase(name, connectionCommand)) {
        return SignonCommand::connection;
    }
    if (equalIgnoringCase(name, resourcesCommand)) {
        return SignonCommand::resources;
    }
    if (equalIgnoringCase(name, spawnCommand)) {
        return SignonCommand::spawn;
    }
    return std::nullopt;
}

auto refusalReason(RejectCode code) noexcept -> const char * {
    switch (code) {
    case RejectCode::malformedFrame:
        return "ExtendedPrecache: malformed negotiation data";
    case RejectCode::unsupportedVersion:
        return "ExtendedPrecache: unsupported negotiation version";
    case RejectCode::unexpectedMessage:
    case RejectCode::unexpectedSequence:
        return "ExtendedPrecache: negotiation out of order";
    case RejectCode::unsupportedContract:
        return "ExtendedPrecache: no contract in common";
    case RejectCode::capacityInsufficient:
        return "ExtendedPrecache: this map exceeds the negotiated limits";
    case RejectCode::invalidConfiguration:
        return "ExtendedPrecache: invalid negotiation configuration";
    case RejectCode::storageUnavailable:
        return "ExtendedPrecache: not enough memory for this map";
    case RejectCode::transcriptMismatch:
        return "ExtendedPrecache: negotiation data does not match";
    case RejectCode::deadlineExceeded:
        return "ExtendedPrecache: negotiation timed out";
    case RejectCode::mapChanged:
        return "ExtendedPrecache: the map changed during negotiation";
    case RejectCode::legacyIncompatible:
        return "ExtendedPrecache: this map requires the extension";
    case RejectCode::transportUnavailable:
        return "ExtendedPrecache: negotiation channel unavailable";
    case RejectCode::budgetExceeded:
        return "ExtendedPrecache: too much negotiation traffic";
    }
    return "ExtendedPrecache: negotiation refused";
}

auto ClientBarrier::context(MonotonicTime now) const noexcept -> session::SessionContext {
    return {binding, now, channel->state()};
}

auto ClientBarrier::negotiating() noexcept -> NegotiationSession * {
    return currentPhase == BarrierPhase::negotiating && session ? std::addressof(*session)
                                                                : nullptr;
}

auto ClientBarrier::admit(SignonCommand command, MonotonicTime now)
    -> std::expected<CommandVerdict, Rejection> {
    if (optIn == ClientOptIn::legacy || currentPhase == BarrierPhase::released) {
        return CommandVerdict::forward;
    }
    // A native spawn with a stale count runs the connection body, so it opens a session as well.
    if (currentPhase == BarrierPhase::idle && command != SignonCommand::resources) {
        if (const auto opened = open(now); !opened) {
            return std::unexpected{opened.error()};
        }
    }
    return CommandVerdict::hold;
}

auto ClientBarrier::open(MonotonicTime now) -> std::expected<void, Rejection> {
    auto created = sessions->open(context(now));
    if (!created) {
        return refuse(created.error(), now);
    }
    session.emplace(std::move(*created));
    currentPhase = BarrierPhase::negotiating;
    commandsSeen = 0;
    registrationQueued = false;
    return {};
}

auto ClientBarrier::receiveCommand(std::string_view text, MonotonicTime now)
    -> std::expected<void, Rejection> {
    if (commandsSeen == maxControlCommands) {
        return refuse({RejectCode::budgetExceeded}, now);
    }
    ++commandsSeen;
    auto *const live = negotiating();
    if (live == nullptr) {
        return {};
    }
    const auto frame = session::decodeClientCommand(text);
    if (!frame) {
        return refuse(frame.error(), now);
    }
    const auto encoded = session::encodeFrame(*frame);
    if (!encoded) {
        return refuse(encoded.error(), now);
    }
    const auto received = live->receive(Direction::client, encoded->bytes(), context(now));
    if (!received) {
        return refuse(received.error(), now);
    }
    return {};
}

auto ClientBarrier::pump(MonotonicTime now) -> std::expected<void, Rejection> {
    auto *const live = negotiating();
    if (live == nullptr) {
        return {};
    }
    if (const auto polled = live->poll(context(now)); !polled) {
        return refuse(polled.error(), now);
    }
    return sendOutbound(*live, now);
}

auto ClientBarrier::sendOutbound(NegotiationSession &live, MonotonicTime now)
    -> std::expected<void, Rejection> {
    if (!registrationQueued) {
        registrationQueued = channel->queue(encodeRegistration(messageId).bytes());
        if (!registrationQueued) {
            return {};
        }
    }
    for (;;) {
        const auto prepared = live.prepareOutbound(context(now));
        if (!prepared) {
            return refuse(prepared.error(), now);
        }
        const auto &frame = *prepared;
        if (!frame || !channel->queue(encodeFrameMessage(messageId, *frame).bytes())) {
            return {};
        }
        if (const auto committed = live.commitOutbound(*frame, context(now)); !committed) {
            return refuse(committed.error(), now);
        }
    }
}

auto ClientBarrier::tryRelease(MonotonicTime now) -> std::expected<ReleaseDecision, Rejection> {
    auto *const live = negotiating();
    if (live == nullptr || live->state() != SessionState::committed ||
        channel->state() != ChannelState::drained) {
        return ReleaseDecision::wait;
    }
    const auto granted = live->releaseSignon(context(now));
    if (!granted) {
        return refuse(granted.error(), now);
    }
    released = *granted;
    currentPhase = BarrierPhase::released;
    return ReleaseDecision::continueSignon;
}

void ClientBarrier::beginGeneration(session::LocalGeneration next) noexcept {
    binding.generation = next;
    if (currentPhase == BarrierPhase::refused) {
        return;
    }
    session.reset();
    released.reset();
    currentPhase = BarrierPhase::idle;
}

void ClientBarrier::sendTerminalFrame(NegotiationSession &live, MonotonicTime now) {
    if (!registrationQueued) {
        return;
    }
    const auto frame = live.prepareOutbound(context(now)).value_or(std::nullopt);
    if (frame && channel->queue(encodeFrameMessage(messageId, *frame).bytes())) {
        // The session already failed; a failed commit cannot change what happens next.
        std::ignore = live.commitOutbound(*frame, context(now));
    }
}

auto ClientBarrier::refuse(Rejection reason, MonotonicTime now) -> std::unexpected<Rejection> {
    currentPhase = BarrierPhase::refused;
    if (!session) {
        return std::unexpected{reason};
    }
    auto &live = *session;
    live.abort(reason);
    const auto cause = live.failure().value_or(reason);
    sendTerminalFrame(live, now);
    session.reset();
    released.reset();
    return std::unexpected{cause};
}

} // namespace extendedPrecache::transport
