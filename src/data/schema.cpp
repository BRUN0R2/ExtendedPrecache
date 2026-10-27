#include "binary.hpp"
#include <extendedPrecache/data/schema.hpp>

#include <set>

namespace extendedPrecache::data {
namespace {
using namespace detail;
auto fixedBytes(ValueKind kind) -> std::uint16_t {
    constexpr auto widths =
        std::array<std::uint16_t, 14>{1, 1, 2, 2, 4, 4, 4, 0, 4, 2, 2, 2, 6, 12};
    require(kind >= ValueKind::u8 && kind <= ValueKind::vec3f32, ErrorCode::invalidSchema);
    return widths[static_cast<std::size_t>(kind) - 1];
}
} // namespace

auto validateSchema(const Schema &schema) noexcept -> Result<void> {
    return detail::protect([&] {
        require(schema.id.value != 0 && schema.role >= SchemaRole::entity &&
                    schema.role <= SchemaRole::localReplay,
                ErrorCode::invalidSchema);
        require(identifier(schema.name), ErrorCode::invalidSchema);
        within(schema.fields.size(), std::numeric_limits<std::uint16_t>::max());
        const auto emptyAllowed =
            schema.role == SchemaRole::eventArgs || schema.role == SchemaRole::moduleState ||
            schema.role == SchemaRole::modMessage || schema.role == SchemaRole::localReplay;
        require(emptyAllowed || !schema.fields.empty(), ErrorCode::invalidSchema);
        std::set<std::string_view> names;
        for (std::size_t index = 0; index < schema.fields.size(); ++index) {
            const auto &field = schema.fields[index];
            require(field.id.value == index && identifier(field.name) &&
                        names.insert(field.name).second,
                    ErrorCode::invalidSchema);
            require(field.flags <= 7 && field.maxElements > 0 && field.maxAtomBytes > 0,
                    ErrorCode::invalidSchema);
            require(field.has(FieldFlag::array) || field.maxElements == 1,
                    ErrorCode::invalidSchema);
            require(!field.has(FieldFlag::nullable) || field.kind == ValueKind::resource,
                    ErrorCode::invalidSchema);
            require(!field.has(FieldFlag::nativeCString) || field.kind == ValueKind::bytes,
                    ErrorCode::invalidSchema);
            require((field.kind == ValueKind::resource) == field.resourceKind.has_value(),
                    ErrorCode::invalidSchema);
            require(!field.resourceKind || validKind(*field.resourceKind),
                    ErrorCode::invalidSchema);
            const auto width = fixedBytes(field.kind);
            require(width == 0 || field.maxAtomBytes == width, ErrorCode::invalidSchema);
        }
    });
}
auto maxValueBytes(const Schema &schema) noexcept -> Result<std::uint64_t> {
    return detail::protect([&] {
        get(validateSchema(schema));
        std::uint64_t total{};
        for (const auto &field : schema.fields) {
            const auto atom = static_cast<std::uint64_t>(field.maxAtomBytes) +
                              (field.kind == ValueKind::bytes ? 2U : 0U);
            total =
                add(total,
                    field.has(FieldFlag::array) ? add(2, multiply(field.maxElements, atom)) : atom);
        }
        return total;
    });
}
auto encodeSchema(const Schema &schema, RecordBudget budget) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        get(validateSchema(schema));
        Writer writer{std::min<std::uint64_t>(budget.messageBytes.value, budget.ownedBytes.value)};
        writer.integer(schema.id.value);
        writer.enumeration(schema.role);
        writer.zero(1);
        writer.integer(small(schema.fields.size()));
        writer.integer(small(schema.name.size()));
        writer.text(schema.name);
        for (const auto &field : schema.fields) {
            writer.integer(field.id.value);
            writer.enumeration(field.kind);
            writer.integer(static_cast<std::uint8_t>(
                field.resourceKind ? static_cast<std::uint8_t>(*field.resourceKind) : 0));
            writer.integer(field.flags);
            writer.integer(field.maxElements);
            writer.integer(field.maxAtomBytes);
            writer.integer(static_cast<std::uint8_t>(field.name.size()));
            writer.zero(1);
            writer.text(field.name);
        }
        return std::move(writer).finish();
    });
}
auto decodeSchema(std::span<const std::byte> bytes, RecordBudget budget) noexcept
    -> Result<Schema> {
    return detail::protect([&] {
        inputBudget(bytes, budget);
        Reader reader{bytes};
        Schema schema{};
        schema.id = {reader.integer<std::uint16_t>()};
        schema.role = reader.enumeration<SchemaRole>();
        reader.zero(1);
        const auto count = reader.integer<std::uint16_t>();
        const auto nameBytes = reader.integer<std::uint16_t>();
        AllocationBudget allocation{budget.ownedBytes};
        allocation.charge(add(sizeof(Schema), nameBytes));
        schema.name = reader.text(nameBytes, maxNameBytes);
        reader.count(count, 13);
        allocation.charge(multiply(count, sizeof(FieldDefinition)));
        schema.fields.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            FieldDefinition field{};
            field.id = {reader.integer<std::uint16_t>()};
            field.kind = reader.enumeration<ValueKind>();
            const auto kind = reader.integer<std::uint8_t>();
            if (kind != 0) {
                field.resourceKind = static_cast<ResourceKind>(kind);
            }
            field.flags = reader.integer<std::uint16_t>();
            field.maxElements = reader.integer<std::uint16_t>();
            field.maxAtomBytes = reader.integer<std::uint16_t>();
            const auto length = reader.integer<std::uint8_t>();
            reader.zero(1);
            allocation.charge(length);
            field.name = reader.text(length, maxNameBytes);
            schema.fields.push_back(std::move(field));
        }
        reader.end();
        get(validateSchema(schema));
        return schema;
    });
}
auto encodeSchemaSet(std::span<const Schema> schemas, ByteCount maximum) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        require(!schemas.empty(), ErrorCode::invalidSchema);
        Writer writer{maximum.value};
        writer.integer(small(schemas.size()));
        std::uint16_t previous{};
        for (const auto &schema : schemas) {
            require(schema.id.value > previous, ErrorCode::invalidSchema);
            previous = schema.id.value;
            const auto bytes = get(encodeSchema(schema));
            writer.integer(narrow(bytes.size()));
            writer.bytes(bytes);
        }
        return std::move(writer).finish();
    });
}
auto decodeSchemaSet(std::span<const std::byte> bytes, ByteCount maximum) noexcept
    -> Result<std::vector<Schema>> {
    return detail::protect([&] {
        within(bytes.size(), maximum.value);
        Reader reader{bytes};
        const auto count = reader.integer<std::uint16_t>();
        require(count > 0, ErrorCode::invalidSchema);
        reader.count(count, 13);
        std::vector<Schema> result;
        result.reserve(count);
        std::uint16_t previous{};
        for (std::size_t index = 0; index < count; ++index) {
            const auto length = reader.integer<std::uint32_t>();
            auto schema = get(decodeSchema(reader.take(length)));
            require(schema.id.value > previous, ErrorCode::invalidSchema);
            previous = schema.id.value;
            result.push_back(std::move(schema));
        }
        reader.end();
        return result;
    });
}
auto findSchema(std::span<const Schema> schemas, SchemaId id) noexcept -> const Schema * {
    const auto found = std::ranges::find(schemas, id, &Schema::id);
    return found == schemas.end() ? nullptr : &*found;
}
} // namespace extendedPrecache::data
