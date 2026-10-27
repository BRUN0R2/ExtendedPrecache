#include "applicationJson.hpp"

#include <set>

namespace extendedPrecache::data::detail {
void closed(const Json &object, std::initializer_list<std::string_view> keys) {
    require(object.is_object() && object.size() == keys.size(), ErrorCode::invalidSchema);
    for (const auto key : keys) {
        require(object.contains(key), ErrorCode::invalidSchema);
    }
}
auto number(const Json &object, std::string_view key, std::uint64_t minimum, std::uint64_t maximum)
    -> std::uint64_t {
    const auto &value = object.at(key);
    require(value.is_number_integer() &&
                (value.is_number_unsigned() || value.get<std::int64_t>() >= 0),
            ErrorCode::invalidSchema);
    const auto result = value.get<std::uint64_t>();
    require(result >= minimum && result <= maximum, ErrorCode::invalidSchema);
    return result;
}
auto text(const Json &object, std::string_view key) -> const std::string & {
    const auto &value = object.at(key);
    require(value.is_string(), ErrorCode::invalidSchema);
    return value.get_ref<const std::string &>();
}
auto boolean(const Json &object, std::string_view key) -> bool {
    const auto &value = object.at(key);
    require(value.is_boolean(), ErrorCode::invalidSchema);
    return value.get<bool>();
}
auto digestValue(const Json &object, std::string_view key) -> Digest {
    const auto result = session::parseDigest(text(object, key));
    require(result && !result->isZero(), ErrorCode::invalidSchema);
    return *result;
}
auto list(const Json &object, std::string_view key, std::size_t maximum) -> const Json & {
    const auto &value = object.at(key);
    require(value.is_array() && value.size() <= maximum, ErrorCode::invalidSchema);
    return value;
}
auto deliveries(const Json &object) -> Deliveries {
    const auto &values = list(object, "deliveries", 2);
    require(!values.empty(), ErrorCode::invalidSchema);
    Deliveries result{};
    for (const auto &value : values) {
        require(value.is_string(), ErrorCode::invalidSchema);
        if (value == "reliable") {
            require(!result.reliable, ErrorCode::invalidSchema);
            result.reliable = true;
        } else {
            require(value == "unreliable" && !result.unreliable, ErrorCode::invalidSchema);
            result.unreliable = true;
        }
    }
    return result;
}
namespace {
auto strictDocument(std::string_view document) -> Json {
    constexpr auto maximumShapeDepth = 10;
    std::vector<std::set<std::string>> keys;
    auto callback = [&](int depth, Json::parse_event_t event, Json &parsed) {
        require(depth <= maximumShapeDepth, ErrorCode::invalidSchema);
        if (event == Json::parse_event_t::object_start) {
            keys.emplace_back();
        }
        if (event == Json::parse_event_t::key) {
            require(!keys.empty() && keys.back().insert(parsed.get<std::string>()).second,
                    ErrorCode::invalidSchema);
        }
        if (event == Json::parse_event_t::object_end) {
            require(!keys.empty());
            keys.pop_back();
        }
        if (event == Json::parse_event_t::value) {
            require(!parsed.is_number_float(), ErrorCode::invalidSchema);
            require(!parsed.is_number_integer() || parsed.is_number_unsigned() ||
                        parsed.get<std::int64_t>() != 0,
                    ErrorCode::invalidSchema);
        }
        return true;
    };
    auto root = Json::parse(document, callback, false, false);
    require(!root.is_discarded(), ErrorCode::invalidSchema);
    return root;
}
auto parseLimits(const Json &object) -> ApplicationLimits {
    closed(object,
           {"schemaSetBytes", "contractArtifactBytes", "stateFrameBytes", "snapshotWindowFrames",
            "unreliableMessageBytes", "loadTimeoutMs", "consistencyTimeoutMs", "demoFileBytes",
            "demoBlockBytes", "demoBootstrapBytes", "demoDurationMicros"});
    const auto bytes = [&](std::string_view key, std::uint64_t minimum) {
        return ByteCount{
            static_cast<std::uint32_t>(number(object, key, minimum, maxMetadataBytes))};
    };
    const auto wide = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    return {bytes("schemaSetBytes", 2),
            bytes("contractArtifactBytes", 1),
            bytes("stateFrameBytes", 1),
            static_cast<std::uint16_t>(number(object, "snapshotWindowFrames", 1, 65535)),
            {static_cast<std::uint32_t>(
                number(object, "unreliableMessageBytes", envelopeBytes, maxMessageBytes))},
            {static_cast<std::uint32_t>(
                number(object, "loadTimeoutMs", session::minTimeoutMs, maxMetadataBytes))},
            {static_cast<std::uint32_t>(
                number(object, "consistencyTimeoutMs", session::minTimeoutMs, maxMetadataBytes))},
            {number(object, "demoFileBytes", 360, wide)},
            bytes("demoBlockBytes", 68),
            bytes("demoBootstrapBytes", 1),
            {number(object, "demoDurationMicros", 1, wide)}};
}
auto parseObjects(const Json &object) -> ObjectLimits {
    closed(object, {"entityEntries", "clientEntries", "weaponEntries", "staticEntries"});
    return {static_cast<std::uint32_t>(number(object, "entityEntries", 1, maxEntityEntries)),
            static_cast<std::uint32_t>(number(object, "clientEntries", 1, maxClientEntries)),
            static_cast<std::uint32_t>(number(object, "weaponEntries", 0, 65535)),
            static_cast<std::uint32_t>(number(object, "staticEntries", 0, maxMetadataBytes))};
}
auto parseSchemas(const Json &root) -> std::vector<Schema> {
    std::vector<Schema> schemas;
    for (const auto &object : list(root, "schemaDefinitions")) {
        closed(object, {"schemaId", "role", "name", "fields"});
        Schema schema{};
        schema.id = {static_cast<std::uint16_t>(number(object, "schemaId", 1, 65535))};
        schema.role = static_cast<SchemaRole>(number(object, "role", 1, 8));
        schema.name = text(object, "name");
        for (const auto &item : list(object, "fields")) {
            closed(item, {"fieldId", "valueKind", "resourceKind", "flags", "maxElements",
                          "maxAtomBytes", "name"});
            FieldDefinition field{};
            field.id = {static_cast<std::uint16_t>(number(item, "fieldId", 0, 65534))};
            field.kind = static_cast<ValueKind>(number(item, "valueKind", 1, 14));
            const auto resourceKind = number(item, "resourceKind", 0, 5);
            if (resourceKind != 0) {
                field.resourceKind = static_cast<ResourceKind>(resourceKind);
            }
            field.flags = static_cast<std::uint16_t>(number(item, "flags", 0, 7));
            field.maxElements = static_cast<std::uint16_t>(number(item, "maxElements", 1, 65535));
            field.maxAtomBytes = static_cast<std::uint16_t>(number(item, "maxAtomBytes", 1, 65535));
            field.name = text(item, "name");
            schema.fields.push_back(std::move(field));
        }
        std::ranges::sort(schema.fields, {}, &FieldDefinition::id);
        get(validateSchema(schema));
        schemas.push_back(std::move(schema));
    }
    std::ranges::sort(schemas, {}, &Schema::id);
    return schemas;
}
} // namespace
auto parseApplication(std::string_view document) -> ApplicationDefinition {
    const auto root = strictDocument(document);
    closed(root, {"schemaVersion", "specificationOnly", "availability", "dataContractId",
                  "dataContractSha256", "schemaSetSha256", "limits", "objectLimits",
                  "schemaDefinitions", "moduleProfiles", "effectPolicies", "servicePolicies"});
    number(root, "schemaVersion", 2, 2);
    number(root, "dataContractId", 2, 2);
    require(boolean(root, "specificationOnly") && text(root, "availability") == "unavailable",
            ErrorCode::unsupportedContract);
    return {digestValue(root, "dataContractSha256"),
            digestValue(root, "schemaSetSha256"),
            parseLimits(root.at("limits")),
            parseObjects(root.at("objectLimits")),
            parseSchemas(root),
            parseModules(root),
            parseEffects(root),
            parseServices(root)};
}
} // namespace extendedPrecache::data::detail
