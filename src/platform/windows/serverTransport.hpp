#pragma once

#include "rehldsApi.hpp"

#include <extendedPrecache/metamod/lifecycle.hpp>
#include <extendedPrecache/session/negotiation.hpp>
#include <extendedPrecache/transport/controlChannel.hpp>

#include <memory>

namespace extendedPrecache::platform::windows {

// Trusted local source of the contract and the map snapshot that a session is opened with.
class TransportProfile {
  public:
    virtual ~TransportProfile() = default;
    [[nodiscard]] virtual auto open(session::SessionContext context, session::SpawnCount spawn)
        -> std::expected<session::NegotiationSession, session::Rejection> = 0;
};

// Empty until a contract profile is approved; without one no engine hook is installed.
[[nodiscard]] auto approvedTransportProfile() -> std::unique_ptr<TransportProfile>;

struct TransportHost final {
    void (*report)(const char *text) noexcept;
    void (*reportError)(const char *text) noexcept;
    // Empty when the control message cannot be registered and owned by this plugin.
    std::optional<transport::ControlMessageId> (*registerControlMessage)() noexcept;
};

// Hooks stay registered until the process ends, so the use keeps the module loaded until then.
void installServerTransport(rehlds::Api api, std::unique_ptr<TransportProfile> profile,
                            TransportHost host, metamod::RetainedUse use);

} // namespace extendedPrecache::platform::windows
