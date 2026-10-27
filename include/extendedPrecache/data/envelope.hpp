#pragma once
#include <extendedPrecache/data/types.hpp>

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
struct Route final {
    Direction direction;
    std::uint8_t opcode;
    std::uint8_t kind;
    Delivery delivery;
    auto operator==(const Route &) const -> bool = default;
};
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
