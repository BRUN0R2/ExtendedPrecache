#include "valueIo.hpp"

namespace extendedPrecache::data {
namespace detail {
namespace {
template <typename T> auto atomAs(const Atom &atom) -> const T & {
    const auto *value = std::get_if<T>(&atom);
    require(value != nullptr, ErrorCode::invalidSchema);
    return *value;
}
void validEntity(std::uint16_t value, ValueContext context) {
    require(context.entityEntries > 0 && context.entityEntries <= maxEntityEntries &&
                value < context.entityEntries,
            ErrorCode::capacity);
}
void writeAtom(Writer &writer, const FieldDefinition &field, const Atom &atom,
               ValueContext context) {
    switch (field.kind) {
    case ValueKind::u8:
        writer.integer(atomAs<std::uint8_t>(atom));
        break;
    case ValueKind::i8:
        writer.integer(std::bit_cast<std::uint8_t>(atomAs<std::int8_t>(atom)));
        break;
    case ValueKind::u16:
        writer.integer(atomAs<std::uint16_t>(atom));
        break;
    case ValueKind::i16:
        writer.integer(std::bit_cast<std::uint16_t>(atomAs<std::int16_t>(atom)));
        break;
    case ValueKind::u32:
        writer.integer(atomAs<std::uint32_t>(atom));
        break;
    case ValueKind::i32:
        writer.integer(std::bit_cast<std::uint32_t>(atomAs<std::int32_t>(atom)));
        break;
    case ValueKind::f32:
        writer.f32(atomAs<float>(atom));
        break;
    case ValueKind::bytes: {
        const auto &bytes = atomAs<ByteString>(atom).bytes;
        within(bytes.size(), field.maxAtomBytes);
        require(!field.has(FieldFlag::nativeCString) ||
                std::ranges::find(bytes, std::byte{}) == bytes.end());
        writer.integer(small(bytes.size()));
        writer.bytes(bytes);
        break;
    }
    case ValueKind::resource: {
        const auto value = atomAs<ResourceReference>(atom);
        require(field.resourceKind && *field.resourceKind == value.kind, ErrorCode::invalidSchema);
        get(context.resources.validate(value, field.has(FieldFlag::nullable)));
        writer.integer(value.id.value);
        break;
    }
    case ValueKind::entity16: {
        const auto value = atomAs<EntityId>(atom).value;
        validEntity(value, context);
        writer.integer(value);
        break;
    }
    case ValueKind::packedEntity16: {
        const auto value = atomAs<PackedEntity>(atom).value;
        validEntity(static_cast<std::uint16_t>(value & 0x0fffU), context);
        writer.integer(value);
        break;
    }
    case ValueKind::coord16:
        writer.integer(std::bit_cast<std::uint16_t>(atomAs<Coord>(atom).value));
        break;
    case ValueKind::coords3:
        for (const auto coord : atomAs<Coords>(atom)) {
            writer.integer(std::bit_cast<std::uint16_t>(coord.value));
        }
        break;
    case ValueKind::vec3f32:
        for (const auto value : atomAs<Vec3>(atom)) {
            writer.f32(value);
        }
        break;
    }
}
auto readAtom(Reader &reader, const FieldDefinition &field, ValueContext context,
              AllocationBudget &allocation) -> Atom {
    switch (field.kind) {
    case ValueKind::u8:
        return reader.integer<std::uint8_t>();
    case ValueKind::i8:
        return std::bit_cast<std::int8_t>(reader.integer<std::uint8_t>());
    case ValueKind::u16:
        return reader.integer<std::uint16_t>();
    case ValueKind::i16:
        return std::bit_cast<std::int16_t>(reader.integer<std::uint16_t>());
    case ValueKind::u32:
        return reader.integer<std::uint32_t>();
    case ValueKind::i32:
        return std::bit_cast<std::int32_t>(reader.integer<std::uint32_t>());
    case ValueKind::f32:
        return reader.f32();
    case ValueKind::bytes: {
        const auto length = reader.integer<std::uint16_t>();
        within(length, field.maxAtomBytes);
        const auto part = reader.take(length);
        allocation.charge(length);
        require(!field.has(FieldFlag::nativeCString) ||
                std::ranges::find(part, std::byte{}) == part.end());
        return ByteString{Bytes{part.begin(), part.end()}};
    }
    case ValueKind::resource: {
        if (!field.resourceKind) {
            fail(ErrorCode::invalidSchema);
        }
        const auto kind = *field.resourceKind;
        const auto value = ResourceReference{kind, {reader.integer<std::uint32_t>()}};
        get(context.resources.validate(value, field.has(FieldFlag::nullable)));
        return value;
    }
    case ValueKind::entity16: {
        const auto value = reader.integer<std::uint16_t>();
        validEntity(value, context);
        return EntityId{value};
    }
    case ValueKind::packedEntity16: {
        const auto value = reader.integer<std::uint16_t>();
        validEntity(static_cast<std::uint16_t>(value & 0x0fffU), context);
        return PackedEntity{value};
    }
    case ValueKind::coord16:
        return Coord{std::bit_cast<std::int16_t>(reader.integer<std::uint16_t>())};
    case ValueKind::coords3: {
        Coords result{};
        for (auto &coord : result) {
            coord = {std::bit_cast<std::int16_t>(reader.integer<std::uint16_t>())};
        }
        return result;
    }
    case ValueKind::vec3f32: {
        Vec3 result{};
        for (auto &value : result) {
            value = reader.f32();
        }
        return result;
    }
    }
    fail(ErrorCode::invalidSchema);
}
} // namespace
void writeField(Writer &writer, const FieldDefinition &field, const FieldValue &value,
                ValueContext context) {
    if (field.has(FieldFlag::array)) {
        within(value.elements.size(), field.maxElements);
        writer.integer(small(value.elements.size()));
    } else {
        require(value.elements.size() == 1, ErrorCode::invalidSchema);
    }
    for (const auto &atom : value.elements) {
        writeAtom(writer, field, atom, context);
    }
}
auto readField(Reader &reader, const FieldDefinition &field, ValueContext context,
               AllocationBudget &allocation) -> FieldValue {
    const auto count =
        field.has(FieldFlag::array) ? reader.integer<std::uint16_t>() : std::uint16_t{1};
    within(count, field.maxElements);
    reader.count(count, field.kind == ValueKind::bytes ? 2 : field.maxAtomBytes);
    allocation.charge(multiply(count, sizeof(Atom)));
    FieldValue result;
    result.elements.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        result.elements.push_back(readAtom(reader, field, context, allocation));
    }
    return result;
}
} // namespace detail
auto encodeValues(const Schema &schema, const ValueSet &values, ValueContext context,
                  RecordBudget budget) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        detail::get(validateSchema(schema));
        detail::require(values.size() == schema.fields.size(), ErrorCode::invalidSchema);
        detail::Writer writer{
            std::min<std::uint64_t>(budget.messageBytes.value, budget.ownedBytes.value)};
        for (std::size_t index = 0; index < values.size(); ++index) {
            detail::writeField(writer, schema.fields[index], values[index], context);
        }
        return std::move(writer).finish();
    });
}
auto decodeValues(const Schema &schema, std::span<const std::byte> bytes, ValueContext context,
                  RecordBudget budget) noexcept -> Result<ValueSet> {
    return detail::protect([&] {
        detail::get(validateSchema(schema));
        detail::inputBudget(bytes, budget);
        detail::Reader reader{bytes};
        detail::AllocationBudget allocation{budget.ownedBytes};
        allocation.charge(detail::multiply(schema.fields.size(), sizeof(FieldValue)));
        ValueSet result;
        result.reserve(schema.fields.size());
        for (const auto &field : schema.fields) {
            result.push_back(detail::readField(reader, field, context, allocation));
        }
        reader.end();
        return result;
    });
}
} // namespace extendedPrecache::data
