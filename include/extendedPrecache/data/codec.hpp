#pragma once
#include <extendedPrecache/data/consistency.hpp>
#include <extendedPrecache/data/effects.hpp>
#include <extendedPrecache/data/media.hpp>
#include <extendedPrecache/data/modMessages.hpp>
#include <extendedPrecache/data/readiness.hpp>

namespace extendedPrecache::data {
struct SchemaSetEnd final {
    Digest digest;
};
struct CatalogEnd final {
    Digest digest;
};
struct GameplayAck final {
    FrameAck frame;
};
struct ResyncRequest final {
    StateFrameId lastApplied;
};
struct FatalMessage final {
    Failure failure;
};
using DataPayload = std::variant<Schema, SchemaSetEnd, CatalogBegin, ChunkView, CatalogEnd,
                                 GameStart, ModMessage, FrameBegin, FrameObject, FrameEnd, Sound,
                                 Event, EffectMessage, DecalName, ConsistencyPage, ConsistencyEnd,
                                 AssetsReady, GameplayAck, FrameAck, ResyncRequest, FatalMessage>;
struct CodecBindings final {
    const ObjectBindings *objects{};
    const EffectBindings *effects{};
    const SoundBindings *sounds{};
    const EventBindings *events{};
};
struct CodecContext final {
    const ApplicationContract &application;
    const session::MapRequirements &requirements;
    const session::CapabilityLimits &selected;
    const Catalog *catalog{};
    CodecBindings bindings;
    MapEpoch epoch;
    Digest catalogIdentity;
    Phase phase;
    RecordBudget budget;
};
[[nodiscard]] auto authorizeMessage(Route route, const CodecContext &context) noexcept
    -> Result<RecordBudget>;
[[nodiscard]] auto encodeBody(const DataPayload &payload, Route route,
                              const CodecContext &context) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeBody(std::span<const std::byte> bytes, Route route,
                              const CodecContext &context) noexcept -> Result<DataPayload>;
[[nodiscard]] auto encodeMessage(const DataPayload &payload, Envelope header,
                                 const CodecContext &context) noexcept -> Result<Bytes>;
struct DecodedMessage final {
    Envelope header;
    DataPayload payload;
};
[[nodiscard]] auto decodeMessage(std::span<const std::byte> bytes, Direction direction,
                                 const CodecContext &context) noexcept -> Result<DecodedMessage>;

// One logical transport message; every envelope contributes to the same byte limit.
class DataMessageBuilder final {
  public:
    [[nodiscard]] static auto create(Direction direction, Delivery delivery, SessionNonce nonce,
                                     const CodecContext &context) noexcept
        -> Result<DataMessageBuilder>;
    [[nodiscard]] auto append(const DataPayload &payload, Envelope header,
                              const CodecContext &context) noexcept -> Result<void>;
    [[nodiscard]] auto view() const noexcept -> std::span<const std::byte> {
        return bytes;
    }
    [[nodiscard]] auto finish() && -> Bytes {
        return std::move(bytes);
    }

  private:
    DataMessageBuilder(Direction direction, Delivery delivery, SessionNonce nonce, MapEpoch epoch,
                       ByteCount maximum)
        : directionValue{direction}, deliveryValue{delivery}, nonceValue{nonce}, epochValue{epoch},
          maximumBytes{maximum} {}
    Direction directionValue;
    Delivery deliveryValue;
    SessionNonce nonceValue;
    MapEpoch epochValue;
    ByteCount maximumBytes;
    Bytes bytes;
};

class DataReceiver final {
  public:
    [[nodiscard]] static auto create(SessionNonce nonce, MapEpoch epoch,
                                     std::array<std::uint32_t, 3> firstSequence = {}) noexcept
        -> Result<DataReceiver>;
    [[nodiscard]] auto receive(std::span<const std::byte> bytes, Direction direction,
                               const CodecContext &context) noexcept
        -> Result<std::optional<DecodedMessage>>;

  private:
    DataReceiver(SessionNonce nonce, MapEpoch epoch, std::array<std::uint32_t, 3> first)
        : tracker{nonce, epoch, first}, epochValue{epoch} {}
    SequenceTracker tracker;
    MapEpoch epochValue;
    bool failed{};
};
} // namespace extendedPrecache::data
