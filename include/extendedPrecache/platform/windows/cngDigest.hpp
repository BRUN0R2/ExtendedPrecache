#pragma once

#include <extendedPrecache/session/crypto.hpp>

namespace extendedPrecache::platform::windows {

class CngDigest final : public session::DigestProvider {
  public:
    [[nodiscard]] auto sha256(std::span<const std::byte> input) const noexcept
        -> std::expected<session::Digest, session::CryptoError> override;
};
[[nodiscard]] auto generateSessionNonce() noexcept
    -> std::expected<session::SessionNonce, session::CryptoError>;
[[nodiscard]] auto generateClientNonce() noexcept
    -> std::expected<session::ClientNonce, session::CryptoError>;

} // namespace extendedPrecache::platform::windows
