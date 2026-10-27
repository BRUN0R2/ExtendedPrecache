#include "applicationJson.hpp"

namespace extendedPrecache::data::detail {
namespace {
auto binding(const Json &value) -> std::optional<BindingIdentity> {
    if (value.is_null()) {
        return std::nullopt;
    }
    closed(value, {"binarySha256", "bindingSha256"});
    return BindingIdentity{digestValue(value, "binarySha256"), digestValue(value, "bindingSha256")};
}
auto message(const Json &object) -> MessageDefinition {
    closed(object, {"messageId", "name", "direction", "deliveries", "schemaId", "wireMode",
                    "nativeSize", "resourcePolicy", "legacyFields"});
    MessageDefinition result{};
    result.id = {static_cast<std::uint16_t>(number(object, "messageId", 1, 65535))};
    result.name = text(object, "name");
    result.direction = named<Direction>(object, "direction", directionNames);
    result.deliveries = deliveries(object);
    result.schema = {static_cast<std::uint16_t>(number(object, "schemaId", 1, 65535))};
    result.wireMode = named<WireMode>(object, "wireMode",
                                      std::array<std::string_view, 2>{"typed2", "nativeExact"});
    const auto &size = object.at("nativeSize");
    require(size.is_number_integer(), ErrorCode::invalidSchema);
    if (size.is_number_unsigned()) {
        require(size.get<std::uint64_t>() <= nativeUserMessageBytes, ErrorCode::invalidSchema);
    }
    const auto nativeSize = size.get<std::int64_t>();
    require(nativeSize >= -1 && nativeSize <= nativeUserMessageBytes, ErrorCode::invalidSchema);
    result.nativeSize = static_cast<std::int16_t>(nativeSize);
    result.resources = named<ResourcePolicy>(object, "resourcePolicy", resourcePolicyNames);
    for (const auto &field : list(object, "legacyFields")) {
        closed(field, {"fieldId", "bits", "signed"});
        result.legacyFields.push_back(
            {{static_cast<std::uint16_t>(number(field, "fieldId", 0, 65534))},
             static_cast<std::uint8_t>(number(field, "bits", 1, 32)),
             boolean(field, "signed")});
    }
    return result;
}
} // namespace
auto parseModules(const Json &root) -> std::vector<ModuleProfile> {
    std::vector<ModuleProfile> modules;
    for (const auto &object : list(root, "moduleProfiles")) {
        closed(object, {"moduleId", "name", "server", "client", "replayPolicy", "messages",
                        "localReplayBindings"});
        ModuleProfile module{};
        module.id = {static_cast<std::uint16_t>(number(object, "moduleId", 1, 65535))};
        module.name = text(object, "name");
        module.server = binding(object.at("server"));
        module.client = binding(object.at("client"));
        module.replay = named<ReplayPolicy>(
            object, "replayPolicy",
            std::array<std::string_view, 2>{"wireStateOnly", "typedLocalState"});
        for (const auto &item : list(object, "messages")) {
            module.messages.push_back(message(item));
        }
        for (const auto &item : list(object, "localReplayBindings")) {
            closed(item, {"recordKind", "schemaId", "originPolicy"});
            require(text(item, "originPolicy") == "localOnly", ErrorCode::invalidSchema);
            module.localReplay.push_back(
                {static_cast<std::uint16_t>(number(item, "recordKind", 1, 6)),
                 {static_cast<std::uint16_t>(number(item, "schemaId", 1, 65535))}});
        }
        std::ranges::sort(module.messages, {}, &MessageDefinition::id);
        modules.push_back(std::move(module));
    }
    std::ranges::sort(modules, {}, &ModuleProfile::id);
    return modules;
}
auto parseEffects(const Json &root) -> std::vector<EffectPolicy> {
    std::vector<EffectPolicy> policies;
    for (const auto &item : list(root, "effectPolicies", 256)) {
        closed(item, {"effectId", "ownerModuleId", "deliveries", "wireSchema", "schemaId",
                      "resourcePolicy"});
        policies.push_back(
            {{static_cast<std::uint16_t>(number(item, "effectId", 0, 255))},
             {static_cast<std::uint16_t>(number(item, "ownerModuleId", 1, 65535))},
             deliveries(item),
             named<EffectSchema>(item, "wireSchema",
                                 std::array<std::string_view, 2>{"builtin", "application"}),
             {static_cast<std::uint16_t>(number(item, "schemaId", 0, 65535))},
             named<ResourcePolicy>(item, "resourcePolicy", resourcePolicyNames)});
    }
    std::ranges::sort(policies, {}, &EffectPolicy::id);
    return policies;
}
auto parseServices(const Json &root) -> std::vector<ServicePolicy> {
    std::vector<ServicePolicy> policies;
    constexpr auto phaseNames =
        std::array<std::string_view, 4>{"negotiation", "bootstrap", "loading", "gameplay"};
    for (const auto &item : list(root, "servicePolicies", 512)) {
        closed(item, {"direction", "opcode", "mode", "phases", "grammarSha256", "maxBytes",
                      "ownerModuleId"});
        ServicePolicy policy{};
        policy.direction = named<Direction>(item, "direction", directionNames);
        policy.opcode = static_cast<std::uint8_t>(number(item, "opcode", 0, 255));
        policy.mode = named<ServiceMode>(
            item, "mode", std::array<std::string_view, 3>{"data2", "nativeExact", "forbidden"});
        for (const auto &phase : list(item, "phases", phaseNames.size())) {
            require(phase.is_string(), ErrorCode::invalidSchema);
            const auto found = std::ranges::find(phaseNames, phase.get_ref<const std::string &>());
            require(found != phaseNames.end(), ErrorCode::invalidSchema);
            const auto index = static_cast<std::size_t>(found - phaseNames.begin());
            require(!policy.phases[index], ErrorCode::invalidSchema);
            policy.phases[index] = true;
        }
        if (!item.at("grammarSha256").is_null()) {
            policy.grammar = digestValue(item, "grammarSha256");
        }
        policy.maxBytes = {
            static_cast<std::uint32_t>(number(item, "maxBytes", 0, maxMessageBytes))};
        policy.owner = {static_cast<std::uint16_t>(number(item, "ownerModuleId", 1, 65535))};
        policies.push_back(policy);
    }
    std::ranges::sort(policies, [](const ServicePolicy &left, const ServicePolicy &right) {
        return std::pair{left.direction, left.opcode} < std::pair{right.direction, right.opcode};
    });
    return policies;
}
} // namespace extendedPrecache::data::detail
