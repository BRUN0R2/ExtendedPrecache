#include <extendedPrecache/platform/windows/cngDigest.hpp>

#include <windows.h>

#include <bcrypt.h>
#include <limits>
#include <new>

namespace extendedPrecache::platform::windows {
namespace {
using namespace session;
class CngStream final : public DigestStream {
  public:
    CngStream() = default;
    CngStream(const CngStream &) = delete;
    auto operator=(const CngStream &) -> CngStream & = delete;
    ~CngStream() override {
        if (handle != nullptr) {
            BCryptDestroyHash(handle);
        }
    }
    auto initialize() noexcept -> bool {
        BCRYPT_HASH_HANDLE created{};
        if (BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &created, nullptr, 0, nullptr, 0, 0) < 0) {
            return false;
        }
        handle = created;
        return true;
    }
    auto update(std::span<const std::byte> input) noexcept
        -> std::expected<void, CryptoError> override {
        if (terminal || handle == nullptr) {
            return std::unexpected{CryptoError::providerFailure};
        }
        if (input.size() > std::numeric_limits<ULONG>::max()) {
            return std::unexpected{CryptoError::inputTooLarge};
        }
        if (input.empty()) {
            return {};
        }
        // CNG declares a mutable pointer for read-only hash input.
        auto *bytes = const_cast<PUCHAR>(reinterpret_cast<const UCHAR *>(input.data()));
        if (BCryptHashData(handle, bytes, static_cast<ULONG>(input.size()), 0) < 0) {
            terminal = true;
            return std::unexpected{CryptoError::providerFailure};
        }
        return {};
    }
    auto finish() noexcept -> std::expected<Digest, CryptoError> override {
        if (terminal || handle == nullptr) {
            return std::unexpected{CryptoError::providerFailure};
        }
        terminal = true;
        Digest result;
        if (BCryptFinishHash(handle, reinterpret_cast<PUCHAR>(result.bytes.data()),
                             static_cast<ULONG>(result.bytes.size()), 0) < 0) {
            return std::unexpected{CryptoError::providerFailure};
        }
        return result;
    }

  private:
    BCRYPT_HASH_HANDLE handle{};
    bool terminal{};
};
} // namespace
auto CngDigest::beginSha256() const noexcept
    -> std::expected<std::unique_ptr<session::DigestStream>, session::CryptoError> {
    auto stream = std::unique_ptr<CngStream>{new (std::nothrow) CngStream};
    if (!stream || !stream->initialize()) {
        return std::unexpected{session::CryptoError::providerFailure};
    }
    return stream;
}
} // namespace extendedPrecache::platform::windows
