#include <extendedPrecache/session/configuration.hpp>

#include <new>
#include <nlohmann/json.hpp>
#include <utility>

namespace extendedPrecache::session {
namespace {
using Json = nlohmann::json;
constexpr auto maximumObjectDepth = std::size_t{3};
constexpr auto signedMaximum = std::uint32_t{2147483647};
constexpr auto modelCacheMaximum = std::uint32_t{5478274};
constexpr auto soundCacheMaximum = std::uint32_t{29826161};
constexpr auto hudMaximum = std::uint32_t{178956970};

// SAX stops at a lexical violation instead of building a lossy DOM first.
class ConfigurationSax final : public nlohmann::json_sax<Json> {
  public:
    std::optional<Json> result;
    ConfigurationFailure failure{ConfigurationError::invalidJson};
    explicit ConfigurationSax(std::string_view input) : source{input} {}
    auto null() -> bool override {
        return append(nullptr);
    }
    auto boolean(bool value) -> bool override {
        return append(value);
    }
    auto number_integer(number_integer_t value) -> bool override {
        if (value == 0) {
            // Positive JSON integer tokens use number_unsigned; this is the token -0.
            failure.code = ConfigurationError::invalidNumberToken;
            return false;
        }
        return append(value);
    }
    auto number_unsigned(number_unsigned_t value) -> bool override {
        return append(value);
    }
    auto number_float(number_float_t, const string_t &) -> bool override {
        failure.code = ConfigurationError::invalidNumberToken;
        return false;
    }
    auto string(string_t &value) -> bool override {
        return append(std::move(value));
    }
    auto binary(binary_t &) -> bool override {
        failure.code = ConfigurationError::schemaViolation;
        return false;
    }
    auto start_object(std::size_t) -> bool override {
        if (objects.size() >= maximumObjectDepth) {
            failure.code = ConfigurationError::schemaViolation;
            return false;
        }
        objects.push_back({Json::object(), std::nullopt});
        return true;
    }
    auto key(string_t &value) -> bool override {
        if (objects.empty()) {
            return false;
        }
        if (objects.back().object.contains(value)) {
            failure.code = ConfigurationError::duplicateKey;
            return false;
        }
        objects.back().key = value;
        return true;
    }
    auto end_object() -> bool override {
        if (objects.empty()) {
            return false;
        }
        auto object = std::move(objects.back().object);
        objects.pop_back();
        return append(std::move(object));
    }
    auto start_array(std::size_t) -> bool override {
        failure.code = ConfigurationError::schemaViolation;
        return false;
    }
    auto end_array() -> bool override {
        return false;
    }
    auto parse_error(std::size_t position, const std::string &, const Json::exception &error)
        -> bool override {
        failure.offset = position;
        const auto text = std::string_view{error.what()};
        if (text.find("UTF-8") != std::string_view::npos ||
            text.find("surrogate") != std::string_view::npos) {
            failure.code = ConfigurationError::invalidEncoding;
        } else if (position > 0 && position <= source.size() &&
                   (source.substr(position - 1).starts_with("NaN") ||
                    source.substr(position - 1).starts_with("Infinity"))) {
            failure.code = ConfigurationError::invalidNumberToken;
        }
        return false;
    }

  private:
    struct Object final {
        Json object;
        std::optional<std::string> key;
    };
    std::vector<Object> objects;
    std::string_view source;
    auto append(Json value) -> bool {
        if (objects.empty()) {
            result = std::move(value);
            return true;
        }
        auto &parent = objects.back();
        if (!parent.key) {
            return false;
        }
        parent.object.emplace(std::move(*parent.key), std::move(value));
        parent.key.reset();
        return true;
    }
};

auto schemaFailure(std::string_view field) -> std::unexpected<ConfigurationFailure> {
    return std::unexpected{ConfigurationFailure{ConfigurationError::schemaViolation, field}};
}
auto closedObject(const Json &object, std::span<const std::string_view> required,
                  std::span<const std::string_view> optional, std::string_view field)
    -> std::expected<void, ConfigurationFailure> {
    if (!object.is_object()) {
        return schemaFailure(field);
    }
    for (const auto key : required) {
        if (!object.contains(key)) {
            return schemaFailure(field);
        }
    }
    for (const auto &item : object.items()) {
        if (std::ranges::find(required, item.key()) == required.end() &&
            std::ranges::find(optional, item.key()) == optional.end()) {
            return schemaFailure(field);
        }
    }
    return {};
}
auto unsignedValue(const Json &object, std::string_view key, std::uint32_t minimum,
                   std::uint32_t maximum, std::string_view field)
    -> std::expected<std::uint32_t, ConfigurationFailure> {
    const auto &value = object.at(key);
    if (!value.is_number_integer() || (value.is_number_integer() && !value.is_number_unsigned() &&
                                       value.get<std::int64_t>() < 0)) {
        return schemaFailure(field);
    }
    const auto number = value.get<std::uint64_t>();
    if (number < minimum || number > maximum) {
        return schemaFailure(field);
    }
    return static_cast<std::uint32_t>(number);
}
auto decodeConfiguration(const Json &root) -> std::expected<Configuration, ConfigurationFailure> {
    constexpr auto requiredRoot = std::array<std::string_view, 7>{
        "schemaVersion",       "role",       "connectionPolicy", "limits", "storage",
        "metadataBudgetBytes", "negotiation"};
    constexpr auto optionalRoot = std::array<std::string_view, 1>{"applicationContractSha256"};
    if (const auto valid = closedObject(root, requiredRoot, optionalRoot, ""); !valid) {
        return std::unexpected{valid.error()};
    }
    if (!unsignedValue(root, "schemaVersion", 1, 1, "schemaVersion")) {
        return schemaFailure("schemaVersion");
    }
    if (!root.at("role").is_string() || !root.at("connectionPolicy").is_string()) {
        return schemaFailure("role");
    }
    const auto &roleName = root.at("role").get_ref<const std::string &>();
    const auto &policyName = root.at("connectionPolicy").get_ref<const std::string &>();
    if (roleName != "server" && roleName != "client") {
        return schemaFailure("role");
    }
    const auto role = roleName == "server" ? EndpointRole::server : EndpointRole::client;
    ConnectionPolicy policy;
    if (policyName == "legacyOnly") {
        policy = ConnectionPolicy::legacyOnly;
    } else if (policyName == "preferExtended") {
        policy = ConnectionPolicy::preferExtended;
    } else if (policyName == "requireExtended") {
        policy = ConnectionPolicy::requireExtended;
    } else {
        return schemaFailure("connectionPolicy");
    }
    const auto &limitsObject = root.at("limits");
    if (const auto valid = closedObject(limitsObject, capabilityNames, {}, "limits"); !valid) {
        return std::unexpected{valid.error()};
    }
    std::array<std::uint32_t, capabilityCount> wireValues{};
    for (std::size_t index = 0; index < capabilityCount; ++index) {
        const auto value =
            unsignedValue(limitsObject, capabilityNames[index], 0, signedMaximum, "limits");
        if (!value) {
            return std::unexpected{value.error()};
        }
        wireValues[index] = *value;
    }
    const auto limits = CapabilityLimits::create(CapabilityVector{wireValues});
    if (!limits) {
        return schemaFailure("limits");
    }
    constexpr auto serverFields =
        std::array<std::string_view, 2>{"knownModelEntries", "localModelReserve"};
    constexpr auto clientFields = std::array<std::string_view, 5>{
        "knownModelEntries", "localModelReserve", "soundDescriptorEntries", "localSoundReserve",
        "hudSpriteEntries"};
    const auto &storageObject = root.at("storage");
    const auto fields = role == EndpointRole::server
                            ? std::span<const std::string_view>{serverFields}
                            : std::span<const std::string_view>{clientFields};
    if (const auto valid = closedObject(storageObject, fields, {}, "storage"); !valid) {
        return std::unexpected{valid.error()};
    }
    LocalStorage storage{};
    const auto models = unsignedValue(storageObject, "knownModelEntries", 1, modelCacheMaximum,
                                      "storage.knownModelEntries");
    const auto localModels = unsignedValue(storageObject, "localModelReserve", 0, modelCacheMaximum,
                                           "storage.localModelReserve");
    if (!models) {
        return std::unexpected{models.error()};
    }
    if (!localModels) {
        return std::unexpected{localModels.error()};
    }
    storage.knownModels = {*models};
    storage.localModels = {*localModels};
    if (role == EndpointRole::client) {
        const auto audio = unsignedValue(storageObject, "soundDescriptorEntries", 0,
                                         soundCacheMaximum, "storage.soundDescriptorEntries");
        const auto localAudio = unsignedValue(storageObject, "localSoundReserve", 0,
                                              soundCacheMaximum, "storage.localSoundReserve");
        const auto hud = unsignedValue(storageObject, "hudSpriteEntries", 0, hudMaximum,
                                       "storage.hudSpriteEntries");
        if (!audio) {
            return std::unexpected{audio.error()};
        }
        if (!localAudio) {
            return std::unexpected{localAudio.error()};
        }
        if (!hud) {
            return std::unexpected{hud.error()};
        }
        storage.soundDescriptors = {*audio};
        storage.localSounds = {*localAudio};
        storage.hudSprites = {*hud};
    }
    const auto &negotiation = root.at("negotiation");
    constexpr auto negotiationFields = std::array<std::string_view, 2>{"timeoutMs", "maxBytes"};
    if (const auto valid = closedObject(negotiation, negotiationFields, {}, "negotiation");
        !valid) {
        return std::unexpected{valid.error()};
    }
    const auto timeout = unsignedValue(negotiation, "timeoutMs", minTimeoutMs, maxTimeoutMs,
                                       "negotiation.timeoutMs");
    const auto maxBytes = unsignedValue(negotiation, "maxBytes", normalTranscriptBytes,
                                        maxTranscriptBytes, "negotiation.maxBytes");
    const auto metadata =
        unsignedValue(root, "metadataBudgetBytes", 1, signedMaximum, "metadataBudgetBytes");
    if (!timeout) {
        return std::unexpected{timeout.error()};
    }
    if (!maxBytes) {
        return std::unexpected{maxBytes.error()};
    }
    if (!metadata) {
        return std::unexpected{metadata.error()};
    }
    std::optional<Digest> application;
    if (root.contains("applicationContractSha256")) {
        const auto &value = root.at("applicationContractSha256");
        if (!value.is_string()) {
            return schemaFailure("applicationContractSha256");
        }
        const auto digest = parseDigest(value.get_ref<const std::string &>());
        if (!digest || digest->isZero()) {
            return schemaFailure("applicationContractSha256");
        }
        application = *digest;
    }
    return Configuration::create(
        {role, policy, *limits, storage, {*metadata}, {{*timeout}, {*maxBytes}}, application});
}
} // namespace

auto parseConfiguration(std::string_view text)
    -> std::expected<Configuration, ConfigurationFailure> {
    if (text.size() > maxConfigurationBytes) {
        return std::unexpected{ConfigurationFailure{ConfigurationError::inputTooLarge}};
    }
    if (const auto nul = text.find('\0'); nul != std::string_view::npos) {
        return std::unexpected{ConfigurationFailure{ConfigurationError::invalidJson, {}, nul}};
    }
    constexpr auto bom = std::string_view{"\xef\xbb\xbf"};
    if (text.starts_with(bom)) {
        return std::unexpected{ConfigurationFailure{ConfigurationError::invalidEncoding}};
    }
    try {
        ConfigurationSax parser{text};
        if (!Json::sax_parse(text.begin(), text.end(), &parser) || !parser.result) {
            return std::unexpected{parser.failure};
        }
        return decodeConfiguration(*parser.result);
    } catch (const std::bad_alloc &) {
        return std::unexpected{ConfigurationFailure{ConfigurationError::allocationFailure}};
    }
}
} // namespace extendedPrecache::session
