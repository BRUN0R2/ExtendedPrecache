#include <extendedPrecache/session/sessionTypes.hpp>

namespace extendedPrecache::session {
namespace {
constexpr auto hexAlphabet = std::string_view{"0123456789abcdef"};
auto nibble(char value) noexcept -> std::optional<std::uint32_t> {
    const auto position = hexAlphabet.find(value);
    if (position == std::string_view::npos) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(position);
}
} // namespace

auto parseDigest(std::string_view hex) noexcept -> std::expected<Digest, Rejection> {
    Digest result;
    if (hex.size() != result.bytes.size() * 2) {
        return std::unexpected{Rejection{RejectCode::malformedFrame}};
    }
    for (std::size_t index = 0; index < result.bytes.size(); ++index) {
        const auto high = nibble(hex[index * 2]);
        const auto low = nibble(hex[index * 2 + 1]);
        if (!high || !low) {
            return std::unexpected{Rejection{RejectCode::malformedFrame}};
        }
        result.bytes[index] = static_cast<std::byte>((*high << 4U) | *low);
    }
    return result;
}

auto encodeHex(std::span<const std::byte> bytes) -> std::string {
    std::string result;
    result.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        const auto value = std::to_integer<std::uint8_t>(byte);
        result.push_back(hexAlphabet[value >> 4U]);
        result.push_back(hexAlphabet[value & 0x0fU]);
    }
    return result;
}

auto decodeHex(std::string_view hex) -> std::expected<std::vector<std::byte>, Rejection> {
    if (hex.size() % 2 != 0 || hex.size() > maxTranscriptBytes * 2) {
        return std::unexpected{Rejection{RejectCode::malformedFrame}};
    }
    std::vector<std::byte> result;
    result.reserve(hex.size() / 2);
    for (std::size_t index = 0; index < hex.size(); index += 2) {
        const auto high = nibble(hex[index]);
        const auto low = nibble(hex[index + 1]);
        if (!high || !low) {
            return std::unexpected{Rejection{RejectCode::malformedFrame}};
        }
        result.push_back(static_cast<std::byte>((*high << 4U) | *low));
    }
    return result;
}
} // namespace extendedPrecache::session
