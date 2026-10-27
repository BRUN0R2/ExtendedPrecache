#include "binary.hpp"
#include <extendedPrecache/data/codec.hpp>

namespace extendedPrecache::data {
namespace {
using namespace detail;
auto isFrame(Route route) -> bool {
    return route.opcode == 22 || route.opcode == 40 || route.opcode == 41;
}
auto catalog(const CodecContext &context) -> const Catalog & {
    require(context.catalog && context.catalog->digest() == context.catalogIdentity &&
                context.catalog->data().schemaSetHash ==
                    context.application.definition().schemaSetHash,
            ErrorCode::wrongPhase);
    return *context.catalog;
}
auto stateContext(const CodecContext &context, RecordBudget budget) -> StateContext {
    require(context.bindings.objects != nullptr, ErrorCode::unsupportedModule);
    return {catalog(context),
            context.application.definition().schemas,
            *context.bindings.objects,
            context.application.definition().objects,
            context.epoch,
            context.application.definition().limits.stateFrameBytes,
            budget};
}
auto effectContext(const CodecContext &context, RecordBudget budget) -> EffectContext {
    require(context.bindings.effects != nullptr, ErrorCode::unsupportedModule);
    return {context.application, catalog(context), *context.bindings.effects,
            context.application.definition().objects.entityEntries, budget};
}
auto soundContext(const CodecContext &context, RecordBudget budget) -> SoundContext {
    require(context.bindings.sounds != nullptr, ErrorCode::unsupportedModule);
    return {catalog(context), *context.bindings.sounds,
            context.application.definition().objects.entityEntries, budget};
}
auto eventContext(const CodecContext &context, RecordBudget budget) -> EventContext {
    require(context.bindings.events != nullptr, ErrorCode::unsupportedModule);
    return {context.application, catalog(context), *context.bindings.events,
            context.application.definition().objects.entityEntries, budget};
}
auto modContext(const CodecContext &context, Route route, RecordBudget budget) -> ModContext {
    return {context.application,
            {catalog(context), context.application.definition().objects.entityEntries},
            route.direction,
            route.delivery,
            context.phase,
            budget};
}
template <typename T> auto matches(const T &payload, Route route) -> bool {
    const auto server = route.direction == Direction::serverToClient;
    const auto resource = server && route.opcode == 43;
    if constexpr (std::is_same_v<T, Schema>) {
        return server && route.opcode == 14 && route.kind == 1;
    } else if constexpr (std::is_same_v<T, SchemaSetEnd>) {
        return server && route.opcode == 14 && route.kind == 2;
    } else if constexpr (std::is_same_v<T, CatalogBegin>) {
        return resource && route.kind == 1;
    } else if constexpr (std::is_same_v<T, ChunkView>) {
        return resource && route.kind == 2;
    } else if constexpr (std::is_same_v<T, CatalogEnd>) {
        return resource && route.kind == 3;
    } else if constexpr (std::is_same_v<T, GameStart>) {
        return resource && route.kind == 4;
    } else if constexpr (std::is_same_v<T, ModMessage>) {
        return server ? resource && route.kind == 5 : route.opcode == 7 && route.kind == 7;
    } else if constexpr (std::is_same_v<T, FrameBegin>) {
        return server && isFrame(route) && route.kind == 1 &&
               ((payload.base.value != 0) == (route.opcode == 41));
    } else if constexpr (std::is_same_v<T, FrameEnd>) {
        return server && isFrame(route) && route.kind == 3;
    } else if constexpr (std::is_same_v<T, FrameObject>) {
        if (payload.key.kind == ObjectKind::client || payload.key.kind == ObjectKind::weapon) {
            return server && route.opcode == 15 && route.kind == 2;
        }
        if (payload.key.kind == ObjectKind::module) {
            return resource && route.kind == 6;
        }
        return server && isFrame(route) && route.kind == 2;
    } else if constexpr (std::is_same_v<T, Sound>) {
        return server && (route.opcode == 6 || route.opcode == 29) && route.kind == 1;
    } else if constexpr (std::is_same_v<T, Event>) {
        return server && (route.opcode == 3 || route.opcode == 21) && route.kind == 1;
    } else if constexpr (std::is_same_v<T, EffectMessage>) {
        return server && route.opcode == 23 && route.kind == 1;
    } else if constexpr (std::is_same_v<T, DecalName>) {
        return server && route.opcode == 36 && route.kind == 1;
    } else if constexpr (std::is_same_v<T, ConsistencyPage>) {
        return !server && route.opcode == 7 && route.kind == 1;
    } else if constexpr (std::is_same_v<T, ConsistencyEnd>) {
        return !server && route.opcode == 7 && route.kind == 2;
    } else if constexpr (std::is_same_v<T, AssetsReady>) {
        return !server && route.opcode == 7 && route.kind == 3;
    } else if constexpr (std::is_same_v<T, GameplayAck>) {
        return !server && route.opcode == 7 && route.kind == 4;
    } else if constexpr (std::is_same_v<T, FrameAck>) {
        return !server && route.opcode == 7 && route.kind == 5;
    } else if constexpr (std::is_same_v<T, ResyncRequest>) {
        return !server && route.opcode == 7 && route.kind == 6;
    } else {
        return route.kind == 255 && (resource || (!server && route.opcode == 7));
    }
}
void semantic(const Schema &schema, const CodecContext &context) {
    const auto *approved = findSchema(context.application.definition().schemas, schema.id);
    require(approved && *approved == schema, ErrorCode::invalidSchema);
}
void semantic(SchemaSetEnd end, const CodecContext &context) {
    require(end.digest == context.application.definition().schemaSetHash,
            ErrorCode::digestMismatch);
}
void semantic(CatalogEnd end, const CodecContext &context) {
    require(end.digest == context.catalogIdentity, ErrorCode::digestMismatch);
}
void semantic(const CatalogBegin &begin, const CodecContext &context) {
    const auto &required = context.requirements.vector();
    require(begin.totalBytes.value ==
                    required.get<session::CapabilityField::manifestBytes>().value &&
                begin.resourceCount ==
                    required.get<session::CapabilityField::resourceEntries>().value &&
                begin.consistencyCount ==
                    required.get<session::CapabilityField::consistencyEntries>().value,
            ErrorCode::capacity);
}
void semantic(const AssetsReady &ready, const CodecContext &context) {
    require(ready.catalogHash == context.catalogIdentity &&
                ready.schemaHash == context.application.definition().schemaSetHash,
            ErrorCode::digestMismatch);
}
void semantic(const GameStart &start, const CodecContext &context) {
    require(start.catalogHash == context.catalogIdentity, ErrorCode::digestMismatch);
}
void semantic(const FrameObject &object, const CodecContext &context) {
    if (object.key.kind == ObjectKind::module) {
        within(object.key.id.value, std::numeric_limits<std::uint16_t>::max());
        require(context.application.module({static_cast<std::uint16_t>(object.key.id.value)}) !=
                    nullptr,
                ErrorCode::unsupportedModule);
    }
}
template <typename T> void semantic(const T &, const CodecContext &) {}
template <typename T>
auto encode(const T &value, Route route, const CodecContext &context, RecordBudget budget)
    -> Bytes {
    require(matches(value, route), ErrorCode::malformed);
    semantic(value, context);
    if constexpr (std::is_same_v<T, Schema>) {
        return get(encodeSchema(value, budget));
    } else if constexpr (std::is_same_v<T, SchemaSetEnd> || std::is_same_v<T, CatalogEnd>) {
        return get(encodeDigest(value.digest));
    } else if constexpr (std::is_same_v<T, CatalogBegin>) {
        return get(encodeCatalogBegin(value));
    } else if constexpr (std::is_same_v<T, ChunkView>) {
        return get(encodeChunk(
            value, {context.selected.vector().get<session::CapabilityField::chunkBytes>().value},
            budget));
    } else if constexpr (std::is_same_v<T, GameStart>) {
        return get(encodeGameStart(value));
    } else if constexpr (std::is_same_v<T, ModMessage>) {
        return get(encodeModMessage(value, modContext(context, route, budget)));
    } else if constexpr (std::is_same_v<T, FrameBegin>) {
        return get(encodeFrameBegin(value));
    } else if constexpr (std::is_same_v<T, FrameObject>) {
        return get(encodeFrameObject(value, stateContext(context, budget)));
    } else if constexpr (std::is_same_v<T, FrameEnd>) {
        return get(encodeFrameEnd(value));
    } else if constexpr (std::is_same_v<T, Sound>) {
        return get(encodeSound(value, route, context.phase, soundContext(context, budget)));
    } else if constexpr (std::is_same_v<T, Event>) {
        return get(encodeEvent(value, route, context.phase, eventContext(context, budget)));
    } else if constexpr (std::is_same_v<T, EffectMessage>) {
        return get(
            encodeEffect(value, route.delivery, context.phase, effectContext(context, budget)));
    } else if constexpr (std::is_same_v<T, DecalName>) {
        return get(encodeDecalName(value, catalog(context), budget));
    } else if constexpr (std::is_same_v<T, ConsistencyPage>) {
        return get(encodeConsistencyPage(value, budget));
    } else if constexpr (std::is_same_v<T, ConsistencyEnd>) {
        return get(encodeConsistencyEnd(value));
    } else if constexpr (std::is_same_v<T, AssetsReady>) {
        return get(encodeAssetsReady(value));
    } else if constexpr (std::is_same_v<T, GameplayAck>) {
        return get(encodeFrameAck(value.frame));
    } else if constexpr (std::is_same_v<T, FrameAck>) {
        return get(encodeFrameAck(value));
    } else if constexpr (std::is_same_v<T, ResyncRequest>) {
        return get(encodeResyncRequest(value.lastApplied));
    } else {
        return get(encodeFatal(value.failure));
    }
}
} // namespace
auto authorizeMessage(Route route, const CodecContext &context) noexcept -> Result<RecordBudget> {
    return detail::protect([&] {
        require(validRoute(route) && context.epoch.value != 0 && !context.catalogIdentity.isZero());
        require(context.phase == Phase::loading || context.phase == Phase::gameplay,
                ErrorCode::wrongPhase);
        const auto *policy = context.application.service(route.direction, route.opcode);
        require(policy && policy->mode == ServiceMode::data2 &&
                    policy->phases[static_cast<std::size_t>(context.phase)],
                ErrorCode::wrongPhase);
        const auto metadata =
            route.direction == Direction::serverToClient &&
            (route.opcode == 14 || route.opcode == 22 || (route.opcode == 43 && route.kind <= 4));
        const auto clientLoading = route.direction == Direction::clientToServer && route.kind <= 4;
        const auto initialObjects = route.direction == Direction::serverToClient &&
                                    (route.opcode == 15 || (route.opcode == 43 && route.kind == 6));
        if (metadata || clientLoading) {
            require(context.phase == Phase::loading, ErrorCode::wrongPhase);
        } else if (!initialObjects && route.kind != 255) {
            require(context.phase == Phase::gameplay, ErrorCode::wrongPhase);
        }
        if (initialObjects && context.phase == Phase::loading) {
            require(route.delivery == Delivery::reliable, ErrorCode::wrongPhase);
        }
        auto budget = context.budget;
        budget.messageBytes.value =
            std::min({budget.messageBytes.value, policy->maxBytes.value,
                      context.selected.vector()
                          .get<session::CapabilityField::reliableMessageBytes>()
                          .value});
        if (route.delivery == Delivery::unreliable) {
            budget.messageBytes.value =
                std::min(budget.messageBytes.value,
                         context.application.definition().limits.unreliableMessageBytes.value);
        }
        require(budget.messageBytes.value >= envelopeBytes, ErrorCode::byteBudget);
        return budget;
    });
}
auto encodeBody(const DataPayload &payload, Route route, const CodecContext &context) noexcept
    -> Result<Bytes> {
    return detail::protect([&] {
        require(!payload.valueless_by_exception(), ErrorCode::invalidSchema);
        const auto budget = get(authorizeMessage(route, context));
        auto bytes = std::visit(
            [&](const auto &value) { return encode(value, route, context, budget); }, payload);
        within(add(envelopeBytes, bytes.size()), budget.messageBytes.value);
        return bytes;
    });
}
auto decodeBody(std::span<const std::byte> bytes, Route route, const CodecContext &context) noexcept
    -> Result<DataPayload> {
    return detail::protect([&]() -> DataPayload {
        const auto budget = get(authorizeMessage(route, context));
        within(add(envelopeBytes, bytes.size()), budget.messageBytes.value);
        auto decode = [&]() -> DataPayload {
            if (route.kind == 255) {
                return FatalMessage{get(decodeFatal(bytes))};
            }
            if (route.direction == Direction::clientToServer) {
                switch (static_cast<ClientKind>(route.kind)) {
                case ClientKind::consistencyPage:
                    return get(decodeConsistencyPage(bytes, budget));
                case ClientKind::consistencyEnd:
                    return get(decodeConsistencyEnd(bytes));
                case ClientKind::assetsReady:
                    return get(decodeAssetsReady(bytes));
                case ClientKind::gameplayAck:
                    return GameplayAck{get(decodeFrameAck(bytes))};
                case ClientKind::frameAck:
                    return get(decodeFrameAck(bytes));
                case ClientKind::resyncRequest:
                    return ResyncRequest{get(decodeResyncRequest(bytes))};
                case ClientKind::modMessage:
                    return get(decodeModMessage(bytes, modContext(context, route, budget)));
                case ClientKind::fatal:
                    break;
                }
            } else if (route.opcode == 14) {
                if (route.kind == 1) {
                    return get(decodeSchema(bytes, budget));
                }
                return SchemaSetEnd{get(decodeDigest(bytes))};
            } else if (route.opcode == 43) {
                switch (static_cast<ResourceMessageKind>(route.kind)) {
                case ResourceMessageKind::catalogBegin:
                    return get(decodeCatalogBegin(bytes));
                case ResourceMessageKind::catalogChunk:
                    return get(decodeChunk(bytes,
                                           {context.selected.vector()
                                                .get<session::CapabilityField::chunkBytes>()
                                                .value},
                                           budget));
                case ResourceMessageKind::catalogEnd:
                    return CatalogEnd{get(decodeDigest(bytes))};
                case ResourceMessageKind::gameStart:
                    return get(decodeGameStart(bytes));
                case ResourceMessageKind::modMessage:
                    return get(decodeModMessage(bytes, modContext(context, route, budget)));
                case ResourceMessageKind::moduleState:
                    return get(decodeFrameObject(bytes, stateContext(context, budget)));
                case ResourceMessageKind::fatal:
                    break;
                }
            } else if (isFrame(route)) {
                if (route.kind == 1) {
                    return get(decodeFrameBegin(bytes));
                }
                if (route.kind == 2) {
                    return get(decodeFrameObject(bytes, stateContext(context, budget)));
                }
                return get(decodeFrameEnd(bytes));
            } else {
                switch (static_cast<Service>(route.opcode)) {
                case Service::clientData:
                    return get(decodeFrameObject(bytes, stateContext(context, budget)));
                case Service::sound:
                case Service::staticSound:
                    return get(
                        decodeSound(bytes, route, context.phase, soundContext(context, budget)));
                case Service::event:
                case Service::reliableEvent:
                    return get(
                        decodeEvent(bytes, route, context.phase, eventContext(context, budget)));
                case Service::effect:
                    return get(decodeEffect(bytes, route.delivery, context.phase,
                                            effectContext(context, budget)));
                case Service::decalName:
                    return get(decodeDecalName(bytes, catalog(context), budget));
                default:
                    break;
                }
            }
            fail();
        };
        auto payload = decode();
        std::visit(
            [&](const auto &value) {
                require(matches(value, route));
                semantic(value, context);
            },
            payload);
        return payload;
    });
}
auto encodeMessage(const DataPayload &payload, Envelope header,
                   const CodecContext &context) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        require(header.epoch == context.epoch, ErrorCode::staleContext);
        const auto budget = get(authorizeMessage(header.route, context));
        const auto body = get(encodeBody(payload, header.route, context));
        header.body = body;
        return get(encodeEnvelope(header, budget));
    });
}
auto decodeMessage(std::span<const std::byte> bytes, Direction direction,
                   const CodecContext &context) noexcept -> Result<DecodedMessage> {
    return detail::protect([&] {
        const auto header = get(decodeEnvelope(bytes, direction, context.budget));
        require(header.epoch == context.epoch, ErrorCode::staleContext);
        auto payload = get(decodeBody(header.body, header.route, context));
        return DecodedMessage{header, std::move(payload)};
    });
}
auto DataReceiver::create(SessionNonce nonce, MapEpoch epoch,
                          std::array<std::uint32_t, 3> firstSequence) noexcept
    -> Result<DataReceiver> {
    return detail::protect([&] {
        require(!nonce.isZero() && epoch.value != 0);
        for (const auto sequence : firstSequence) {
            require(sequence <= maxSequence, ErrorCode::sequence);
        }
        return DataReceiver{nonce, epoch, firstSequence};
    });
}
auto DataReceiver::receive(std::span<const std::byte> bytes, Direction direction,
                           const CodecContext &context) noexcept
    -> Result<std::optional<DecodedMessage>> {
    auto result = detail::protect([&]() -> std::optional<DecodedMessage> {
        require(!failed, ErrorCode::wrongPhase);
        require(context.epoch == epochValue, ErrorCode::staleContext);
        const auto header = get(decodeEnvelope(bytes, direction, context.budget));
        const auto disposition = get(tracker.accept(header));
        if (disposition != ReceiveDisposition::accept) {
            return std::nullopt;
        }
        auto payload = get(decodeBody(header.body, header.route, context));
        return DecodedMessage{header, std::move(payload)};
    });
    if (!result) {
        failed = true;
    }
    return result;
}
} // namespace extendedPrecache::data
