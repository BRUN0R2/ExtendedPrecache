#include "applicationChecks.hpp"
#include "binary.hpp"
#include <extendedPrecache/data/application.hpp>

#include <set>

namespace extendedPrecache::data {
namespace {
using namespace detail;
auto valueMemory(const Schema &schema) -> std::uint64_t {
    auto total = multiply(schema.fields.size(), sizeof(FieldValue));
    for (const auto &field : schema.fields) {
        const auto count = field.has(FieldFlag::array) ? field.maxElements : 1U;
        total = add(total, multiply(count, sizeof(Atom)));
        if (field.kind == ValueKind::bytes) {
            total = add(total, multiply(count, field.maxAtomBytes));
        }
    }
    return total;
}
void legacyRange(const MessageDefinition &message, const Schema &schema,
                 const NativeMessageProof &proof, const session::MapRequirements &requirements) {
    require(proof.fields.size() == message.legacyFields.size(), ErrorCode::unsupportedModule);
    std::set<FieldId> seen;
    for (const auto &legacy : message.legacyFields) {
        const auto &field = schema.fields[legacy.field.value];
        if (!field.resourceKind) {
            fail(ErrorCode::invalidSchema);
        }
        const auto kind = *field.resourceKind;
        const auto zeroBased = kind == ResourceKind::generic || kind == ResourceKind::decal;
        require(!zeroBased || !field.has(FieldFlag::nullable), ErrorCode::unsupportedModule);
        const auto count = requirements.vector().wireValues()[kindIndex(kind)];
        require(count > 0 || field.has(FieldFlag::nullable), ErrorCode::capacity);
        const auto maximum = zeroBased && count != 0 ? count - 1 : count;
        const auto minimum = zeroBased || field.has(FieldFlag::nullable) ? 0U : 1U;
        const auto bits = static_cast<unsigned>(legacy.bits);
        const auto encodedMaximum = (std::uint64_t{1} << (bits - (legacy.isSigned ? 1U : 0U))) - 1;
        require(maximum <= encodedMaximum, ErrorCode::capacity);
        const auto found = std::ranges::find(proof.fields, legacy.field, &NativeFieldRange::field);
        require(found != proof.fields.end() && seen.insert(found->field).second &&
                    found->minimum <= minimum && found->maximum >= maximum,
                ErrorCode::unsupportedModule);
    }
}
auto dataOpcode(Direction direction, std::uint8_t opcode) -> bool {
    constexpr auto server =
        std::array<std::uint8_t, 12>{3, 6, 14, 15, 21, 22, 23, 29, 36, 40, 41, 43};
    return direction == Direction::clientToServer
               ? opcode == 7
               : std::ranges::find(server, opcode) != server.end();
}
void serviceCoverage(const ApplicationContract &contract, const ApplicationBindings &bindings,
                     std::uint32_t reliableBytes) {
    for (const auto direction : {Direction::serverToClient, Direction::clientToServer}) {
        const auto count = direction == Direction::serverToClient ? 64U : 256U;
        for (unsigned opcode = 0; opcode < count; ++opcode) {
            const auto *service = contract.service(direction, static_cast<std::uint8_t>(opcode));
            require(service != nullptr, ErrorCode::unsupportedModule);
            const auto data = dataOpcode(direction, service->opcode);
            require((service->mode == ServiceMode::data2) == data, ErrorCode::unsupportedContract);
            if (direction == Direction::serverToClient &&
                (opcode == 0 || opcode == 7 || opcode == 20 || opcode >= 60)) {
                require(service->mode == ServiceMode::forbidden, ErrorCode::unsupportedModule);
            }
            if (direction == Direction::clientToServer && (opcode == 0 || opcode >= 12)) {
                require(service->mode == ServiceMode::forbidden, ErrorCode::unsupportedModule);
            }
            if (service->mode == ServiceMode::forbidden) {
                continue;
            }
            within(service->maxBytes.value, reliableBytes);
            if (data) {
                require(service->maxBytes.value >= envelopeBytes && !service->phases[0] &&
                            !service->phases[1],
                        ErrorCode::invalidSchema);
                if (direction == Direction::serverToClient) {
                    const auto metadata = opcode == 14 || opcode == 22;
                    const auto both = opcode == 15 || opcode == 43;
                    require(both || (metadata ? !service->phases[3] && service->phases[2]
                                              : !service->phases[2] && service->phases[3]),
                            ErrorCode::invalidSchema);
                }
            }
            get(bindings.approveService(*service));
        }
    }
}
} // namespace
auto preflightApplication(const ApplicationContract &contract, PreflightContext context,
                          const ApplicationBindings &bindings) noexcept -> Result<ApplicationPlan> {
    return detail::protect([&] {
        const auto &app = contract.definition();
        require(!context.installedDataContractHash.isZero() &&
                    context.installedDataContractHash == app.dataContractHash,
                ErrorCode::unsupportedContract);
        require(context.role == session::EndpointRole::client ||
                    context.role == session::EndpointRole::server,
                ErrorCode::invalidSchema);
        require(session::admitRequirements(context.requirements, context.selected).has_value(),
                ErrorCode::capacity);
        require(session::validateChunkBudget(context.selected, {chunkOverheadBytes}).has_value(),
                ErrorCode::byteBudget);
        get(bindings.approveLimits(app.limits, app.objects, context.selected));
        const auto &selected = context.selected.vector();
        const auto &required = context.requirements.vector();
        within(contract.artifactBytes().value, app.limits.contractArtifactBytes.value);
        within(context.installedDataArtifactBytes.value, app.limits.contractArtifactBytes.value);
        within(app.limits.unreliableMessageBytes.value,
               selected.get<session::CapabilityField::reliableMessageBytes>().value);
        require(app.limits.demoBlockBytes.value >=
                    add(selected.get<session::CapabilityField::reliableMessageBytes>().value,
                        demoBlockHeaderBytes + demoPacketHeaderBytes),
                ErrorCode::byteBudget);
        std::array<std::uint64_t, 8> maxWire{};
        std::array<std::uint64_t, 8> maxOwned{};
        std::array<std::uint64_t, 8> maxFields{};
        for (const auto &schema : app.schemas) {
            get(bindings.approveSchema(schema));
            const auto encoded = get(encodeSchema(schema));
            within(add(envelopeBytes, encoded.size()),
                   selected.get<session::CapabilityField::reliableMessageBytes>().value);
            const auto value = get(maxValueBytes(schema));
            const auto role = static_cast<std::size_t>(schema.role) - 1;
            maxWire[role] = std::max(maxWire[role], value);
            maxOwned[role] = std::max(maxOwned[role], valueMemory(schema));
            maxFields[role] = std::max<std::uint64_t>(maxFields[role], schema.fields.size());
            const auto state =
                schema.role == SchemaRole::entity || schema.role == SchemaRole::client ||
                schema.role == SchemaRole::weapon || schema.role == SchemaRole::moduleState;
            const auto header = state ? 16U + (schema.fields.size() + 7U) / 8U
                                      : (schema.role == SchemaRole::eventArgs ? 16U : 8U);
            within(add(envelopeBytes, add(header, value)),
                   selected.get<session::CapabilityField::reliableMessageBytes>().value);
        }
        std::uint64_t stateBytes{};
        std::uint64_t ownedState{};
        std::uint64_t changesOwned{};
        const auto addObjects = [&](std::uint64_t count, SchemaRole role) {
            if (count == 0) {
                return;
            }
            const auto index = static_cast<std::size_t>(role) - 1;
            require(std::ranges::any_of(app.schemas,
                                        [&](const Schema &schema) { return schema.role == role; }),
                    ErrorCode::invalidSchema);
            stateBytes =
                add(stateBytes, multiply(count, add(fullObjectHeaderBytes, maxWire[index])));
            ownedState = add(ownedState, multiply(count, add(sizeof(FullObject), maxOwned[index])));
            changesOwned = add(
                changesOwned,
                multiply(count,
                         add(sizeof(FrameObject),
                             add(maxOwned[index],
                                 multiply(maxFields[index], sizeof(std::optional<FieldValue>))))));
        };
        addObjects(app.objects.entityEntries, SchemaRole::entity);
        addObjects(app.objects.clientEntries, SchemaRole::client);
        addObjects(app.objects.weaponEntries, SchemaRole::weapon);
        addObjects(app.objects.staticEntries, SchemaRole::entity);
        for (const auto &module : app.modules) {
            get(bindings.approveModule(module));
            const auto identity =
                context.role == session::EndpointRole::server ? module.server : module.client;
            if (identity) {
                require(get(bindings.identify(module.id, context.role)) == *identity,
                        ErrorCode::unsupportedModule);
            }
            for (const auto &message : module.messages) {
                const auto *schema = findSchema(app.schemas, message.schema);
                require(schema != nullptr, ErrorCode::invalidSchema);
                const auto proof = get(bindings.approveMessage(module.id, message));
                if (message.wireMode == WireMode::typed2) {
                    const auto bytes =
                        add(envelopeBytes + modMessageHeaderBytes, get(maxValueBytes(*schema)));
                    within(bytes,
                           selected.get<session::CapabilityField::reliableMessageBytes>().value);
                    if (message.deliveries.unreliable && !message.deliveries.reliable) {
                        within(bytes, app.limits.unreliableMessageBytes.value);
                    }
                } else {
                    within(proof.maximumPayload.value, nativeUserMessageBytes);
                    require(message.nativeSize < 0 ||
                                proof.maximumPayload.value ==
                                    static_cast<std::uint32_t>(message.nativeSize),
                            ErrorCode::unsupportedModule);
                    legacyRange(message, *schema, proof, context.requirements);
                }
            }
            for (const auto &schema : app.schemas) {
                if (schema.role == SchemaRole::moduleState &&
                    bindings.allows({ObjectKind::module, {module.id.value}}, schema.id)) {
                    addObjects(1, SchemaRole::moduleState);
                    break;
                }
            }
        }
        require(!app.modules.empty(), ErrorCode::unsupportedModule);
        for (const auto &effect : app.effects) {
            get(bindings.approveEffect(effect));
        }
        serviceCoverage(contract, bindings,
                        selected.get<session::CapabilityField::reliableMessageBytes>().value);
        validateServiceSizes(contract, context.selected);
        within(stateBytes, app.limits.stateFrameBytes.value);
        for (const auto object : bindings.mandatoryObjects()) {
            require(std::ranges::any_of(
                        app.schemas,
                        [&](const Schema &schema) { return bindings.allows(object, schema.id); }),
                    ErrorCode::unsupportedModule);
        }
        auto minimum = add(multiply(app.limits.snapshotWindowFrames + std::uint64_t{1}, ownedState),
                           multiply(2, changesOwned));
        minimum = add(minimum, multiply(2, stateBytes));
        minimum = add(
            minimum, add(required.get<session::CapabilityField::manifestBytes>().value,
                         multiply(required.get<session::CapabilityField::resourceEntries>().value,
                                  sizeof(Resource) + maxNameBytes + sizeof(Asset) + maxNameBytes)));
        minimum = add(minimum,
                      multiply(required.get<session::CapabilityField::consistencyEntries>().value,
                               sizeof(ConsistencyRule) + 64U));
        minimum = add(minimum, selected.get<session::CapabilityField::inFlightBytes>().value);
        minimum = add(minimum, multiply(3, add(app.limits.contractArtifactBytes.value,
                                               app.limits.schemaSetBytes.value)));
        minimum = add(minimum, app.limits.demoBootstrapBytes.value);
        minimum = add(minimum, multiply(2, app.limits.demoBlockBytes.value));
        const CoreRequirements core{app.limits.schemaSetBytes, {stateBytes}, {minimum}};
        const auto quote = get(bindings.quotePeak(core));
        require(quote.value >= minimum, ErrorCode::storageUnavailable);
        within(quote.value, context.metadataBudget.value);
        within(quote.value, maxMetadataBytes);
        return ApplicationPlan{core, quote};
    });
}
} // namespace extendedPrecache::data
