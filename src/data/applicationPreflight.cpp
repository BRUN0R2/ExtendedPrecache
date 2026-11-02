#include "applicationChecks.hpp"
#include "binary.hpp"
#include <extendedPrecache/data/application.hpp>

#include <set>
#include <utility>

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
constexpr auto badOpcode = 0U;
// Native opcodes the data plane replaces or never emits, and those no audited table defines.
constexpr auto suppressedServerOpcodes = std::array{badOpcode, 7U, 20U};
constexpr auto firstReservedServerOpcode = 60U;
constexpr auto firstReservedClientOpcode = 12U;
auto dataOpcode(Direction direction, std::uint8_t opcode) -> bool {
    constexpr auto server = std::array{
        Service::event,         Service::sound,        Service::schema,     Service::clientData,
        Service::reliableEvent, Service::initialFrame, Service::effect,     Service::staticSound,
        Service::decalName,     Service::fullFrame,    Service::deltaFrame, Service::resource};
    return direction == Direction::clientToServer
               ? opcode == clientDataOpcode
               : std::ranges::find(server, static_cast<Service>(opcode)) != server.end();
}
// Each record kind needs a live phase, so a service missing one of its phases would fail
// only after the offer, when it first tried to send.
void dataServicePhases(const ServicePolicy &service) {
    const auto enabled = [&](Phase phase) {
        return service.phases[static_cast<std::size_t>(phase)];
    };
    const auto server = service.direction == Direction::serverToClient;
    const auto metadata = server && (service.opcode == std::to_underlying(Service::schema) ||
                                     service.opcode == std::to_underlying(Service::initialFrame));
    const auto twoPhases = !server || service.opcode == std::to_underlying(Service::clientData) ||
                           service.opcode == std::to_underlying(Service::resource);
    require(!enabled(Phase::negotiation) && !enabled(Phase::bootstrap) &&
                enabled(Phase::loading) == (metadata || twoPhases) &&
                enabled(Phase::gameplay) == !metadata,
            ErrorCode::invalidSchema);
}
void serviceCoverage(const ApplicationContract &contract, const ApplicationBindings &bindings,
                     std::uint32_t reliableBytes) {
    for (const auto direction : {Direction::serverToClient, Direction::clientToServer}) {
        const auto count =
            direction == Direction::serverToClient ? serverOpcodeCount : clientOpcodeCount;
        for (unsigned opcode = 0; opcode < count; ++opcode) {
            const auto *service = contract.service(direction, static_cast<std::uint8_t>(opcode));
            require(service != nullptr, ErrorCode::unsupportedModule);
            const auto data = dataOpcode(direction, service->opcode);
            require((service->mode == ServiceMode::data2) == data, ErrorCode::unsupportedContract);
            if (direction == Direction::serverToClient &&
                (std::ranges::find(suppressedServerOpcodes, opcode) !=
                     suppressedServerOpcodes.end() ||
                 opcode >= firstReservedServerOpcode)) {
                require(service->mode == ServiceMode::forbidden, ErrorCode::unsupportedModule);
            }
            if (direction == Direction::clientToServer &&
                (opcode == badOpcode || opcode >= firstReservedClientOpcode)) {
                require(service->mode == ServiceMode::forbidden, ErrorCode::unsupportedModule);
            }
            if (service->mode == ServiceMode::forbidden) {
                continue;
            }
            within(service->maxBytes.value, reliableBytes);
            if (data) {
                require(service->maxBytes.value >= envelopeBytes, ErrorCode::invalidSchema);
                dataServicePhases(*service);
            }
            get(bindings.approveService(*service));
        }
    }
}
constexpr auto schemaRoleCount = static_cast<std::size_t>(SchemaRole::localReplay);
constexpr auto roleIndex(SchemaRole role) noexcept -> std::size_t {
    return static_cast<std::size_t>(role) - 1;
}
// The largest value any schema of a role allows; every object of that role is sized by it.
struct RolePeak final {
    std::uint64_t wireBytes{};
    std::uint64_t ownedBytes{};
    std::uint64_t fields{};
};
using RolePeaks = std::array<RolePeak, schemaRoleCount>;
void checkInstallation(const ApplicationDefinition &app, const PreflightContext &context) {
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
}
void checkArtifactLimits(const ApplicationContract &contract, const PreflightContext &context,
                         std::uint32_t reliableBytes) {
    const auto &limits = contract.definition().limits;
    within(contract.artifactBytes().value, limits.contractArtifactBytes.value);
    within(context.installedDataArtifactBytes.value, limits.contractArtifactBytes.value);
    within(limits.unreliableMessageBytes.value, reliableBytes);
    require(limits.demoBlockBytes.value >=
                add(reliableBytes, demoBlockHeaderBytes + demoPacketHeaderBytes),
            ErrorCode::byteBudget);
}
auto recordHeaderBytes(const Schema &schema) -> std::uint64_t {
    const auto state = schema.role == SchemaRole::entity || schema.role == SchemaRole::client ||
                       schema.role == SchemaRole::weapon || schema.role == SchemaRole::moduleState;
    return state ? 16U + (schema.fields.size() + 7U) / 8U
                 : (schema.role == SchemaRole::eventArgs ? 16U : 8U);
}
auto approveSchemas(const ApplicationDefinition &app, const ApplicationBindings &bindings,
                    std::uint32_t reliableBytes) -> RolePeaks {
    RolePeaks peaks{};
    for (const auto &schema : app.schemas) {
        get(bindings.approveSchema(schema));
        const auto encoded = get(encodeSchema(schema));
        within(add(envelopeBytes, encoded.size()), reliableBytes);
        const auto value = get(maxValueBytes(schema));
        auto &peak = peaks[roleIndex(schema.role)];
        peak.wireBytes = std::max(peak.wireBytes, value);
        peak.ownedBytes = std::max(peak.ownedBytes, valueMemory(schema));
        peak.fields = std::max<std::uint64_t>(peak.fields, schema.fields.size());
        within(add(envelopeBytes, add(recordHeaderBytes(schema), value)), reliableBytes);
    }
    return peaks;
}
// What every configured object can occupy at once, on the wire and in owned memory.
struct StateBudget final {
    const ApplicationDefinition &app;
    const RolePeaks &peaks;
    std::uint64_t wireBytes{};
    std::uint64_t ownedBytes{};
    std::uint64_t changesOwnedBytes{};
    void addObjects(std::uint64_t count, SchemaRole role) {
        if (count == 0) {
            return;
        }
        const auto &peak = peaks[roleIndex(role)];
        require(std::ranges::any_of(app.schemas,
                                    [&](const Schema &schema) { return schema.role == role; }),
                ErrorCode::invalidSchema);
        wireBytes = add(wireBytes, multiply(count, add(fullObjectHeaderBytes, peak.wireBytes)));
        ownedBytes = add(ownedBytes, multiply(count, add(sizeof(FullObject), peak.ownedBytes)));
        changesOwnedBytes = add(
            changesOwnedBytes,
            multiply(count, add(sizeof(FrameObject),
                                add(peak.ownedBytes,
                                    multiply(peak.fields, sizeof(std::optional<FieldValue>))))));
    }
};
void checkTypedMessage(const MessageDefinition &message, const Schema &schema,
                       const ApplicationLimits &limits, std::uint32_t reliableBytes) {
    const auto bytes = add(envelopeBytes + modMessageHeaderBytes, get(maxValueBytes(schema)));
    within(bytes, reliableBytes);
    if (message.deliveries.unreliable && !message.deliveries.reliable) {
        within(bytes, limits.unreliableMessageBytes.value);
    }
}
void checkNativeMessage(const MessageDefinition &message, const Schema &schema,
                        const NativeMessageProof &proof,
                        const session::MapRequirements &requirements) {
    within(proof.maximumPayload.value, nativeUserMessageBytes);
    require(message.nativeSize < 0 ||
                proof.maximumPayload.value == static_cast<std::uint32_t>(message.nativeSize),
            ErrorCode::unsupportedModule);
    legacyRange(message, schema, proof, requirements);
}
void approveMessages(const ModuleProfile &module, const ApplicationDefinition &app,
                     const PreflightContext &context, const ApplicationBindings &bindings) {
    const auto reliableBytes =
        context.selected.vector().get<session::CapabilityField::reliableMessageBytes>().value;
    for (const auto &message : module.messages) {
        const auto *schema = findSchema(app.schemas, message.schema);
        require(schema != nullptr, ErrorCode::invalidSchema);
        const auto proof = get(bindings.approveMessage(module.id, message));
        if (message.wireMode == WireMode::typed2) {
            checkTypedMessage(message, *schema, app.limits, reliableBytes);
        } else {
            checkNativeMessage(message, *schema, proof, context.requirements);
        }
    }
}
void addModuleState(const ModuleProfile &module, const ApplicationBindings &bindings,
                    StateBudget &state) {
    for (const auto &schema : state.app.schemas) {
        if (schema.role == SchemaRole::moduleState &&
            bindings.allows({ObjectKind::module, {module.id.value}}, schema.id)) {
            state.addObjects(1, SchemaRole::moduleState);
            break;
        }
    }
}
void approveModules(const PreflightContext &context, const ApplicationBindings &bindings,
                    StateBudget &state) {
    for (const auto &module : state.app.modules) {
        get(bindings.approveModule(module));
        const auto identity =
            context.role == session::EndpointRole::server ? module.server : module.client;
        if (identity) {
            require(get(bindings.identify(module.id, context.role)) == *identity,
                    ErrorCode::unsupportedModule);
        }
        approveMessages(module, state.app, context, bindings);
        addModuleState(module, bindings, state);
    }
}
void checkMandatoryObjects(const ApplicationDefinition &app, const ApplicationBindings &bindings) {
    for (const auto object : bindings.mandatoryObjects()) {
        require(std::ranges::any_of(
                    app.schemas,
                    [&](const Schema &schema) { return bindings.allows(object, schema.id); }),
                ErrorCode::unsupportedModule);
    }
}
auto minimumOwnedBytes(const ApplicationDefinition &app, const PreflightContext &context,
                       const StateBudget &state) -> std::uint64_t {
    const auto &selected = context.selected.vector();
    const auto &required = context.requirements.vector();
    auto minimum =
        add(multiply(app.limits.snapshotWindowFrames.value + std::uint64_t{1}, state.ownedBytes),
            multiply(2, state.changesOwnedBytes));
    minimum = add(minimum, multiply(2, state.wireBytes));
    minimum =
        add(minimum, add(required.get<session::CapabilityField::manifestBytes>().value,
                         multiply(required.get<session::CapabilityField::resourceEntries>().value,
                                  sizeof(Resource) + maxNameBytes + sizeof(Asset) + maxNameBytes)));
    minimum =
        add(minimum, multiply(required.get<session::CapabilityField::consistencyEntries>().value,
                              sizeof(ConsistencyRule) + 64U));
    minimum = add(minimum, selected.get<session::CapabilityField::inFlightBytes>().value);
    minimum = add(minimum, multiply(3, add(app.limits.contractArtifactBytes.value,
                                           app.limits.schemaSetBytes.value)));
    minimum = add(minimum, app.limits.demoBootstrapBytes.value);
    return add(minimum, multiply(2, app.limits.demoBlockBytes.value));
}
} // namespace
auto preflightApplication(const ApplicationContract &contract, PreflightContext context,
                          const ApplicationBindings &bindings) noexcept -> Result<ApplicationPlan> {
    return detail::protect([&] {
        const auto &app = contract.definition();
        checkInstallation(app, context);
        get(bindings.approveLimits(app.limits, app.objects, context.selected));
        const auto reliableBytes =
            context.selected.vector().get<session::CapabilityField::reliableMessageBytes>().value;
        checkArtifactLimits(contract, context, reliableBytes);
        const auto peaks = approveSchemas(app, bindings, reliableBytes);
        StateBudget state{app, peaks};
        state.addObjects(app.objects.entityEntries.value, SchemaRole::entity);
        state.addObjects(app.objects.clientEntries.value, SchemaRole::client);
        state.addObjects(app.objects.weaponEntries.value, SchemaRole::weapon);
        state.addObjects(app.objects.staticEntries.value, SchemaRole::entity);
        approveModules(context, bindings, state);
        require(!app.modules.empty(), ErrorCode::unsupportedModule);
        for (const auto &effect : app.effects) {
            get(bindings.approveEffect(effect));
        }
        serviceCoverage(contract, bindings, reliableBytes);
        validateServiceSizes(contract, context.selected);
        within(state.wireBytes, app.limits.stateFrameBytes.value);
        checkMandatoryObjects(app, bindings);
        const auto minimum = minimumOwnedBytes(app, context, state);
        const CoreRequirements core{app.limits.schemaSetBytes, {state.wireBytes}, {minimum}};
        const auto quote = get(bindings.quotePeak(core));
        require(quote.value >= minimum, ErrorCode::storageUnavailable);
        within(quote.value, context.metadataBudget.value);
        within(quote.value, maxMetadataBytes);
        return ApplicationPlan{core, quote};
    });
}
} // namespace extendedPrecache::data
