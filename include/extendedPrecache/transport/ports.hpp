#pragma once

#include <extendedPrecache/session/negotiation.hpp>

namespace extendedPrecache::transport {

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

} // namespace extendedPrecache::transport
