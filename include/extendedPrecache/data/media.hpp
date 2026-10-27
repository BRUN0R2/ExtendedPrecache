#pragma once
#include <extendedPrecache/data/application.hpp>

namespace extendedPrecache::data {
using SentenceId = Id<struct SentenceTag, std::uint32_t>;
enum class SoundAction : std::uint8_t { start, stop, update };
using SoundSource = std::variant<std::monostate, ResourceReference, SentenceId>;
struct Sound final {
    EntityId entity;
    std::uint8_t channel{};
    SoundAction action;
    SoundSource source;
    std::uint8_t volume{};
    std::uint8_t attenuationQ6{};
    std::uint8_t pitch{};
    Coords origin{};
    bool spawning{};
    auto operator==(const Sound &) const -> bool = default;
};
class SoundBindings {
  public:
    virtual ~SoundBindings() = default;
    [[nodiscard]] virtual auto sentenceExists(SentenceId id) const noexcept -> bool = 0;
    [[nodiscard]] virtual auto canUpdate(EntityId entity, std::uint8_t channel,
                                         const SoundSource &source) const noexcept -> bool = 0;
};
struct SoundContext final {
    const ReferenceValidator &resources;
    const SoundBindings &bindings;
    std::uint32_t entityEntries{maxEntityEntries};
    RecordBudget budget;
};
[[nodiscard]] auto encodeSound(const Sound &sound, Route route, Phase phase,
                               SoundContext context) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeSound(std::span<const std::byte> bytes, Route route, Phase phase,
                               SoundContext context) noexcept -> Result<Sound>;

struct Event final {
    DomainId id;
    EntityId entity;
    std::uint16_t behaviorFlags{};
    std::uint16_t delayTicks{};
    SchemaId argsSchema;
    ValueSet args;
    auto operator==(const Event &) const -> bool = default;
};
class EventBindings {
  public:
    virtual ~EventBindings() = default;
    [[nodiscard]] virtual auto argumentSchema(DomainId event) const noexcept
        -> Result<SchemaId> = 0;
};
struct EventContext final {
    const ApplicationContract &application;
    const ReferenceValidator &resources;
    const EventBindings &bindings;
    std::uint32_t entityEntries{maxEntityEntries};
    RecordBudget budget;
};
[[nodiscard]] auto encodeEvent(const Event &event, Route route, Phase phase,
                               EventContext context) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeEvent(std::span<const std::byte> bytes, Route route, Phase phase,
                               EventContext context) noexcept -> Result<Event>;
struct DecalName final {
    DomainId id;
    std::string name;
    auto operator==(const DecalName &) const -> bool = default;
};
[[nodiscard]] auto encodeDecalName(const DecalName &decal, const Catalog &catalog,
                                   RecordBudget budget = {}) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeDecalName(std::span<const std::byte> bytes, const Catalog &catalog,
                                   RecordBudget budget = {}) noexcept -> Result<DecalName>;
} // namespace extendedPrecache::data
