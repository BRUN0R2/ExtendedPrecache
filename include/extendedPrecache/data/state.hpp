#pragma once
#include <extendedPrecache/data/catalog.hpp>
#include <extendedPrecache/data/envelope.hpp>

#include <deque>

namespace extendedPrecache::data {
enum class ObjectKind : std::uint8_t { entity = 1, client, weapon, module, staticObject };
enum class Operation : std::uint8_t { replace, patch, remove };
struct ObjectKey final {
    ObjectKind kind{ObjectKind::entity};
    ObjectId id;
    auto operator<=>(const ObjectKey &) const = default;
};
struct FrameBegin final {
    StateFrameId id;
    StateFrameId base;
    std::uint32_t changeCount{};
    ByteCount changeBytes;
    float serverTime{};
    auto operator==(const FrameBegin &) const -> bool = default;
};
struct FrameObject final {
    StateFrameId frame;
    ObjectKey key;
    Operation operation;
    SchemaId schema;
    std::vector<std::optional<FieldValue>> fields;
    auto operator==(const FrameObject &) const -> bool = default;
};
struct FrameEnd final {
    StateFrameId id;
    std::uint32_t changeCount{};
    Digest snapshotHash;
    auto operator==(const FrameEnd &) const -> bool = default;
};
struct FrameAck final {
    StateFrameId id;
    Digest snapshotHash;
    auto operator==(const FrameAck &) const -> bool = default;
};
struct FullObject final {
    ObjectKey key;
    SchemaId schema;
    ValueSet values;
    auto operator==(const FullObject &) const -> bool = default;
};
class ObjectBindings {
  public:
    virtual ~ObjectBindings() = default;
    [[nodiscard]] virtual auto allows(ObjectKey object, SchemaId schema) const noexcept -> bool = 0;
    [[nodiscard]] virtual auto mandatoryObjects() const noexcept -> std::span<const ObjectKey> = 0;
};
struct StateContext final {
    Catalog catalog;
    std::span<const Schema> schemas;
    const ObjectBindings &bindings;
    ObjectLimits objects;
    MapEpoch epoch;
    ByteCount frameBytes;
    RecordBudget records;
};
struct StateSnapshot final {
    FrameBegin begin;
    MapEpoch epoch;
    Digest catalogHash;
    Digest schemaHash;
    Digest digest;
    std::vector<FullObject> objects;
};
[[nodiscard]] auto encodeFrameBegin(FrameBegin begin) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeFrameBegin(std::span<const std::byte> bytes) noexcept
    -> Result<FrameBegin>;
[[nodiscard]] auto encodeFrameObject(const FrameObject &object,
                                     const StateContext &context) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeFrameObject(std::span<const std::byte> bytes,
                                     const StateContext &context) noexcept -> Result<FrameObject>;
[[nodiscard]] auto encodeFrameEnd(FrameEnd end) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeFrameEnd(std::span<const std::byte> bytes) noexcept -> Result<FrameEnd>;
[[nodiscard]] auto encodeFrameAck(FrameAck ack) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeFrameAck(std::span<const std::byte> bytes) noexcept -> Result<FrameAck>;
[[nodiscard]] auto reconstructState(FrameBegin begin, std::span<const FrameObject> changes,
                                    const StateSnapshot *base, const StateContext &context,
                                    const DigestProvider &provider) noexcept
    -> Result<StateSnapshot>;

enum class FrameDisposition : std::uint8_t { staged, applied, obsolete, needsRecovery };
class FrameReceiver final {
  public:
    FrameReceiver(StateContext context, const DigestProvider &provider, std::uint16_t window)
        : contextValue{std::move(context)}, digestProvider{provider}, windowFrames{window} {}
    [[nodiscard]] auto begin(Route route, FrameBegin frame) noexcept -> Result<FrameDisposition>;
    [[nodiscard]] auto object(Route route, const FrameObject &object) noexcept
        -> Result<FrameDisposition>;
    [[nodiscard]] auto end(Route route, FrameEnd frame) noexcept -> Result<FrameDisposition>;
    [[nodiscard]] auto current() const noexcept -> std::shared_ptr<const StateSnapshot>;
    [[nodiscard]] auto recoveryRequest() const noexcept -> std::optional<StateFrameId>;
    void confirmRecoveryQueued() noexcept {
        requestQueued = recoveryNeeded;
    }

  private:
    struct Pending final {
        FrameBegin begin;
        Route route;
        std::vector<FrameObject> changes;
        std::uint64_t bytes{};
        bool discard{};
        bool missingBase{};
        std::uint64_t ownedBytes{};
    };
    auto failure(Failure error) noexcept -> std::unexpected<Failure>;
    StateContext contextValue;
    const DigestProvider &digestProvider;
    std::uint16_t windowFrames;
    std::array<std::optional<Pending>, 2> pending;
    std::deque<std::shared_ptr<const StateSnapshot>> retained;
    bool recoveryNeeded{};
    bool requestQueued{};
    bool failed{};
};

class FrameLedger final {
  public:
    explicit FrameLedger(std::uint16_t window) : windowFrames{window} {}
    [[nodiscard]] auto recordSent(std::shared_ptr<const StateSnapshot> snapshot, Delivery delivery,
                                  bool recovery = false) noexcept -> Result<void>;
    [[nodiscard]] auto acknowledge(FrameAck ack) noexcept -> Result<bool>;
    [[nodiscard]] auto base() const noexcept -> std::shared_ptr<const StateSnapshot>;
    [[nodiscard]] auto recovering() const noexcept -> bool {
        return recoveryFrame.has_value();
    }

  private:
    struct Sent final {
        std::shared_ptr<const StateSnapshot> snapshot;
        bool acknowledged{};
    };
    std::uint16_t windowFrames;
    StateFrameId lastSent{};
    StateFrameId retiredThrough{};
    std::optional<FrameAck> recoveryFrame;
    std::deque<Sent> retained;
};
} // namespace extendedPrecache::data
