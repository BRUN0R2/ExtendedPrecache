#include "stateIo.hpp"

namespace extendedPrecache::data {
namespace detail {
void validateFrameBegin(FrameBegin begin) {
    require(begin.id.value > 0 && begin.id.value <= maxSequence &&
            begin.base.value < begin.id.value);
    validFloat(begin.serverTime);
    require(begin.serverTime >= 0);
}
auto objectSchema(ObjectKey key, SchemaId id, const StateContext &context) -> const Schema & {
    require(
        context.objects.entityEntries > 0 && context.objects.entityEntries <= maxEntityEntries &&
            context.objects.clientEntries > 0 && context.objects.clientEntries <= maxClientEntries,
        ErrorCode::capacity);
    auto role = SchemaRole::entity;
    switch (key.kind) {
    case ObjectKind::entity:
        require(key.id.value < context.objects.entityEntries, ErrorCode::capacity);
        role = SchemaRole::entity;
        break;
    case ObjectKind::client:
        require(key.id.value > 0 && key.id.value <= context.objects.clientEntries,
                ErrorCode::capacity);
        role = SchemaRole::client;
        break;
    case ObjectKind::weapon:
        require(key.id.value < context.objects.weaponEntries, ErrorCode::capacity);
        role = SchemaRole::weapon;
        break;
    case ObjectKind::module:
        require(key.id.value > 0 && key.id.value <= std::numeric_limits<std::uint16_t>::max(),
                ErrorCode::capacity);
        role = SchemaRole::moduleState;
        break;
    case ObjectKind::staticObject:
        require(key.id.value < context.objects.staticEntries, ErrorCode::capacity);
        role = SchemaRole::entity;
        break;
    default:
        fail();
    }
    const auto *schema = findSchema(context.schemas, id);
    require(schema && schema->role == role && context.bindings.allows(key, id),
            ErrorCode::invalidSchema);
    get(validateSchema(*schema));
    return *schema;
}
} // namespace detail
auto encodeFrameBegin(FrameBegin begin) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        detail::validateFrameBegin(begin);
        detail::Writer writer{frameBeginBytes};
        writer.integer(begin.id.value);
        writer.integer(begin.base.value);
        writer.integer(begin.changeCount);
        writer.integer(begin.changeBytes.value);
        writer.f32(begin.serverTime);
        writer.zero(4);
        return std::move(writer).finish();
    });
}
auto decodeFrameBegin(std::span<const std::byte> bytes) noexcept -> Result<FrameBegin> {
    return detail::protect([&] {
        detail::Reader reader{bytes};
        const FrameBegin begin{{reader.integer<std::uint32_t>()},
                               {reader.integer<std::uint32_t>()},
                               reader.integer<std::uint32_t>(),
                               {reader.integer<std::uint32_t>()},
                               reader.f32()};
        reader.zero(4);
        reader.end();
        detail::validateFrameBegin(begin);
        return begin;
    });
}
auto encodeFrameObject(const FrameObject &object, const StateContext &context) noexcept
    -> Result<Bytes> {
    return detail::protect([&] {
        using namespace detail;
        require(object.frame.value > 0 && object.frame.value <= maxSequence &&
                object.operation <= Operation::remove);
        const auto &schema = objectSchema(object.key, object.schema, context);
        const auto removed = object.operation == Operation::remove;
        require(!(removed && object.key == ObjectKey{ObjectKind::entity, {0}}));
        require(removed ? object.fields.empty() : object.fields.size() == schema.fields.size(),
                ErrorCode::invalidSchema);
        require(context.records.messageBytes.value >= envelopeBytes + frameObjectHeaderBytes,
                ErrorCode::byteBudget);
        Writer writer{std::min<std::uint64_t>(context.records.messageBytes.value - envelopeBytes,
                                              context.records.ownedBytes.value)};
        writer.integer(object.frame.value);
        writer.enumeration(object.key.kind);
        writer.enumeration(object.operation);
        writer.integer(object.schema.value);
        writer.integer(object.key.id.value);
        const auto maskBytes = removed ? 0 : (schema.fields.size() + 7) / 8;
        writer.integer(small(maskBytes));
        writer.zero(2);
        Bytes mask(maskBytes);
        bool any{};
        for (std::size_t index = 0; index < object.fields.size(); ++index) {
            if (object.fields[index]) {
                mask[index / 8] |= static_cast<std::byte>(1U << (index % 8));
                any = true;
            }
            require(object.operation != Operation::replace || object.fields[index].has_value(),
                    ErrorCode::invalidSchema);
        }
        require(object.operation != Operation::patch || any);
        writer.bytes(mask);
        const ValueContext values{context.catalog, context.objects.entityEntries};
        for (std::size_t index = 0; index < object.fields.size(); ++index) {
            if (object.fields[index]) {
                writeField(writer, schema.fields[index], *object.fields[index], values);
            }
        }
        return std::move(writer).finish();
    });
}
auto decodeFrameObject(std::span<const std::byte> bytes, const StateContext &context) noexcept
    -> Result<FrameObject> {
    return detail::protect([&] {
        using namespace detail;
        within(add(envelopeBytes, bytes.size()), context.records.messageBytes.value);
        Reader reader{bytes};
        FrameObject object{};
        object.frame = {reader.integer<std::uint32_t>()};
        object.key.kind = reader.enumeration<ObjectKind>();
        object.operation = reader.enumeration<Operation>();
        object.schema = {reader.integer<std::uint16_t>()};
        object.key.id = {reader.integer<std::uint32_t>()};
        const auto maskBytes = reader.integer<std::uint16_t>();
        reader.zero(2);
        require(object.frame.value > 0 && object.frame.value <= maxSequence &&
                object.operation <= Operation::remove);
        const auto &schema = objectSchema(object.key, object.schema, context);
        if (object.operation == Operation::remove) {
            require(maskBytes == 0 && object.key != ObjectKey{ObjectKind::entity, {0}});
            reader.end();
            return object;
        }
        require(maskBytes == (schema.fields.size() + 7) / 8);
        const auto mask = reader.take(maskBytes);
        if (!mask.empty() && schema.fields.size() % 8 != 0) {
            require((std::to_integer<unsigned>(mask.back()) >> (schema.fields.size() % 8)) == 0);
        }
        AllocationBudget allocation{context.records.ownedBytes};
        allocation.charge(multiply(schema.fields.size(), sizeof(std::optional<FieldValue>)));
        object.fields.resize(schema.fields.size());
        bool any{};
        const ValueContext values{context.catalog, context.objects.entityEntries};
        for (std::size_t index = 0; index < schema.fields.size(); ++index) {
            const auto present =
                (std::to_integer<unsigned>(mask[index / 8]) & (1U << (index % 8))) != 0;
            require(object.operation != Operation::replace || present);
            if (present) {
                object.fields[index] = readField(reader, schema.fields[index], values, allocation);
                any = true;
            }
        }
        require(object.operation != Operation::patch || any);
        reader.end();
        return object;
    });
}
auto encodeFrameEnd(FrameEnd end) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        detail::require(end.id.value > 0 && end.id.value <= maxSequence &&
                        !end.snapshotHash.isZero());
        detail::Writer writer{frameEndBytes};
        writer.integer(end.id.value);
        writer.integer(end.changeCount);
        writer.bytes(end.snapshotHash.bytes);
        return std::move(writer).finish();
    });
}
auto decodeFrameEnd(std::span<const std::byte> bytes) noexcept -> Result<FrameEnd> {
    return detail::protect([&] {
        detail::Reader reader{bytes};
        const FrameEnd result{{reader.integer<std::uint32_t>()},
                              reader.integer<std::uint32_t>(),
                              reader.identity<Digest>()};
        reader.end();
        detail::require(result.id.value > 0 && result.id.value <= maxSequence &&
                        !result.snapshotHash.isZero());
        return result;
    });
}
auto encodeFrameAck(FrameAck ack) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        detail::require(ack.id.value > 0 && ack.id.value <= maxSequence &&
                        !ack.snapshotHash.isZero());
        detail::Writer writer{frameAckBytes};
        writer.integer(ack.id.value);
        writer.bytes(ack.snapshotHash.bytes);
        return std::move(writer).finish();
    });
}
auto decodeFrameAck(std::span<const std::byte> bytes) noexcept -> Result<FrameAck> {
    return detail::protect([&] {
        detail::Reader reader{bytes};
        const FrameAck result{{reader.integer<std::uint32_t>()}, reader.identity<Digest>()};
        reader.end();
        detail::get(encodeFrameAck(result));
        return result;
    });
}
} // namespace extendedPrecache::data
