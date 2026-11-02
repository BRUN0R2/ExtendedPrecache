#include "serverTransport.hpp"

namespace extendedPrecache::platform::windows {

// No contract profile has been approved yet, so this build installs nothing in the engine.
auto approvedTransportProfile() -> std::unique_ptr<TransportProfile> {
    return nullptr;
}

} // namespace extendedPrecache::platform::windows
