#include "clientTransport.hpp"

namespace extendedPrecache::platform::windows::client {

// No contract profile has been approved yet, so this build changes nothing in the engine.
auto approvedClientProfile() -> std::unique_ptr<ClientProfile> {
    return nullptr;
}

} // namespace extendedPrecache::platform::windows::client
