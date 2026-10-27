#pragma once

#include <extendedPrecache/session/sessionTypes.hpp>

namespace extendedPrecache::session {
inline constexpr std::size_t transcriptHashPrefixBytes =
    sizeof("ExtendedPrecache/selection/1") + 2 * Digest{}.bytes.size();
inline constexpr std::size_t transcriptHashWorkspaceBytes =
    maxTranscriptBytes + transcriptHashPrefixBytes;

enum class CryptoError : std::uint8_t { inputTooLarge, providerFailure };
class DigestProvider {
  public:
    virtual ~DigestProvider() = default;
    [[nodiscard]] virtual auto sha256(std::span<const std::byte> input) const noexcept
        -> std::expected<Digest, CryptoError> = 0;
};

} // namespace extendedPrecache::session
