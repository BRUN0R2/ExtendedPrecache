#include <extendedPrecache/transport/controlChannel.hpp>

#include <algorithm>

namespace extendedPrecache::transport {
namespace {
using session::RejectCode;
using session::Rejection;

constexpr auto registerUserMessageService = std::byte{39};
constexpr auto variableSizeMarker = std::byte{0xff};
constexpr auto optInValue = std::string_view{"1"};
constexpr auto infoSeparator = '\\';
constexpr auto commandSeparator = ' ';

// Removes and returns the text up to the next separator, like the engine's own info reader.
auto nextInfoField(std::string_view &text) noexcept -> std::string_view {
    const auto end = text.find(infoSeparator);
    const auto field = text.substr(0, end);
    text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    return field;
}
} // namespace

auto encodeRegistration(ControlMessageId id) noexcept -> ControlBytes {
    auto message = ControlBytes{};
    message.storage[0] = registerUserMessageService;
    message.storage[1] = std::byte{id.value()};
    message.storage[2] = variableSizeMarker;
    std::ranges::transform(session::controlMessageName, message.storage.begin() + 3,
                           [](char value) { return static_cast<std::byte>(value); });
    message.length = 3 + registrationNameBytes;
    return message;
}

auto encodeFrameMessage(ControlMessageId id, const session::EncodedFrame &frame) noexcept
    -> ControlBytes {
    const auto payload = frame.bytes();
    auto message = ControlBytes{};
    message.storage[0] = std::byte{id.value()};
    message.storage[1] = static_cast<std::byte>(payload.size());
    std::ranges::copy(payload, message.storage.begin() + 2);
    message.length = 2 + payload.size();
    return message;
}

auto isControlCommand(std::string_view text) noexcept -> bool {
    if (!text.starts_with(session::controlCommandName)) {
        return false;
    }
    const auto remainder = text.substr(session::controlCommandName.size());
    return remainder.empty() || remainder.front() == commandSeparator;
}

auto parseClientOptIn(std::string_view userinfo) noexcept -> std::expected<ClientOptIn, Rejection> {
    if (userinfo.starts_with(infoSeparator)) {
        userinfo.remove_prefix(1);
    }
    auto found = std::optional<std::string_view>{};
    while (!userinfo.empty()) {
        const auto key = nextInfoField(userinfo);
        const auto value = nextInfoField(userinfo);
        if (key != clientOptInKey) {
            continue;
        }
        if (found) {
            return std::unexpected{Rejection{RejectCode::malformedFrame}};
        }
        found = value;
    }
    if (!found) {
        return ClientOptIn::legacy;
    }
    if (*found != optInValue) {
        return std::unexpected{Rejection{RejectCode::unsupportedVersion}};
    }
    return ClientOptIn::negotiation;
}

} // namespace extendedPrecache::transport
