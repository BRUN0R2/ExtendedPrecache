#pragma once

#include <extendedPrecache/data/types.hpp>

#include <optional>
#include <string>

namespace extendedPrecache::data {

enum class SchemaRole : std::uint8_t {
    entity = 1,
    client,
    weapon,
    eventArgs,
    moduleState,
    modMessage,
    effect,
    localReplay
};
enum class ValueKind : std::uint8_t {
    u8 = 1,
    i8,
    u16,
    i16,
    u32,
    i32,
    f32,
    bytes,
    resource,
    entity16,
    packedEntity16,
    coord16,
    coords3,
    vec3f32
};
enum class FieldFlag : std::uint8_t { nullable = 1, array = 2, nativeCString = 4 };
struct FieldDefinition final {
    FieldId id;
    ValueKind kind{ValueKind::u8};
    std::optional<ResourceKind> resourceKind;
    std::uint16_t flags{};
    std::uint16_t maxElements{1};
    std::uint16_t maxAtomBytes{};
    std::string name;
    [[nodiscard]] auto has(FieldFlag flag) const noexcept -> bool {
        return (flags & static_cast<std::uint16_t>(flag)) != 0;
    }
    auto operator==(const FieldDefinition &) const -> bool = default;
};
struct Schema final {
    SchemaId id;
    SchemaRole role{SchemaRole::entity};
    std::string name;
    std::vector<FieldDefinition> fields;
    auto operator==(const Schema &) const -> bool = default;
};

[[nodiscard]] auto validateSchema(const Schema &schema) noexcept -> Result<void>;
[[nodiscard]] auto maxValueBytes(const Schema &schema) noexcept -> Result<std::uint64_t>;
[[nodiscard]] auto encodeSchema(const Schema &schema, RecordBudget budget = {}) noexcept
    -> Result<Bytes>;
[[nodiscard]] auto decodeSchema(std::span<const std::byte> bytes, RecordBudget budget = {}) noexcept
    -> Result<Schema>;
[[nodiscard]] auto encodeSchemaSet(std::span<const Schema> schemas, ByteCount maximum) noexcept
    -> Result<Bytes>;
[[nodiscard]] auto decodeSchemaSet(std::span<const std::byte> bytes, ByteCount maximum) noexcept
    -> Result<std::vector<Schema>>;
[[nodiscard]] auto findSchema(std::span<const Schema> schemas, SchemaId id) noexcept
    -> const Schema *;

class ReferenceValidator {
  public:
    virtual ~ReferenceValidator() = default;
    [[nodiscard]] virtual auto validate(ResourceReference reference, bool nullable) const noexcept
        -> Result<void> = 0;
};
struct ValueContext final {
    const ReferenceValidator &resources;
    std::uint32_t entityEntries{maxEntityEntries};
};
[[nodiscard]] auto encodeValues(const Schema &schema, const ValueSet &values, ValueContext context,
                                RecordBudget budget = {}) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeValues(const Schema &schema, std::span<const std::byte> bytes,
                                ValueContext context, RecordBudget budget = {}) noexcept
    -> Result<ValueSet>;

} // namespace extendedPrecache::data
