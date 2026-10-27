#include "applicationJson.hpp"
#include <extendedPrecache/data/effects.hpp>

#include <set>

namespace extendedPrecache::data {
namespace detail {
namespace {
auto schemaFor(const ApplicationDefinition &app, SchemaId id, SchemaRole role) -> const Schema & {
    const auto *schema = findSchema(app.schemas, id);
    require(schema && schema->role == role, ErrorCode::invalidSchema);
    return *schema;
}
auto resourceFree(const Schema &schema) -> bool {
    return std::ranges::none_of(schema.fields, [](const FieldDefinition &field) {
        return field.kind == ValueKind::resource;
    });
}
} // namespace
void validateApplicationDefinition(const ApplicationDefinition &app,
                                   const DigestProvider &provider) {
    const auto schemaBytes = get(encodeSchemaSet(app.schemas, app.limits.schemaSetBytes));
    require(hash(schemaBytes, provider) == app.schemaSetHash, ErrorCode::digestMismatch);
    std::set<ModuleId> moduleIds;
    std::set<std::string_view> moduleNames;
    std::set<std::string_view> nativeNames;
    for (const auto &module : app.modules) {
        require(module.id.value != 0 && identifier(module.name) &&
                    moduleIds.insert(module.id).second && moduleNames.insert(module.name).second,
                ErrorCode::invalidSchema);
        require(module.server || module.client, ErrorCode::unsupportedModule);
        std::set<ModMessageId> ids;
        std::set<std::string_view> names;
        for (const auto &message : module.messages) {
            require(module.server && module.client && message.id.value != 0 &&
                        ids.insert(message.id).second && names.insert(message.name).second &&
                        identifier(message.name, 15),
                    ErrorCode::invalidSchema);
            require(message.name != "ExtPrecache" && message.name != "epc1",
                    ErrorCode::unsupportedModule);
            const auto &schema = schemaFor(app, message.schema, SchemaRole::modMessage);
            if (message.wireMode == WireMode::typed2) {
                require(message.nativeSize == -1 &&
                            message.resources == ResourcePolicy::typedReferences &&
                            message.legacyFields.empty(),
                        ErrorCode::invalidSchema);
                require(message.direction != Direction::clientToServer ||
                            (message.deliveries.reliable && !message.deliveries.unreliable),
                        ErrorCode::invalidSchema);
            } else {
                require(message.direction == Direction::serverToClient &&
                            nativeNames.insert(message.name).second &&
                            message.resources != ResourcePolicy::typedReferences,
                        ErrorCode::invalidSchema);
                require(message.resources == ResourcePolicy::resourceFree
                            ? resourceFree(schema) && message.legacyFields.empty()
                            : !message.legacyFields.empty(),
                        ErrorCode::invalidSchema);
                std::set<FieldId> fields;
                for (const auto &field : message.legacyFields) {
                    require(field.field.value < schema.fields.size() &&
                                schema.fields[field.field.value].kind == ValueKind::resource &&
                                fields.insert(field.field).second,
                            ErrorCode::invalidSchema);
                }
                const auto resources =
                    std::ranges::count(schema.fields, ValueKind::resource, &FieldDefinition::kind);
                require(message.resources != ResourcePolicy::boundedLegacy ||
                            static_cast<std::size_t>(resources) == fields.size(),
                        ErrorCode::invalidSchema);
            }
        }
        require(module.replay != ReplayPolicy::wireStateOnly || module.localReplay.empty(),
                ErrorCode::invalidSchema);
        require(module.replay != ReplayPolicy::typedLocalState ||
                    (module.client && !module.localReplay.empty()),
                ErrorCode::invalidSchema);
        std::set<std::pair<std::uint16_t, SchemaId>> local;
        for (const auto &item : module.localReplay) {
            require(local.emplace(item.recordKind, item.schema).second, ErrorCode::invalidSchema);
            schemaFor(app, item.schema,
                      item.recordKind == 6 ? SchemaRole::moduleState : SchemaRole::localReplay);
        }
    }
    std::set<EffectId> effectIds;
    for (const auto &policy : app.effects) {
        require(moduleIds.contains(policy.owner) && effectIds.insert(policy.id).second,
                ErrorCode::invalidSchema);
        const auto builtin = builtinEffect(policy.id);
        if (policy.wireSchema == EffectSchema::builtin) {
            require(builtin.has_value() && policy.schema.value == 0 &&
                        policy.resources == ResourcePolicy::typedReferences,
                    ErrorCode::invalidSchema);
        } else {
            require(!builtin && policy.id.value != 16 && policy.id.value != 26 &&
                        policy.id.value != 117 && policy.id.value != 118 &&
                        policy.resources == ResourcePolicy::resourceFree,
                    ErrorCode::invalidSchema);
            require(resourceFree(schemaFor(app, policy.schema, SchemaRole::effect)),
                    ErrorCode::invalidSchema);
        }
    }
    std::set<std::pair<Direction, std::uint8_t>> services;
    for (const auto &service : app.services) {
        require(moduleIds.contains(service.owner) &&
                    services.emplace(service.direction, service.opcode).second,
                ErrorCode::invalidSchema);
        require(service.direction != Direction::serverToClient || service.opcode < 64,
                ErrorCode::invalidSchema);
        const auto anyPhase = std::ranges::any_of(service.phases, [](bool value) { return value; });
        if (service.mode == ServiceMode::forbidden) {
            require(service.maxBytes.value == 0 && !anyPhase && !service.grammar,
                    ErrorCode::invalidSchema);
        } else {
            require(service.maxBytes.value > 0 && anyPhase &&
                        service.grammar.has_value() == (service.mode == ServiceMode::nativeExact),
                    ErrorCode::invalidSchema);
        }
    }
}
} // namespace detail
struct ApplicationContract::Storage final {
    ApplicationDefinition definition;
    Digest digest;
    ByteCount artifactBytes;
};
auto ApplicationContract::read(std::string_view document, const DigestProvider &provider,
                               ByteCount maximum) noexcept -> Result<ApplicationContract> {
    return detail::protect([&] {
        detail::within(document.size(), maximum.value);
        detail::within(document.size(), maxMetadataBytes);
        detail::require(!document.empty() && document.back() == '\n' &&
                            document.find('\r') == std::string_view::npos &&
                            !document.starts_with("\xef\xbb\xbf"),
                        ErrorCode::invalidSchema);
        auto app = detail::parseApplication(document);
        detail::validateApplicationDefinition(app, provider);
        detail::within(document.size(), app.limits.contractArtifactBytes.value);
        const auto hash =
            detail::hash(std::as_bytes(std::span{document.data(), document.size()}), provider);
        return ApplicationContract{std::make_shared<const Storage>(
            Storage{std::move(app), hash, {detail::narrow(document.size())}})};
    });
}
auto ApplicationContract::definition() const noexcept -> const ApplicationDefinition & {
    return storage->definition;
}
auto ApplicationContract::digest() const noexcept -> Digest {
    return storage->digest;
}
auto ApplicationContract::artifactBytes() const noexcept -> ByteCount {
    return storage->artifactBytes;
}
auto ApplicationContract::module(ModuleId id) const noexcept -> const ModuleProfile * {
    const auto found = std::ranges::find(definition().modules, id, &ModuleProfile::id);
    return found == definition().modules.end() ? nullptr : &*found;
}
auto ApplicationContract::message(ModuleId owner, ModMessageId id) const noexcept
    -> const MessageDefinition * {
    const auto *profile = module(owner);
    if (!profile) {
        return nullptr;
    }
    const auto found = std::ranges::find(profile->messages, id, &MessageDefinition::id);
    return found == profile->messages.end() ? nullptr : &*found;
}
auto ApplicationContract::effect(EffectId id) const noexcept -> const EffectPolicy * {
    const auto found = std::ranges::find(definition().effects, id, &EffectPolicy::id);
    return found == definition().effects.end() ? nullptr : &*found;
}
auto ApplicationContract::service(Direction direction, std::uint8_t opcode) const noexcept
    -> const ServicePolicy * {
    const auto found =
        std::ranges::find_if(definition().services, [&](const ServicePolicy &policy) {
            return policy.direction == direction && policy.opcode == opcode;
        });
    return found == definition().services.end() ? nullptr : &*found;
}
} // namespace extendedPrecache::data
