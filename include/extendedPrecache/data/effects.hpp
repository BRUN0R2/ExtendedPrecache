#pragma once
#include <extendedPrecache/data/application.hpp>

namespace extendedPrecache::data {
struct EffectField final {
    std::string_view name;
    ValueKind kind;
    std::optional<ResourceKind> resourceKind;
    std::uint16_t allowedTypes{};
    bool nullable{};
    std::optional<std::uint16_t> allowedMask;
};
struct BuiltinEffect final {
    EffectId id;
    std::string_view name;
    std::span<const EffectField> fields;
    ByteCount valuesBytes;
};
[[nodiscard]] auto builtinEffect(EffectId id) noexcept -> std::optional<BuiltinEffect>;
[[nodiscard]] auto builtinEffectSchema(EffectId id) noexcept -> Result<Schema>;
class EffectBindings {
  public:
    virtual ~EffectBindings() = default;
    [[nodiscard]] virtual auto bspModel(EntityId entity) const noexcept
        -> Result<ResourceReference> = 0;
    [[nodiscard]] virtual auto validateAuxiliary(EffectId id, const ValueSet &values) const noexcept
        -> Result<void> = 0;
};
struct EffectMessage final {
    EffectId id;
    ValueSet values;
    auto operator==(const EffectMessage &) const -> bool = default;
};
struct EffectContext final {
    const ApplicationContract &application;
    const Catalog &catalog;
    const EffectBindings &bindings;
    std::uint32_t entityEntries;
    RecordBudget budget;
};
[[nodiscard]] auto encodeEffect(const EffectMessage &effect, Delivery delivery, Phase phase,
                                EffectContext context) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeEffect(std::span<const std::byte> bytes, Delivery delivery, Phase phase,
                                EffectContext context) noexcept -> Result<EffectMessage>;
} // namespace extendedPrecache::data
