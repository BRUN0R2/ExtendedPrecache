#pragma once

#include <extendedPrecache/session/frameCodec.hpp>

namespace extendedPrecache::transport {

inline constexpr std::int32_t firstUserMessageId = 64;
inline constexpr std::int32_t lastUserMessageId = 255;

// The identifier the engine allocated to the control message, never a value chosen in advance.
class ControlMessageId final {
  public:
    [[nodiscard]] static constexpr auto create(std::int32_t allocated) noexcept
        -> std::optional<ControlMessageId> {
        if (allocated < firstUserMessageId || allocated > lastUserMessageId) {
            return std::nullopt;
        }
        return ControlMessageId{static_cast<std::uint8_t>(allocated)};
    }
    [[nodiscard]] constexpr auto value() const noexcept -> std::uint8_t {
        return id;
    }
    auto operator==(const ControlMessageId &) const -> bool = default;

  private:
    explicit constexpr ControlMessageId(std::uint8_t allocated) noexcept : id{allocated} {}
    std::uint8_t id;
};

inline constexpr std::size_t registrationNameBytes = 16;
inline constexpr std::size_t maxControlMessageBytes = 2 + session::maxFrameBytes;

// One whole control message, ready for the reliable channel of the connection.
class ControlBytes final {
  public:
    [[nodiscard]] auto bytes() const noexcept -> std::span<const std::byte> {
        return {storage.data(), length};
    }

  private:
    friend auto encodeRegistration(ControlMessageId id) noexcept -> ControlBytes;
    friend auto encodeFrameMessage(ControlMessageId id, const session::EncodedFrame &frame) noexcept
        -> ControlBytes;
    std::array<std::byte, maxControlMessageBytes> storage{};
    std::size_t length{};
};

[[nodiscard]] auto encodeRegistration(ControlMessageId id) noexcept -> ControlBytes;
[[nodiscard]] auto encodeFrameMessage(ControlMessageId id,
                                      const session::EncodedFrame &frame) noexcept -> ControlBytes;

// True for any text under the reserved token, valid or not: it never reaches the game.
[[nodiscard]] auto isControlCommand(std::string_view text) noexcept -> bool;

inline constexpr auto clientOptInKey = std::string_view{"epc"};
enum class ClientOptIn : std::uint8_t { legacy, negotiation };
[[nodiscard]] auto parseClientOptIn(std::string_view userinfo) noexcept
    -> std::expected<ClientOptIn, session::Rejection>;

} // namespace extendedPrecache::transport
