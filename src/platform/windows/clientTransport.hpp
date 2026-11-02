#pragma once

#include "clientEngine.hpp"

#include <extendedPrecache/session/negotiation.hpp>

#include <memory>

namespace extendedPrecache::platform::windows::client {

// Trusted local source of the contract that a session is opened with, and of the policy.
class ClientProfile {
  public:
    virtual ~ClientProfile() = default;
    [[nodiscard]] virtual auto policy() const noexcept -> session::ConnectionPolicy = 0;
    // The preparer reserves the tables and stays alive for as long as the session.
    [[nodiscard]] virtual auto open(session::SessionContext context,
                                    session::SessionPreparer &preparer)
        -> std::expected<session::NegotiationSession, session::Rejection> = 0;
};

// Empty until a contract profile is approved; without one nothing in the engine changes.
[[nodiscard]] auto approvedClientProfile() -> std::unique_ptr<ClientProfile>;

enum class TransportRefusal : std::uint8_t { noBinding, unexpectedTables };

// The engine's tables keep pointing here, so the transport stays until the process ends. Call it
// once, before the engine starts its client library.
[[nodiscard]] auto installClientTransport(ClientEngine engine,
                                          std::unique_ptr<ClientProfile> profile)
    -> std::expected<void, TransportRefusal>;

} // namespace extendedPrecache::platform::windows::client
