#pragma once

#include "engineIdentity.hpp"
#include "imageRewrite.hpp"
#include "rehldsApi.hpp"

#include <extendedPrecache/metamod/lifecycle.hpp>
#include <extendedPrecache/session/negotiation.hpp>
#include <extendedPrecache/transport/controlChannel.hpp>

#include <memory>

namespace extendedPrecache::platform::windows {

// What the engine itself says about the current map.
struct MapFacts final {
    session::SpawnCount spawn;
    std::uint32_t modelEntries;
};

// Trusted local source of the contract and the map snapshot that a session is opened with.
class TransportProfile {
  public:
    virtual ~TransportProfile() = default;
    // The configured limit, which decides how far the tables move before the first map.
    [[nodiscard]] virtual auto modelEntries() const noexcept -> std::uint32_t = 0;
    // The preparer accounts for the tables and stays alive for as long as the session.
    [[nodiscard]] virtual auto open(session::SessionContext context, MapFacts map,
                                    session::SessionPreparer &preparer)
        -> std::expected<session::NegotiationSession, session::Rejection> = 0;
    [[nodiscard]] virtual auto admitLegacy(MapFacts map) const
        -> std::expected<void, session::Rejection> = 0;
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
// Call before the first map, which precaches into the tables this moves.
[[nodiscard]] auto installServerTransport(SelectedEngine engine,
                                          std::unique_ptr<TransportProfile> profile,
                                          TransportHost host, metamod::RetainedUse use)
    -> std::expected<void, RewriteRefusal>;

} // namespace extendedPrecache::platform::windows
