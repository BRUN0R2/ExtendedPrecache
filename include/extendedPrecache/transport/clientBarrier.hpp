#pragma once

#include <extendedPrecache/session/negotiation.hpp>
#include <extendedPrecache/transport/controlChannel.hpp>

namespace extendedPrecache::transport {

// The native commands that would start or advance the sign-on of a connection.
enum class SignonCommand : std::uint8_t { connection, resources, spawn };
[[nodiscard]] auto classifySignonCommand(std::string_view name) noexcept
    -> std::optional<SignonCommand>;

// Readable text derived locally, since the control channel never carries strings.
[[nodiscard]] auto refusalReason(session::RejectCode code) noexcept -> const char *;

enum class CommandVerdict : std::uint8_t { forward, hold };
enum class ReleaseDecision : std::uint8_t { wait, continueSignon };
enum class BarrierPhase : std::uint8_t { idle, negotiating, released, refused };

// A reliable channel delivers each command once, so a conversation cannot need more commands than
// it has frames; the engine's own limiter ignores connections that are not active yet.
inline constexpr std::uint32_t maxControlCommands = session::maxFramesPerDirection;

class ReliableChannel {
  public:
    virtual ~ReliableChannel() = default;
    [[nodiscard]] virtual auto state() const noexcept -> session::ChannelState = 0;
    // Queues the whole message or nothing; false means there is no room yet.
    [[nodiscard]] virtual auto queue(std::span<const std::byte> message) noexcept -> bool = 0;
};

// Provides the local contract and map snapshot, which this module does not know.
class SessionSource {
  public:
    virtual ~SessionSource() = default;
    [[nodiscard]] virtual auto open(session::SessionContext context)
        -> std::expected<session::NegotiationSession, session::Rejection> = 0;
};

// Decides for one connection what its sign-on may do. An unexpected result means the connection
// must be closed: the session is gone, and any terminal frame that fitted has been queued.
class ClientBarrier final {
  public:
    // The owner keeps the source and the channel alive for as long as this barrier.
    ClientBarrier(ClientOptIn optIn, session::ConnectionBinding binding, ControlMessageId messageId,
                  SessionSource &sessions, ReliableChannel &channel) noexcept
        : optIn{optIn}, binding{binding}, messageId{messageId}, sessions{&sessions},
          channel{&channel} {}

    [[nodiscard]] auto admit(SignonCommand command, session::MonotonicTime now)
        -> std::expected<CommandVerdict, session::Rejection>;
    [[nodiscard]] auto receiveCommand(std::string_view text, session::MonotonicTime now)
        -> std::expected<void, session::Rejection>;
    // Call once per server frame: it enforces the deadline and sends what the channel accepts.
    [[nodiscard]] auto pump(session::MonotonicTime now) -> std::expected<void, session::Rejection>;
    // Call where the native command would run; continueSignon is granted at most once per map.
    [[nodiscard]] auto tryRelease(session::MonotonicTime now)
        -> std::expected<ReleaseDecision, session::Rejection>;
    // A new map invalidates the agreement; the next sign-on command opens another session.
    void beginGeneration(session::LocalGeneration next) noexcept;

    [[nodiscard]] auto phase() const noexcept -> BarrierPhase {
        return currentPhase;
    }
    [[nodiscard]] auto agreement() const noexcept
        -> const std::optional<session::SessionAgreement> & {
        return released;
    }

  private:
    [[nodiscard]] auto context(session::MonotonicTime now) const noexcept
        -> session::SessionContext;
    // The session exists only while it can still reach an agreement or holds one.
    [[nodiscard]] auto negotiating() noexcept -> session::NegotiationSession *;
    [[nodiscard]] auto open(session::MonotonicTime now) -> std::expected<void, session::Rejection>;
    [[nodiscard]] auto sendOutbound(session::NegotiationSession &live, session::MonotonicTime now)
        -> std::expected<void, session::Rejection>;
    void sendTerminalFrame(session::NegotiationSession &live, session::MonotonicTime now);
    [[nodiscard]] auto refuse(session::Rejection reason, session::MonotonicTime now)
        -> std::unexpected<session::Rejection>;

    ClientOptIn optIn;
    session::ConnectionBinding binding;
    ControlMessageId messageId;
    SessionSource *sessions;
    ReliableChannel *channel;
    BarrierPhase currentPhase{BarrierPhase::idle};
    std::uint32_t commandsSeen{};
    bool registrationQueued{};
    std::optional<session::NegotiationSession> session;
    std::optional<session::SessionAgreement> released;
};

} // namespace extendedPrecache::transport
