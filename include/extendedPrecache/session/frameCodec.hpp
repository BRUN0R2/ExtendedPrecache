#pragma once

#include <extendedPrecache/session/capabilities.hpp>
#include <variant>

namespace extendedPrecache::session {

enum class MessageKind : std::uint8_t {
    serverOffer = 1,
    applicationContract,
    dataContract,
    requirementsPage,
    serverLimitsPage,
    offerEnd,
    clientHello,
    offerReference,
    clientLimitsPage,
    clientCapabilitiesEnd,
    selectionPage,
    selectionEnd,
    clientReady,
    serverCommit,
    clientCommitAck,
    reject,
    catalogIdentity
};
inline constexpr std::size_t frameHeaderBytes = 28;
inline constexpr std::size_t maxBodyBytes = 32;
inline constexpr std::size_t maxFrameBytes = frameHeaderBytes + maxBodyBytes;
inline constexpr std::size_t limitsPerPage = 4;
inline constexpr std::size_t pageCount = capabilityCount / limitsPerPage;
using PageOrdinal = Quantity<struct PageOrdinalTag, std::uint8_t>;

struct ServerOfferBody final {
    static constexpr auto kind = MessageKind::serverOffer;
    ConnectionPolicy policy;
    SpawnCount spawn;
    MapEpoch epoch;
    DurationMs timeout;
    DataContractId dataContract;
    auto operator==(const ServerOfferBody &) const -> bool = default;
};
struct ClientHelloBody final {
    static constexpr auto kind = MessageKind::clientHello;
    ConnectionPolicy policy;
    ClientNonce nonce;
    DurationMs timeout;
    auto operator==(const ClientHelloBody &) const -> bool = default;
};
template <MessageKind message> struct DigestBody final {
    static_assert(message == MessageKind::applicationContract ||
                  message == MessageKind::dataContract || message == MessageKind::catalogIdentity ||
                  message == MessageKind::offerEnd || message == MessageKind::offerReference ||
                  message == MessageKind::clientCapabilitiesEnd ||
                  message == MessageKind::selectionEnd || message == MessageKind::clientReady ||
                  message == MessageKind::serverCommit || message == MessageKind::clientCommitAck);
    static constexpr auto kind = message;
    Digest value;
    auto operator==(const DigestBody &) const -> bool = default;
};
template <MessageKind message> struct LimitPageBody final {
    static_assert(message == MessageKind::requirementsPage ||
                  message == MessageKind::serverLimitsPage ||
                  message == MessageKind::clientLimitsPage ||
                  message == MessageKind::selectionPage);
    static constexpr auto kind = message;
    PageOrdinal ordinal;
    std::array<std::uint32_t, limitsPerPage> wireValues;
    auto operator==(const LimitPageBody &) const -> bool = default;
};
struct RejectBody final {
    static constexpr auto kind = MessageKind::reject;
    Rejection value;
    auto operator==(const RejectBody &) const -> bool = default;
};
using FramePayload = std::variant<
    ServerOfferBody, DigestBody<MessageKind::applicationContract>,
    DigestBody<MessageKind::dataContract>, LimitPageBody<MessageKind::requirementsPage>,
    LimitPageBody<MessageKind::serverLimitsPage>, DigestBody<MessageKind::offerEnd>,
    ClientHelloBody, DigestBody<MessageKind::offerReference>,
    LimitPageBody<MessageKind::clientLimitsPage>, DigestBody<MessageKind::clientCapabilitiesEnd>,
    LimitPageBody<MessageKind::selectionPage>, DigestBody<MessageKind::selectionEnd>,
    DigestBody<MessageKind::clientReady>, DigestBody<MessageKind::serverCommit>,
    DigestBody<MessageKind::clientCommitAck>, RejectBody, DigestBody<MessageKind::catalogIdentity>>;
struct Frame final {
    SessionNonce nonce;
    FrameSequence sequence;
    FramePayload payload;
    auto operator==(const Frame &) const -> bool = default;
};
class EncodedFrame final {
  public:
    [[nodiscard]] auto bytes() const noexcept -> std::span<const std::byte> {
        return {storage.data(), length};
    }
    [[nodiscard]] auto operator==(const EncodedFrame &other) const noexcept -> bool {
        return std::ranges::equal(bytes(), other.bytes());
    }

  private:
    friend auto encodeFrame(const Frame &) -> std::expected<EncodedFrame, Rejection>;
    friend class NegotiationSession;
    EncodedFrame() = default;
    std::array<std::byte, maxFrameBytes> storage{};
    std::size_t length{};
};
[[nodiscard]] auto messageKind(const FramePayload &payload) -> MessageKind;
[[nodiscard]] auto encodeFrame(const Frame &frame) -> std::expected<EncodedFrame, Rejection>;
[[nodiscard]] auto decodeFrame(std::span<const std::byte> bytes) noexcept
    -> std::expected<Frame, Rejection>;
[[nodiscard]] auto encodeClientCommand(const EncodedFrame &frame) -> std::string;
[[nodiscard]] auto decodeClientCommand(std::string_view command) -> std::expected<Frame, Rejection>;

} // namespace extendedPrecache::session
