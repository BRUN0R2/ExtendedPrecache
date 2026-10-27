#include "binary.hpp"
#include <extendedPrecache/data/effects.hpp>

namespace extendedPrecache::data {
namespace {
using namespace detail;
#include "effectRegistry.inc"
auto schemaFor(EffectId id, EffectContext context) -> Schema {
    const auto *policy = context.application.effect(id);
    require(policy != nullptr, ErrorCode::unsupportedModule);
    if (policy->wireSchema == EffectSchema::builtin) {
        return get(builtinEffectSchema(id));
    }
    const auto *schema = findSchema(context.application.definition().schemas, policy->schema);
    require(schema && schema->role == SchemaRole::effect, ErrorCode::invalidSchema);
    return *schema;
}
void validateEffect(const EffectMessage &effect, Delivery delivery, Phase phase,
                    EffectContext context) {
    require(phase == Phase::gameplay, ErrorCode::wrongPhase);
    const auto *policy = context.application.effect(effect.id);
    require(policy && policy->deliveries.allows(delivery), ErrorCode::unsupportedModule);
    if (const auto builtin = builtinEffect(effect.id)) {
        require(effect.values.size() == builtin->fields.size());
        for (std::size_t index = 0; index < builtin->fields.size(); ++index) {
            const auto &field = builtin->fields[index];
            require(effect.values[index].elements.size() == 1);
            const auto &atom = effect.values[index].elements.front();
            if (field.resourceKind) {
                const auto *reference = std::get_if<ResourceReference>(&atom);
                require(reference && reference->kind == *field.resourceKind);
                if (reference->id.value == 0) {
                    require(field.nullable);
                } else {
                    const auto *resource = context.catalog.resolve(*reference);
                    require(resource && (field.allowedTypes &
                                         (1U << static_cast<unsigned>(resource->type))) != 0,
                            ErrorCode::capacity);
                }
            }
            if (field.allowedMask) {
                const auto *value = std::get_if<std::uint8_t>(&atom);
                require(value && (static_cast<unsigned>(*value) &
                                  ~static_cast<unsigned>(*field.allowedMask)) == 0);
            }
        }
        if (effect.id.value == 13) {
            const auto *entity = std::get_if<EntityId>(&effect.values[2].elements.front());
            const auto *model = std::get_if<ResourceReference>(&effect.values[3].elements.front());
            require(entity && model);
            require(entity->value == 0 ? model->id.value == 0
                                       : get(context.bindings.bspModel(*entity)) == *model,
                    ErrorCode::capacity);
        }
    }
    get(context.bindings.validateAuxiliary(effect.id, effect.values));
}
} // namespace
auto builtinEffect(EffectId id) noexcept -> std::optional<BuiltinEffect> {
    const auto found = std::ranges::find(builtinEffects, id, &BuiltinEffect::id);
    return found == builtinEffects.end() ? std::nullopt : std::optional{*found};
}
auto builtinEffectSchema(EffectId id) noexcept -> Result<Schema> {
    return detail::protect([&] {
        const auto descriptor = builtinEffect(id);
        require(descriptor.has_value(), ErrorCode::invalidSchema);
        Schema schema{{static_cast<std::uint16_t>(id.value + 1)},
                      SchemaRole::effect,
                      std::string{descriptor->name},
                      {}};
        for (const auto &field : descriptor->fields) {
            constexpr auto widths =
                std::array<std::uint16_t, 14>{1, 1, 2, 2, 4, 4, 4, 0, 4, 2, 2, 2, 6, 12};
            schema.fields.push_back({{small(schema.fields.size())},
                                     field.kind,
                                     field.resourceKind,
                                     static_cast<std::uint16_t>(field.nullable ? 1 : 0),
                                     1,
                                     widths[static_cast<std::size_t>(field.kind) - 1],
                                     std::string{field.name}});
        }
        get(validateSchema(schema));
        return schema;
    });
}
auto encodeEffect(const EffectMessage &effect, Delivery delivery, Phase phase,
                  EffectContext context) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        const auto schema = schemaFor(effect.id, context);
        const auto values = get(encodeValues(
            schema, effect.values, {context.catalog, context.entityEntries}, context.budget));
        validateEffect(effect, delivery, phase, context);
        const auto maxBytes =
            delivery == Delivery::unreliable
                ? std::min(context.budget.messageBytes.value,
                           context.application.definition().limits.unreliableMessageBytes.value)
                : context.budget.messageBytes.value;
        within(add(envelopeBytes + effectHeaderBytes, values.size()), maxBytes);
        Writer writer{context.budget.ownedBytes.value};
        writer.integer(effect.id.value);
        writer.zero(2);
        writer.integer(narrow(values.size()));
        writer.bytes(values);
        return std::move(writer).finish();
    });
}
auto decodeEffect(std::span<const std::byte> bytes, Delivery delivery, Phase phase,
                  EffectContext context) noexcept -> Result<EffectMessage> {
    return detail::protect([&] {
        within(add(envelopeBytes, bytes.size()), context.budget.messageBytes.value);
        if (delivery == Delivery::unreliable) {
            within(add(envelopeBytes, bytes.size()),
                   context.application.definition().limits.unreliableMessageBytes.value);
        }
        Reader reader{bytes};
        const auto id = EffectId{reader.integer<std::uint16_t>()};
        reader.zero(2);
        const auto length = reader.integer<std::uint32_t>();
        const auto values = reader.take(length);
        reader.end();
        auto schema = schemaFor(id, context);
        auto decoded = get(
            decodeValues(schema, values, {context.catalog, context.entityEntries}, context.budget));
        EffectMessage result{id, std::move(decoded)};
        validateEffect(result, delivery, phase, context);
        return result;
    });
}
} // namespace extendedPrecache::data
