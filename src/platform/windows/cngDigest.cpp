#include <extendedPrecache/platform/windows/cngDigest.hpp>

#include <windows.h>

#include <bcrypt.h>
#include <limits>

namespace extendedPrecache::platform::windows {
using namespace session;
namespace {
template <typename T> auto randomNonce() noexcept -> std::expected<T, CryptoError> {
    T result;
    const auto status =
        BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(result.bytes.data()),
                        static_cast<ULONG>(result.bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status < 0 || result.isZero()) {
        return std::unexpected{CryptoError::providerFailure};
    }
    return result;
}
} // namespace

auto CngDigest::sha256(std::span<const std::byte> input) const noexcept
    -> std::expected<Digest, CryptoError> {
    if (input.size() > std::numeric_limits<ULONG>::max()) {
        return std::unexpected{CryptoError::inputTooLarge};
    }
    Digest result;
    // BCryptHash's legacy pointer type is mutable; its documented input contract is read-only.
    auto *data = const_cast<PUCHAR>(reinterpret_cast<const UCHAR *>(input.data()));
    const auto status = BCryptHash(
        BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, data, static_cast<ULONG>(input.size()),
        reinterpret_cast<PUCHAR>(result.bytes.data()), static_cast<ULONG>(result.bytes.size()));
    if (status < 0) {
        return std::unexpected{CryptoError::providerFailure};
    }
    return result;
}

auto generateSessionNonce() noexcept -> std::expected<SessionNonce, CryptoError> {
    return randomNonce<SessionNonce>();
}
auto generateClientNonce() noexcept -> std::expected<ClientNonce, CryptoError> {
    return randomNonce<ClientNonce>();
}
} // namespace extendedPrecache::platform::windows
