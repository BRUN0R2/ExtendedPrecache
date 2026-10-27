#include "applicationChecks.hpp"
#include "binary.hpp"
#include <extendedPrecache/data/effects.hpp>

namespace extendedPrecache::data::detail {
void validateServiceSizes(const ApplicationContract &contract,
                          const session::CapabilityLimits &selected) {
    const auto &app = contract.definition();
    std::array<std::uint64_t, 256> server{};
    std::uint64_t client = envelopeBytes + assetsReadyBytes;
    const auto demand = [&](Service service, std::uint64_t body) {
        auto &minimum = server[static_cast<std::uint8_t>(service)];
        minimum = std::max(minimum, add(envelopeBytes, body));
    };
    demand(Service::schema, digestBytes);
    demand(Service::resource, gameStartBytes);
    demand(Service::resource, maxChunkOverheadBytes - envelopeBytes + 1);
    for (const auto service : {Service::initialFrame, Service::fullFrame, Service::deltaFrame}) {
        demand(service, std::max(frameBeginBytes, frameEndBytes));
    }
    for (const auto &schema : app.schemas) {
        demand(Service::schema, get(encodeSchema(schema)).size());
        const auto values = get(maxValueBytes(schema));
        const auto object = add(frameObjectHeaderBytes + (schema.fields.size() + 7U) / 8U, values);
        switch (schema.role) {
        case SchemaRole::entity:
            for (const auto service :
                 {Service::initialFrame, Service::fullFrame, Service::deltaFrame}) {
                demand(service, object);
            }
            break;
        case SchemaRole::client:
        case SchemaRole::weapon:
            demand(Service::clientData, object);
            break;
        case SchemaRole::moduleState:
            demand(Service::resource, object);
            break;
        case SchemaRole::eventArgs:
            demand(Service::event, add(eventHeaderBytes, values));
            demand(Service::reliableEvent, add(eventHeaderBytes, values));
            break;
        default:
            break;
        }
    }
    for (const auto &module : app.modules) {
        for (const auto &message : module.messages) {
            if (message.wireMode != WireMode::typed2) {
                continue;
            }
            const auto *schema = findSchema(app.schemas, message.schema);
            require(schema != nullptr, ErrorCode::invalidSchema);
            const auto body = add(modMessageHeaderBytes, get(maxValueBytes(*schema)));
            if (message.direction == Direction::serverToClient) {
                demand(Service::resource, body);
            } else {
                client = std::max(client, add(envelopeBytes, body));
            }
        }
    }
    for (const auto &policy : app.effects) {
        std::uint64_t values{};
        if (const auto builtin = builtinEffect(policy.id)) {
            values = builtin->valuesBytes.value;
        } else {
            const auto *schema = findSchema(app.schemas, policy.schema);
            require(schema != nullptr, ErrorCode::invalidSchema);
            values = get(maxValueBytes(*schema));
        }
        const auto body = add(effectHeaderBytes, values);
        demand(Service::effect, body);
        if (policy.deliveries.unreliable && !policy.deliveries.reliable) {
            within(add(envelopeBytes, body), app.limits.unreliableMessageBytes.value);
        }
    }
    demand(Service::sound, soundBytes);
    demand(Service::staticSound, soundBytes);
    demand(Service::decalName, decalNameHeaderBytes + maxDecalNameBytes);
    for (const auto service : {Service::event, Service::sound}) {
        within(server[static_cast<std::uint8_t>(service)], app.limits.unreliableMessageBytes.value);
    }
    for (const auto &policy : app.services) {
        if (policy.mode != ServiceMode::data2) {
            continue;
        }
        const auto minimum =
            policy.direction == Direction::serverToClient ? server[policy.opcode] : client;
        within(minimum, policy.maxBytes.value);
        within(minimum,
               selected.vector().get<session::CapabilityField::reliableMessageBytes>().value);
    }
}
} // namespace extendedPrecache::data::detail
