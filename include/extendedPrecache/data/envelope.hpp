#pragma once
#include <extendedPrecache/data/types.hpp>

#include <utility>

namespace extendedPrecache::data {
enum class Service : std::uint8_t {
    event = 3,
    sound = 6,
    schema = 14,
    clientData = 15,
    reliableEvent = 21,
    initialFrame = 22,
    effect = 23,
    staticSound = 29,
    decalName = 36,
    fullFrame = 40,
    deltaFrame = 41,
    resource = 43
};
enum class ClientKind : std::uint8_t {
    consistencyPage = 1,
    consistencyEnd,
    assetsReady,
    gameplayAck,
    frameAck,
    resyncRequest,
    modMessage,
    fatal = 255
};
enum class ResourceMessageKind : std::uint8_t {
    catalogBegin = 1,
    catalogChunk,
    catalogEnd,
    gameStart,
    modMessage,
    moduleState,
    fatal = 255
};
enum class FrameKind : std::uint8_t { begin = 1, object, end };
enum class SchemaKind : std::uint8_t { schema = 1, setEnd };
// Services with one record type use this kind; fatal shares one value across services.
inline constexpr auto singleRecordKind = std::uint8_t{1};
inline constexpr auto fatalKind = std::uint8_t{255};
inline constexpr auto clientDataOpcode = std::uint8_t{7};
inline constexpr auto serverOpcodeCount = 64U;
inline constexpr auto clientOpcodeCount = 256U;
struct Route final {
    Direction direction;
    std::uint8_t opcode;
    std::uint8_t kind;
    Delivery delivery;
    auto operator==(const Route &) const -> bool = default;
};
[[nodiscard]] constexpr auto isService(Route route, Service service) noexcept -> bool {
    return route.opcode == std::to_underlying(service);
}
template <typename Kind>
[[nodiscard]] constexpr auto hasKind(Route route, Kind kind) noexcept -> bool {
    return route.kind == std::to_underlying(kind);
}
template <typename Kind>
[[nodiscard]] constexpr auto kindWithin(Route route, Kind first, Kind last) noexcept -> bool {
    return route.kind >= std::to_underlying(first) && route.kind <= std::to_underlying(last);
}
struct Envelope final {
    Route route;
    SessionNonce nonce;
    MapEpoch epoch;
    session::FrameSequence sequence;
    std::span<const std::byte> body;
};
[[nodiscard]] auto validRoute(Route route) noexcept -> bool;
[[nodiscard]] auto encodeEnvelope(const Envelope &envelope, RecordBudget budget = {}) noexcept
    -> Result<Bytes>;
// The result borrows the input; it never guesses the framing of a native service.
[[nodiscard]] auto decodeEnvelope(std::span<const std::byte> bytes, Direction direction,
                                  RecordBudget budget = {}) noexcept -> Result<Envelope>;
// Delimits the first record of a buffer shared by several; decodeEnvelope still validates it.
[[nodiscard]] auto leadingEnvelopeBytes(std::span<const std::byte> bytes) noexcept
    -> Result<std::uint32_t>;
enum class ReceiveDisposition : std::uint8_t { accept, staleNonce, oldUnreliable };
class SequenceTracker final {
  public:
    SequenceTracker(SessionNonce nonce, MapEpoch epoch,
                    std::array<std::uint32_t, 3> next = {}) noexcept
        : nonceValue{nonce}, epochValue{epoch}, nextValues{next} {}
    [[nodiscard]] auto accept(const Envelope &envelope) noexcept -> Result<ReceiveDisposition>;

  private:
    SessionNonce nonceValue;
    MapEpoch epochValue;
    std::array<std::uint32_t, 3> nextValues;
};
[[nodiscard]] auto encodeFatal(Failure failure) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeFatal(std::span<const std::byte> bytes) noexcept -> Result<Failure>;
} // namespace extendedPrecache::data
