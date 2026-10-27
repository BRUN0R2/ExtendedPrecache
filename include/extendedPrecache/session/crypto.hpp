#pragma once

#include <extendedPrecache/session/sessionTypes.hpp>
#include <memory>

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

class DigestStream {
  public:
    virtual ~DigestStream() = default;
    [[nodiscard]] virtual auto update(std::span<const std::byte> input) noexcept
        -> std::expected<void, CryptoError> = 0;
    [[nodiscard]] virtual auto finish() noexcept -> std::expected<Digest, CryptoError> = 0;
};
class StreamingDigestProvider {
  public:
    virtual ~StreamingDigestProvider() = default;
    [[nodiscard]] virtual auto beginSha256() const noexcept
        -> std::expected<std::unique_ptr<DigestStream>, CryptoError> = 0;
};

} // namespace extendedPrecache::session
