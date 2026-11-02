#pragma once

#include <extendedPrecache/session/negotiation.hpp>
#include <extendedPrecache/transport/controlChannel.hpp>
#include <extendedPrecache/transport/ports.hpp>

namespace extendedPrecache::transport {

// The wire writes a variable size as this byte, in the registration of a user message.
inline constexpr std::int32_t variableRegistrationSize = 255;

// As read from the wire, before anything about it has been checked.
struct ObservedRegistration final {
    std::int32_t id;
    std::int32_t size;
};

enum class NegotiatorPhase : std::uint8_t { idle, negotiating, confirmed, refused };

// Conducts the client's side of the negotiation, across the maps of a connection. An unexpected
// result means the connection must be closed: the session is gone, and any terminal frame that
// fitted has been queued.
class ClientNegotiator final {
  public:
    // The owner keeps the source and the channel alive for as long as this object.
    ClientNegotiator(session::ConnectionPolicy policy, session::ConnectionBinding binding,
                     SessionSource &sessions, ReliableChannel &channel) noexcept
        : policy{policy}, binding{binding}, sessions{&sessions}, channel{&channel} {}

    // The engine repeats registrations, so none of them opens a session; only the identifier and
    // the size are checked, and the identifier must hold for the whole connection.
    [[nodiscard]] auto observeRegistration(ObservedRegistration registration,
                                           session::MonotonicTime now)
        -> std::expected<void, session::Rejection>;
    // A new session is an offer under another nonce; a frame that is not an offer never opens one.
    [[nodiscard]] auto receiveFrame(std::span<const std::byte> payload, session::MonotonicTime now)
        -> std::expected<void, session::Rejection>;
    // Call once per client frame: it enforces the deadline and sends what the channel accepts.
    [[nodiscard]] auto pump(session::MonotonicTime now) -> std::expected<void, session::Rejection>;
    // Call before the engine parses a serverinfo, whose parser starts by resetting client state.
    [[nodiscard]] auto admitServerInfo(session::ProtocolVersion protocol, session::SpawnCount spawn,
                                       session::MonotonicTime now)
        -> std::expected<void, session::Rejection>;
    // The connection ended or another one began, so nothing here can be answered anymore.
    void connectionLost() noexcept;

    [[nodiscard]] auto phase() const noexcept -> NegotiatorPhase {
        return currentPhase;
    }

  private:
    [[nodiscard]] auto context(session::MonotonicTime now) const noexcept
        -> session::SessionContext;
    [[nodiscard]] auto open(session::MonotonicTime now) -> std::expected<void, session::Rejection>;
    [[nodiscard]] auto deliver(std::span<const std::byte> payload, bool isOffer,
                               session::MonotonicTime now)
        -> std::expected<void, session::Rejection>;
    [[nodiscard]] auto sendOutbound(session::NegotiationSession &live, session::MonotonicTime now)
        -> std::expected<void, session::Rejection>;
    void sendTerminalFrame(session::NegotiationSession &live, session::MonotonicTime now);
    [[nodiscard]] auto refuse(session::Rejection reason, session::MonotonicTime now)
        -> std::unexpected<session::Rejection>;

    session::ConnectionPolicy policy;
    session::ConnectionBinding binding;
    SessionSource *sessions;
    ReliableChannel *channel;
    NegotiatorPhase currentPhase{NegotiatorPhase::idle};
    std::optional<ControlMessageId> registered;
    std::optional<session::NegotiationSession> session;
};

} // namespace extendedPrecache::transport
