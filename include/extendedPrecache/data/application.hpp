#pragma once
#include <extendedPrecache/data/state.hpp>

namespace extendedPrecache::data {
inline constexpr auto defaultApplicationBytes = ByteCount{1048576};
struct ApplicationLimits final {
    ByteCount schemaSetBytes;
    ByteCount contractArtifactBytes;
    ByteCount stateFrameBytes;
    std::uint16_t snapshotWindowFrames{};
    ByteCount unreliableMessageBytes;
    session::DurationMs loadTimeout;
    session::DurationMs consistencyTimeout;
    FileOffset demoFileBytes;
    ByteCount demoBlockBytes;
    ByteCount demoBootstrapBytes;
    TimeMicros demoDuration;
};
struct BindingIdentity final {
    Digest binary;
    Digest binding;
    auto operator==(const BindingIdentity &) const -> bool = default;
};
enum class WireMode : std::uint8_t { typed2, nativeExact };
enum class ResourcePolicy : std::uint8_t { typedReferences, resourceFree, boundedLegacy };
enum class ReplayPolicy : std::uint8_t { wireStateOnly, typedLocalState };
enum class ServiceMode : std::uint8_t { data2, nativeExact, forbidden };
enum class EffectSchema : std::uint8_t { builtin, application };
struct Deliveries final {
    bool reliable{};
    bool unreliable{};
    [[nodiscard]] auto allows(Delivery value) const noexcept -> bool {
        return value == Delivery::reliable ? reliable : value == Delivery::unreliable && unreliable;
    }
};
struct LegacyField final {
    FieldId field;
    std::uint8_t bits{};
    bool isSigned{};
};
struct MessageDefinition final {
    ModMessageId id;
    std::string name;
    Direction direction;
    Deliveries deliveries;
    SchemaId schema;
    WireMode wireMode;
    std::int16_t nativeSize{-1};
    ResourcePolicy resources;
    std::vector<LegacyField> legacyFields;
};
struct LocalReplayBinding final {
    std::uint16_t recordKind{};
    SchemaId schema;
};
struct ModuleProfile final {
    ModuleId id;
    std::string name;
    std::optional<BindingIdentity> server;
    std::optional<BindingIdentity> client;
    ReplayPolicy replay;
    std::vector<MessageDefinition> messages;
    std::vector<LocalReplayBinding> localReplay;
};
struct EffectPolicy final {
    EffectId id;
    ModuleId owner;
    Deliveries deliveries;
    EffectSchema wireSchema;
    SchemaId schema;
    ResourcePolicy resources;
};
struct ServicePolicy final {
    Direction direction;
    std::uint8_t opcode{};
    ServiceMode mode;
    std::array<bool, 4> phases{};
    std::optional<Digest> grammar;
    ByteCount maxBytes;
    ModuleId owner;
};
struct ApplicationDefinition final {
    Digest dataContractHash;
    Digest schemaSetHash;
    ApplicationLimits limits;
    ObjectLimits objects;
    std::vector<Schema> schemas;
    std::vector<ModuleProfile> modules;
    std::vector<EffectPolicy> effects;
    std::vector<ServicePolicy> services;
};
class ApplicationContract final {
  public:
    [[nodiscard]] static auto read(std::string_view document, const DigestProvider &provider,
                                   ByteCount maximum = defaultApplicationBytes) noexcept
        -> Result<ApplicationContract>;
    [[nodiscard]] auto definition() const noexcept -> const ApplicationDefinition &;
    [[nodiscard]] auto digest() const noexcept -> Digest;
    [[nodiscard]] auto artifactBytes() const noexcept -> ByteCount;
    [[nodiscard]] auto module(ModuleId id) const noexcept -> const ModuleProfile *;
    [[nodiscard]] auto message(ModuleId owner, ModMessageId id) const noexcept
        -> const MessageDefinition *;
    [[nodiscard]] auto effect(EffectId id) const noexcept -> const EffectPolicy *;
    [[nodiscard]] auto service(Direction direction, std::uint8_t opcode) const noexcept
        -> const ServicePolicy *;
    [[nodiscard]] static constexpr auto nativeAvailable() noexcept -> bool {
        return false;
    }

  private:
    struct Storage;
    explicit ApplicationContract(std::shared_ptr<const Storage> value)
        : storage{std::move(value)} {}
    std::shared_ptr<const Storage> storage;
};
struct NativeFieldRange final {
    FieldId field;
    std::int64_t minimum{};
    std::uint64_t maximum{};
};
struct NativeMessageProof final {
    ByteCount maximumPayload;
    std::vector<NativeFieldRange> fields;
};
struct CoreRequirements final {
    ByteCount schemaBytes;
    FileOffset fullStateBytes;
    MetadataBytes minimumOwnedBytes;
};
// This local authority must identify compiled bindings; none is supplied for an engine.
class ApplicationBindings : public ObjectBindings {
  public:
    [[nodiscard]] virtual auto
    approveLimits(const ApplicationLimits &limits, ObjectLimits objects,
                  const session::CapabilityLimits &selected) const noexcept -> Result<void> = 0;
    [[nodiscard]] virtual auto approveModule(const ModuleProfile &module) const noexcept
        -> Result<void> = 0;
    [[nodiscard]] virtual auto identify(ModuleId module, session::EndpointRole role) const noexcept
        -> Result<BindingIdentity> = 0;
    [[nodiscard]] virtual auto approveSchema(const Schema &schema) const noexcept
        -> Result<void> = 0;
    [[nodiscard]] virtual auto approveMessage(ModuleId module,
                                              const MessageDefinition &message) const noexcept
        -> Result<NativeMessageProof> = 0;
    [[nodiscard]] virtual auto approveService(const ServicePolicy &service) const noexcept
        -> Result<void> = 0;
    [[nodiscard]] virtual auto approveEffect(const EffectPolicy &effect) const noexcept
        -> Result<void> = 0;
    [[nodiscard]] virtual auto quotePeak(CoreRequirements core) const noexcept
        -> Result<MetadataBytes> = 0;
};
struct PreflightContext final {
    const session::MapRequirements &requirements;
    const session::CapabilityLimits &selected;
    Digest installedDataContractHash;
    ByteCount installedDataArtifactBytes;
    MetadataBytes metadataBudget;
    session::EndpointRole role;
};
struct ApplicationPlan final {
    CoreRequirements core;
    MetadataBytes peakBytes;
};
[[nodiscard]] auto preflightApplication(const ApplicationContract &contract,
                                        PreflightContext context,
                                        const ApplicationBindings &bindings) noexcept
    -> Result<ApplicationPlan>;
} // namespace extendedPrecache::data
