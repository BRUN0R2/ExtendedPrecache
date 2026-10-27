#include <extendedPrecache/session/negotiation.hpp>

#include "negotiationOrder.hpp"

#include <new>

namespace extendedPrecache::session {
namespace {
auto validateLocal(const Configuration &configuration, const ContractProfile &profile,
                   EndpointRole role, const SessionContext &context)
    -> std::expected<void, Rejection> {
    if (configuration.values().role != role ||
        configuration.values().policy == ConnectionPolicy::legacyOnly ||
        context.binding.connection.isZero() || context.binding.peer.isZero() ||
        context.binding.generation.value == 0 || context.channel == ChannelState::unavailable) {
        return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
    }
    if (profile.availability != ProfileAvailability::available || profile.id.value == 0 ||
        profile.application.isZero() || profile.data.isZero() ||
        configuration.values().applicationContract != profile.application) {
        return std::unexpected{Rejection{RejectCode::unsupportedContract}};
    }
    if (!validateConfiguration(configuration, profile.requirements)) {
        return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
    }
    if (minimumSessionMetadataBytes(configuration) > configuration.values().metadataBudget) {
        return std::unexpected{Rejection{RejectCode::storageUnavailable}};
    }
    return {};
}
} // namespace

NegotiationSession::NegotiationSession(Configuration configuration, ContractProfile profile,
                                       SessionContext context, const DigestProvider &digestProvider,
                                       SessionPreparer &preparer)
    : config{configuration}, contract{profile}, initial{context}, lastTime{context.now},
      timeout{configuration.values().negotiation.timeout}, digests{&digestProvider},
      storage{&preparer} {}

auto NegotiationSession::createServer(Configuration configuration, ContractProfile profile,
                                      MapSnapshot map, SessionNonce nonce, SessionContext context,
                                      const DigestProvider &digestProvider,
                                      SessionPreparer &preparer)
    -> std::expected<NegotiationSession, Rejection> {
    if (const auto valid = validateLocal(configuration, profile, EndpointRole::server, context);
        !valid) {
        return std::unexpected{valid.error()};
    }
    if (nonce.isZero() || map.epoch.value == 0 || map.catalog.isZero()) {
        return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
    }
    if (const auto admitted = admitRequirements(map.requirements, configuration.values().limits);
        !admitted) {
        return std::unexpected{admitted.error()};
    }
    NegotiationSession result{configuration, profile, context, digestProvider, preparer};
    result.snapshot = map;
    result.serverNonce = nonce;
    result.serverLimits = configuration.values().limits;
    result.offer = ServerOfferBody{configuration.values().policy, map.spawn, map.epoch,
                                   configuration.values().negotiation.timeout, profile.id};
    return result;
}

auto NegotiationSession::createClient(Configuration configuration, ContractProfile profile,
                                      ClientNonce nonce, SessionContext context,
                                      const DigestProvider &digestProvider,
                                      SessionPreparer &preparer)
    -> std::expected<NegotiationSession, Rejection> {
    if (const auto valid = validateLocal(configuration, profile, EndpointRole::client, context);
        !valid) {
        return std::unexpected{valid.error()};
    }
    if (nonce.isZero()) {
        return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
    }
    NegotiationSession result{configuration, profile, context, digestProvider, preparer};
    result.clientNonce = nonce;
    result.clientLimits = configuration.values().limits;
    return result;
}

auto NegotiationSession::ownDirection() const noexcept -> Direction {
    return config.values().role == EndpointRole::server ? Direction::server : Direction::client;
}

auto NegotiationSession::state() const noexcept -> SessionState {
    if (closed) {
        return SessionState::closed;
    }
    if (terminalFailure) {
        return SessionState::rejected;
    }
    if (ownDirection() == Direction::server) {
        if (step == detail::order.size()) {
            return SessionState::committed;
        }
        if (step > detail::commitStep) {
            return SessionState::awaitCommitAck;
        }
        return step > detail::capabilitiesEndStep ? SessionState::awaitReady
                                                  : SessionState::awaitCapabilities;
    }
    if (step == detail::order.size()) {
        return SessionState::armed;
    }
    if (step > detail::selectionEndStep) {
        return SessionState::awaitCommit;
    }
    return step >= detail::offerFrameCount ? SessionState::awaitSelection
                                           : SessionState::awaitOffer;
}

auto NegotiationSession::reject(Rejection reason) noexcept -> std::unexpected<Rejection> {
    abort(reason);
    return std::unexpected{terminalFailure.value_or(Rejection{RejectCode::transportUnavailable})};
}
void NegotiationSession::abort(Rejection reason) noexcept {
    if (!terminalFailure) {
        terminalFailure = reason;
    }
    pending.reset();
    prepared.reset();
}
void NegotiationSession::close() noexcept {
    closed = true;
    pending.reset();
    prepared.reset();
}

auto NegotiationSession::checkContext(SessionContext context) -> std::expected<void, Rejection> {
    if (context.binding.connection != initial.binding.connection ||
        context.binding.peer != initial.binding.peer) {
        return std::unexpected{Rejection{RejectCode::transportUnavailable}};
    }
    if (terminalFailure) {
        return std::unexpected{
            terminalFailure.value_or(Rejection{RejectCode::transportUnavailable})};
    }
    if (closed) {
        return std::unexpected{Rejection{RejectCode::transportUnavailable}};
    }
    if (context.binding.generation != initial.binding.generation) {
        return reject({RejectCode::mapChanged});
    }
    if (context.channel == ChannelState::unavailable || context.now < lastTime) {
        return reject({RejectCode::transportUnavailable});
    }
    lastTime = context.now;
    if (!nativeHandoff && context.now.value - initial.now.value >= timeout.value) {
        return reject({RejectCode::deadlineExceeded});
    }
    return {};
}
auto NegotiationSession::poll(SessionContext context) -> std::expected<void, Rejection> {
    return checkContext(context);
}

auto NegotiationSession::prepareOutbound(SessionContext context)
    -> std::expected<std::optional<EncodedFrame>, Rejection> {
    if (terminalFailure) {
        if (closed || rejectionSent || rejectionReceived || serverNonce.isZero() ||
            context.binding.connection != initial.binding.connection ||
            context.binding.peer != initial.binding.peer ||
            context.channel == ChannelState::unavailable) {
            return std::optional<EncodedFrame>{};
        }
        if (!pending) {
            const auto index = detail::directionIndex(ownDirection());
            const auto encoded =
                encodeFrame({serverNonce, {counts[index]}, RejectBody{*terminalFailure}});
            if (!encoded || logicalBytes + encoded->bytes().size() >
                                config.values().negotiation.maxBytes.value) {
                return std::optional<EncodedFrame>{};
            }
            pending = *encoded;
        }
        return pending;
    }
    if (const auto valid = checkContext(context); !valid) {
        return std::unexpected{valid.error()};
    }
    if (pending) {
        return pending;
    }
    if (step == detail::order.size() || detail::order[step].direction != ownDirection()) {
        return std::optional<EncodedFrame>{};
    }
    const auto kind = detail::order[step].kind;
    if (kind == MessageKind::clientReady || kind == MessageKind::serverCommit ||
        kind == MessageKind::clientCommitAck) {
        if (const auto ready = prepareStorage(); !ready) {
            return reject(ready.error());
        }
    }
    const auto payload = makePayload();
    if (!payload) {
        return reject(payload.error());
    }
    const auto encoded =
        encodeFrame({serverNonce, {counts[detail::directionIndex(ownDirection())]}, *payload});
    if (!encoded) {
        return reject(encoded.error());
    }
    if (logicalBytes + encoded->bytes().size() > config.values().negotiation.maxBytes.value) {
        return reject({RejectCode::budgetExceeded});
    }
    pending = *encoded;
    return pending;
}

auto NegotiationSession::commitOutbound(const EncodedFrame &queued, SessionContext context)
    -> std::expected<void, Rejection> {
    if (context.binding.connection != initial.binding.connection ||
        context.binding.peer != initial.binding.peer) {
        return std::unexpected{Rejection{RejectCode::transportUnavailable}};
    }
    if (closed || !pending || queued != *pending) {
        if (terminalFailure) {
            return std::unexpected{
                terminalFailure.value_or(Rejection{RejectCode::transportUnavailable})};
        }
        return reject({RejectCode::transportUnavailable});
    }
    if (terminalFailure) {
        if (context.channel == ChannelState::unavailable) {
            pending.reset();
            return std::unexpected{
                terminalFailure.value_or(Rejection{RejectCode::transportUnavailable})};
        }
        ++counts[detail::directionIndex(ownDirection())];
        logicalBytes += static_cast<std::uint32_t>(queued.bytes().size());
        rejectionSent = true;
        pending.reset();
        return {};
    }
    if (const auto valid = checkContext(context); !valid) {
        return valid;
    }
    const auto frame = decodeFrame(queued.bytes()).value();
    if (messageKind(frame.payload) == MessageKind::clientReady ||
        messageKind(frame.payload) == MessageKind::serverCommit ||
        messageKind(frame.payload) == MessageKind::clientCommitAck) {
        if (const auto valid = checkStorage(); !valid) {
            return reject(valid.error());
        }
    }
    const auto accepted = accept(ownDirection(), frame, queued);
    pending.reset();
    return accepted;
}

auto NegotiationSession::receive(Direction direction, std::span<const std::byte> bytes,
                                 SessionContext context)
    -> std::expected<ReceiveDisposition, Rejection> {
    if (context.binding.connection != initial.binding.connection ||
        context.binding.peer != initial.binding.peer) {
        return ReceiveDisposition::foreignConnection;
    }
    if (const auto valid = checkContext(context); !valid) {
        return std::unexpected{valid.error()};
    }
    if (direction != Direction::server && direction != Direction::client) {
        return reject({RejectCode::unexpectedMessage});
    }
    if (direction == ownDirection()) {
        return reject({RejectCode::unexpectedMessage});
    }
    const auto decoded = decodeFrame(bytes);
    if (!decoded) {
        return reject(decoded.error());
    }
    const auto &frame = *decoded;
    if (!serverNonce.isZero() && frame.nonce != serverNonce) {
        return ReceiveDisposition::staleNonce;
    }
    const auto index = detail::directionIndex(direction);
    if (frame.sequence.value < counts[index]) {
        if (std::ranges::equal(bytes, transcript[index][frame.sequence.value].bytes())) {
            return ReceiveDisposition::duplicate;
        }
        return reject({RejectCode::unexpectedSequence});
    }
    if (frame.sequence.value != counts[index]) {
        return reject({RejectCode::unexpectedSequence});
    }
    if (logicalBytes + bytes.size() > config.values().negotiation.maxBytes.value) {
        return reject({RejectCode::budgetExceeded});
    }
    if (messageKind(frame.payload) == MessageKind::reject) {
        ++counts[index];
        logicalBytes += static_cast<std::uint32_t>(bytes.size());
        rejectionReceived = true;
        return reject(std::get<RejectBody>(frame.payload).value);
    }
    if (step == detail::order.size() || detail::order[step].direction != direction ||
        messageKind(frame.payload) != detail::order[step].kind) {
        return reject({RejectCode::unexpectedMessage});
    }
    if (serverNonce.isZero()) {
        serverNonce = frame.nonce;
    }
    const auto accepted = accept(direction, frame, encodeFrame(frame).value());
    if (!accepted) {
        return std::unexpected{accepted.error()};
    }
    // A smaller peer timeout applies to the original start, including time already spent.
    if (const auto valid = checkContext(context); !valid) {
        return std::unexpected{valid.error()};
    }
    return ReceiveDisposition::accepted;
}

auto NegotiationSession::accept(Direction direction, const Frame &frame,
                                const EncodedFrame &encoded) -> std::expected<void, Rejection> {
    if (const auto valid = validatePayload(frame); !valid) {
        return reject(valid.error());
    }
    const auto index = detail::directionIndex(direction);
    transcript[index][counts[index]] = encoded;
    ++counts[index];
    logicalBytes += static_cast<std::uint32_t>(encoded.bytes().size());
    ++step;
    return {};
}

auto NegotiationSession::agreement() const -> std::expected<SessionAgreement, Rejection> {
    if (!snapshot || !selected) {
        return std::unexpected{Rejection{RejectCode::unexpectedMessage}};
    }
    return SessionAgreement{initial.binding, serverNonce, clientNonce,  contract,
                            *snapshot,       *selected,   selectionHash};
}
auto NegotiationSession::prepareStorage() -> std::expected<void, Rejection> {
    if (prepared) {
        return checkStorage();
    }
    try {
        const auto current = agreement();
        if (!current) {
            return std::unexpected{current.error()};
        }
        auto result = storage->prepare(config, *current);
        if (!result) {
            return std::unexpected{result.error()};
        }
        prepared = std::move(*result);
    } catch (const std::bad_alloc &) {
        return std::unexpected{Rejection{RejectCode::storageUnavailable}};
    }
    return checkStorage();
}
auto NegotiationSession::checkStorage() -> std::expected<void, Rejection> {
    if (!prepared || prepared->peakMetadataBytes() < minimumSessionMetadataBytes(config) ||
        prepared->peakMetadataBytes() > config.values().metadataBudget) {
        return std::unexpected{Rejection{RejectCode::storageUnavailable}};
    }
    const auto current = agreement();
    if (!current) {
        return std::unexpected{current.error()};
    }
    if (!prepared->stillValid(*current)) {
        return std::unexpected{Rejection{RejectCode::mapChanged}};
    }
    return {};
}
auto NegotiationSession::releaseSignon(SessionContext context)
    -> std::expected<SessionAgreement, Rejection> {
    if (const auto valid = checkContext(context); !valid) {
        return std::unexpected{valid.error()};
    }
    if (state() != SessionState::committed || context.channel != ChannelState::drained) {
        return std::unexpected{Rejection{RejectCode::unexpectedMessage}};
    }
    if (const auto valid = checkStorage(); !valid) {
        return reject(valid.error());
    }
    nativeHandoff = true;
    return agreement();
}
auto NegotiationSession::confirmServerInfo(std::uint32_t protocol, SpawnCount spawn,
                                           SessionContext context)
    -> std::expected<void, Rejection> {
    if (const auto valid = checkContext(context); !valid) {
        return valid;
    }
    if (nativeHandoff || state() != SessionState::armed) {
        return reject({RejectCode::unexpectedMessage});
    }
    if (!snapshot || protocol != legacyProtocolVersion || spawn != snapshot.value().spawn) {
        return reject({RejectCode::unsupportedContract});
    }
    if (const auto valid = checkStorage(); !valid) {
        return reject(valid.error());
    }
    nativeHandoff = true;
    return {};
}

auto minimumSessionMetadataBytes(const Configuration &configuration) noexcept -> MetadataBytes {
    constexpr auto controlBytes =
        std::uint64_t{sizeof(NegotiationSession) + transcriptHashWorkspaceBytes};
    const auto budgetedTranscript = configuration.values().policy == ConnectionPolicy::legacyOnly
                                        ? 0U
                                        : configuration.values().negotiation.maxBytes.value;
    return {referenceMetadataBytes(configuration).value + controlBytes -
            std::min(controlBytes, std::uint64_t{budgetedTranscript})};
}

auto checkLegacyAdmission(const Configuration &configuration, const MapRequirements &map,
                          LegacyProof proof, bool negotiationStarted) noexcept
    -> std::expected<void, Rejection> {
    if (configuration.values().policy == ConnectionPolicy::requireExtended || negotiationStarted ||
        proof != LegacyProof::complete) {
        return std::unexpected{Rejection{RejectCode::legacyIncompatible}};
    }
    const auto admitted =
        admitRequirements(map, intersectLimits(configuration.values().limits, legacyLimits()));
    if (!admitted) {
        auto error = admitted.error();
        error.code = RejectCode::legacyIncompatible;
        return std::unexpected{error};
    }
    return {};
}
} // namespace extendedPrecache::session
