#pragma once

#include <extendedPrecache/session/configuration.hpp>
#include <extendedPrecache/session/crypto.hpp>
#include <extendedPrecache/session/frameCodec.hpp>

#include <memory>

namespace extendedPrecache::session {

enum class ProfileAvailability : std::uint8_t { unavailable, available };
struct ContractProfile final {
    ProfileAvailability availability;
    DataContractId id;
    Digest application;
    Digest data;
    ConfigurationRequirements requirements;
};
struct ConnectionBinding final {
    ConnectionToken connection;
    PeerToken peer;
    LocalGeneration generation;
    auto operator==(const ConnectionBinding &) const -> bool = default;
};
enum class ChannelState : std::uint8_t { unavailable, pending, drained };
struct SessionContext final {
    ConnectionBinding binding;
    MonotonicTime now;
    ChannelState channel;
};
struct MapSnapshot final {
    SpawnCount spawn;
    MapEpoch epoch;
    Digest catalog;
    MapRequirements requirements;
};
struct SessionAgreement final {
    ConnectionBinding binding;
    SessionNonce nonce;
    ClientNonce clientNonce;
    ContractProfile profile;
    MapSnapshot map;
    CapabilityLimits selected;
    Digest selectionHash;
};

// The owner must keep the provider and its durable storage alive through native reset.
class PreparedSession {
  public:
    virtual ~PreparedSession() = default;
    [[nodiscard]] virtual auto peakMetadataBytes() const noexcept -> MetadataBytes = 0;
    [[nodiscard]] virtual auto stillValid(const SessionAgreement &agreement) const noexcept
        -> bool = 0;
};
class SessionPreparer {
  public:
    virtual ~SessionPreparer() = default;
    [[nodiscard]] virtual auto prepare(const Configuration &configuration,
                                       const SessionAgreement &agreement)
        -> std::expected<std::unique_ptr<PreparedSession>, Rejection> = 0;
};

enum class SessionState : std::uint8_t {
    awaitCapabilities,
    awaitReady,
    awaitCommitAck,
    committed,
    awaitOffer,
    awaitSelection,
    awaitCommit,
    armed,
    rejected,
    closed
};
enum class ReceiveDisposition : std::uint8_t { accepted, duplicate, staleNonce, foreignConnection };

class NegotiationSession final {
  public:
    [[nodiscard]] static auto
    createServer(Configuration configuration, ContractProfile profile, MapSnapshot map,
                 SessionNonce nonce, SessionContext context, const DigestProvider &digestProvider,
                 SessionPreparer &preparer) -> std::expected<NegotiationSession, Rejection>;
    [[nodiscard]] static auto createClient(Configuration configuration, ContractProfile profile,
                                           ClientNonce nonce, SessionContext context,
                                           const DigestProvider &digestProvider,
                                           SessionPreparer &preparer)
        -> std::expected<NegotiationSession, Rejection>;
    NegotiationSession(const NegotiationSession &) = delete;
    auto operator=(const NegotiationSession &) -> NegotiationSession & = delete;
    NegotiationSession(NegotiationSession &&) noexcept = default;
    auto operator=(NegotiationSession &&) noexcept -> NegotiationSession & = default;
    ~NegotiationSession() = default;

    // Commit only after the reliable channel has accepted this exact prepared frame.
    [[nodiscard]] auto prepareOutbound(SessionContext context)
        -> std::expected<std::optional<EncodedFrame>, Rejection>;
    [[nodiscard]] auto commitOutbound(const EncodedFrame &queued, SessionContext context)
        -> std::expected<void, Rejection>;
    [[nodiscard]] auto receive(Direction direction, std::span<const std::byte> bytes,
                               SessionContext context)
        -> std::expected<ReceiveDisposition, Rejection>;
    [[nodiscard]] auto poll(SessionContext context) -> std::expected<void, Rejection>;
    [[nodiscard]] auto releaseSignon(SessionContext context)
        -> std::expected<SessionAgreement, Rejection>;
    [[nodiscard]] auto confirmServerInfo(std::uint32_t protocol, SpawnCount spawn,
                                         SessionContext context) -> std::expected<void, Rejection>;
    void abort(Rejection reason) noexcept;
    void close() noexcept;
    [[nodiscard]] auto state() const noexcept -> SessionState;
    [[nodiscard]] auto failure() const noexcept -> std::optional<Rejection> {
        return terminalFailure;
    }
    [[nodiscard]] auto transcriptBytes() const noexcept -> ByteCount {
        return {logicalBytes};
    }

  private:
    NegotiationSession(Configuration configuration, ContractProfile profile, SessionContext context,
                       const DigestProvider &digestProvider, SessionPreparer &preparer);
    [[nodiscard]] auto observeGeneration(const ConnectionBinding &binding) noexcept -> bool;
    [[nodiscard]] auto checkContext(SessionContext context) -> std::expected<void, Rejection>;
    [[nodiscard]] auto makePayload() const -> std::expected<FramePayload, Rejection>;
    [[nodiscard]] auto accept(Direction direction, const Frame &frame, const EncodedFrame &encoded)
        -> std::expected<void, Rejection>;
    [[nodiscard]] auto validatePayload(const Frame &frame) -> std::expected<void, Rejection>;
    [[nodiscard]] auto hash(MessageKind kind) const noexcept -> std::expected<Digest, Rejection>;
    [[nodiscard]] auto agreement() const -> std::expected<SessionAgreement, Rejection>;
    [[nodiscard]] auto prepareStorage() -> std::expected<void, Rejection>;
    [[nodiscard]] auto checkStorage() -> std::expected<void, Rejection>;
    [[nodiscard]] auto ownDirection() const noexcept -> Direction;
    [[nodiscard]] auto reject(Rejection reason) noexcept -> std::unexpected<Rejection>;

    Configuration config;
    ContractProfile contract;
    SessionContext initial;
    MonotonicTime lastTime;
    DurationMs timeout;
    const DigestProvider *digests;
    SessionPreparer *storage;
    std::optional<MapSnapshot> snapshot;
    SessionNonce serverNonce{};
    ClientNonce clientNonce{};
    std::optional<CapabilityLimits> serverLimits;
    std::optional<CapabilityLimits> clientLimits;
    std::optional<CapabilityLimits> selected;
    std::array<std::uint32_t, capabilityCount> requirementValues{};
    std::array<std::uint32_t, capabilityCount> limitValues{};
    std::array<std::uint32_t, capabilityCount> selectionValues{};
    std::optional<ServerOfferBody> offer;
    Digest catalog{};
    Digest offerHash{};
    Digest clientHash{};
    Digest selectionHash{};
    std::array<std::array<EncodedFrame, maxFramesPerDirection>, 2> transcript{};
    std::array<std::uint32_t, 2> counts{};
    std::uint32_t logicalBytes{};
    std::size_t step{};
    std::optional<EncodedFrame> pending;
    std::unique_ptr<PreparedSession> prepared;
    std::optional<Rejection> terminalFailure;
    bool rejectionSent{};
    bool rejectionReceived{};
    bool closed{};
    bool nativeHandoff{};
    bool generationInvalidated{};
};

[[nodiscard]] auto minimumSessionMetadataBytes(const Configuration &configuration) noexcept
    -> MetadataBytes;

// The external proof covers all native consumers, byte budgets and module contracts.
enum class LegacyProof : std::uint8_t { unverified, complete };
[[nodiscard]] auto checkLegacyAdmission(const Configuration &configuration,
                                        const MapRequirements &map, LegacyProof proof,
                                        bool negotiationStarted) noexcept
    -> std::expected<void, Rejection>;

} // namespace extendedPrecache::session
