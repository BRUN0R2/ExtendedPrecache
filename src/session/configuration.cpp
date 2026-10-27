#include <extendedPrecache/session/configuration.hpp>

#include <fstream>
#include <limits>
#include <new>

namespace extendedPrecache::session {
namespace {
constexpr auto signedMaximum = std::uint32_t{2147483647};
constexpr auto modelDescriptorBytes = std::uint64_t{392};
constexpr auto modelCrcBytes = std::uint64_t{12};
constexpr auto soundDescriptorBytes = std::uint64_t{72};
constexpr auto hudDescriptorBytes = std::uint64_t{12};
constexpr auto eventDescriptorBytes = std::uint64_t{16};
constexpr auto resourceDescriptorBytes = std::uint64_t{136};
constexpr auto consistencyDescriptorBytes = std::uint64_t{44};
constexpr auto legacyResourceSlots = std::uint64_t{1280};
constexpr auto genericNameBytes = std::uint64_t{64};
constexpr auto nativeModelSlots = std::uint64_t{512};
constexpr auto nativeSoundSlots = std::uint64_t{512};
constexpr auto nativeEventSlots = std::uint64_t{256};
constexpr auto nativeDecalSlots = std::uint64_t{512};
constexpr auto nativeConsistencySlots = std::uint64_t{512};
constexpr auto nativeGenericSlots = std::uint64_t{4096};
constexpr auto nativeResourceSlots = std::uint64_t{4096};
constexpr auto nativeFixedResourceNameSlots = std::uint64_t{512};
constexpr auto nativeModelCacheEntries = ModelCacheEntries{1024};
constexpr auto nativeSoundDescriptors = SoundDescriptors{1024};
constexpr auto nativeHudEntries = HudSpriteEntries{256};
constexpr auto defaultModelReserve = LocalModelReserve{64};
constexpr auto defaultSoundReserve = LocalSoundReserve{64};
constexpr auto defaultMetadataBudget = MetadataBytes{16777216};
constexpr auto defaultTimeout = DurationMs{15000};

auto dependency(std::string_view field) -> std::unexpected<ConfigurationFailure> {
    return std::unexpected{ConfigurationFailure{ConfigurationError::invalidDependency, field}};
}
} // namespace

auto Configuration::create(ConfigurationValues input)
    -> std::expected<Configuration, ConfigurationFailure> {
    if (input.role != EndpointRole::server && input.role != EndpointRole::client) {
        return std::unexpected{ConfigurationFailure{ConfigurationError::schemaViolation, "role"}};
    }
    if (input.policy < ConnectionPolicy::legacyOnly ||
        input.policy > ConnectionPolicy::requireExtended) {
        return std::unexpected{
            ConfigurationFailure{ConfigurationError::schemaViolation, "connectionPolicy"}};
    }
    const auto &storage = input.storage;
    if (storage.knownModels.value == 0 ||
        storage.knownModels.value > signedMaximum / modelDescriptorBytes ||
        storage.localModels.value > signedMaximum / modelDescriptorBytes ||
        storage.soundDescriptors.value > signedMaximum / soundDescriptorBytes ||
        storage.localSounds.value > signedMaximum / soundDescriptorBytes ||
        storage.hudSprites.value > signedMaximum / hudDescriptorBytes) {
        return std::unexpected{
            ConfigurationFailure{ConfigurationError::schemaViolation, "storage"}};
    }
    if (input.metadataBudget.value == 0 || input.metadataBudget.value > signedMaximum ||
        input.negotiation.timeout.value < minTimeoutMs ||
        input.negotiation.timeout.value > maxTimeoutMs ||
        input.negotiation.maxBytes.value < normalTranscriptBytes ||
        input.negotiation.maxBytes.value > maxTranscriptBytes) {
        return std::unexpected{
            ConfigurationFailure{ConfigurationError::schemaViolation, "negotiation"}};
    }
    if ((input.applicationContract && input.applicationContract->isZero()) ||
        (input.policy != ConnectionPolicy::legacyOnly && !input.applicationContract)) {
        return std::unexpected{
            ConfigurationFailure{ConfigurationError::schemaViolation, "applicationContractSha256"}};
    }
    if (input.role == EndpointRole::server &&
        (storage.soundDescriptors.value != 0 || storage.localSounds.value != 0 ||
         storage.hudSprites.value != 0)) {
        return std::unexpected{
            ConfigurationFailure{ConfigurationError::schemaViolation, "storage"}};
    }
    if (input.policy == ConnectionPolicy::legacyOnly) {
        const auto native = legacyLimits().vector().wireValues();
        const auto &configured = input.limits.vector().wireValues();
        constexpr auto countFields = fieldIndex(CapabilityField::consistencyEntries) + 1;
        for (std::size_t index = 0; index < countFields; ++index) {
            if (configured[index] > native[index]) {
                return std::unexpected{
                    ConfigurationFailure{ConfigurationError::schemaViolation, "limits"}};
            }
        }
        if (storage.knownModels > nativeModelCacheEntries ||
            storage.soundDescriptors > nativeSoundDescriptors ||
            storage.hudSprites > nativeHudEntries) {
            return std::unexpected{
                ConfigurationFailure{ConfigurationError::schemaViolation, "storage"}};
        }
    }
    const auto requiredModels =
        std::uint64_t{input.limits.vector().get<CapabilityField::modelEntries>().value} +
        storage.localModels.value + storage.hudSprites.value;
    if (storage.knownModels.value < requiredModels) {
        return dependency("storage.knownModelEntries");
    }
    if (input.role == EndpointRole::client &&
        std::uint64_t{storage.soundDescriptors.value} <
            std::uint64_t{input.limits.vector().get<CapabilityField::soundEntries>().value} +
                storage.localSounds.value) {
        return dependency("storage.soundDescriptorEntries");
    }
    if (input.limits.vector().get<CapabilityField::consistencyEntries>().value >
        input.limits.vector().get<CapabilityField::resourceEntries>().value) {
        return dependency("limits.consistencyEntries");
    }
    if (const auto chunk = validateChunkBudget(input.limits, ByteCount{0}); !chunk) {
        return dependency(chunk.error().fieldId ==
                                  static_cast<std::uint8_t>(CapabilityField::inFlightBytes)
                              ? "limits.inFlightBytes"
                              : "limits.reliableMessageBytes");
    }
    Configuration result{input};
    if (referenceMetadataBytes(result).value > result.values().metadataBudget.value) {
        return std::unexpected{ConfigurationFailure{ConfigurationError::metadataBudgetExceeded,
                                                    "metadataBudgetBytes"}};
    }
    return result;
}

auto referenceMetadataBytes(const Configuration &configuration) noexcept -> MetadataBytes {
    const auto &config = configuration.values();
    const auto &values = config.limits.vector().wireValues();
    auto models = std::uint64_t{values[fieldIndex(CapabilityField::modelEntries)]} + 1;
    auto sounds = std::uint64_t{values[fieldIndex(CapabilityField::soundEntries)]} + 1;
    auto events = std::uint64_t{values[fieldIndex(CapabilityField::eventEntries)]} + 1;
    auto generics = std::uint64_t{values[fieldIndex(CapabilityField::genericEntries)]};
    auto decals = std::uint64_t{values[fieldIndex(CapabilityField::decalEntries)]};
    auto resources = std::uint64_t{values[fieldIndex(CapabilityField::resourceEntries)]};
    auto consistency = std::uint64_t{values[fieldIndex(CapabilityField::consistencyEntries)]};
    auto known = std::uint64_t{config.storage.knownModels.value};
    auto audio = std::uint64_t{config.storage.soundDescriptors.value};
    auto hud = std::uint64_t{config.storage.hudSprites.value};
    const auto legacy = config.policy == ConnectionPolicy::legacyOnly;
    const auto server = config.role == EndpointRole::server;
    if (legacy) {
        models = std::max(models, nativeModelSlots);
        sounds = std::max(sounds, nativeSoundSlots);
        events = std::max(events, nativeEventSlots);
        decals = std::max(decals, nativeDecalSlots);
        consistency = std::max(consistency, nativeConsistencySlots);
        known = std::max(known, std::uint64_t{nativeModelCacheEntries.value});
        if (server) {
            generics = std::max(generics, nativeGenericSlots);
            resources = std::max(resources, nativeResourceSlots);
        } else {
            audio = std::max(audio, std::uint64_t{nativeSoundDescriptors.value});
            hud = std::max(hud, std::uint64_t{nativeHudEntries.value});
        }
    }
    // Scalar limits bound every product below uint64_t; no intermediate uses x86 size_t.
    auto total = eventDescriptorBytes * events + resourceDescriptorBytes * resources +
                 consistencyDescriptorBytes * consistency +
                 (modelDescriptorBytes + modelCrcBytes) * known;
    const auto legacyResourceBytes = legacyResourceSlots * resourceDescriptorBytes;
    if (server) {
        constexpr auto modelNetworkBytes = std::uint64_t{9};
        constexpr auto modelMapBytesPerSlot = std::uint64_t{32};
        constexpr auto modelMapHeaderBytes = std::uint64_t{520};
        constexpr auto decalServerBytes = std::uint64_t{17};
        constexpr auto pointerBytes = std::uint64_t{4};
        constexpr auto hashEntryBytes = std::uint64_t{2};
        total += modelNetworkBytes * models + pointerBytes * sounds +
                 hashEntryBytes * (2 * sounds - 1) + genericNameBytes * generics +
                 decalServerBytes * decals + modelMapBytesPerSlot * models + modelMapHeaderBytes +
                 legacyResourceBytes +
                 (pointerBytes + genericNameBytes) * nativeFixedResourceNameSlots;
    } else {
        constexpr auto pointerBytes = std::uint64_t{4};
        constexpr auto decalClientBytes = std::uint64_t{16};
        total += pointerBytes * models + pointerBytes * sounds + decalClientBytes * decals +
                 soundDescriptorBytes * audio + hudDescriptorBytes * hud + legacyResourceBytes;
    }
    if (legacy) {
        total += config.limits.vector().get<CapabilityField::reliableMessageBytes>().value;
    } else {
        total += config.limits.vector().get<CapabilityField::manifestBytes>().value +
                 std::uint64_t{config.limits.vector().get<CapabilityField::inFlightBytes>().value} +
                 config.negotiation.maxBytes.value;
    }
    return {total};
}

auto validateConfiguration(const Configuration &configuration,
                           const ConfigurationRequirements &requirements) noexcept
    -> std::expected<MetadataBytes, ConfigurationFailure> {
    const auto &config = configuration.values();
    if (config.storage.localModels.value < requirements.localModels.value) {
        return dependency("storage.localModelReserve");
    }
    if (config.role == EndpointRole::client) {
        if (config.storage.localSounds.value < requirements.localSounds.value) {
            return dependency("storage.localSoundReserve");
        }
        if (config.storage.hudSprites.value < requirements.hudSprites.value) {
            return dependency("storage.hudSpriteEntries");
        }
    }
    const auto overhead =
        config.policy == ConnectionPolicy::legacyOnly ? ByteCount{0} : requirements.chunkOverhead;
    if (const auto chunk = validateChunkBudget(config.limits, overhead); !chunk) {
        return dependency(chunk.error().fieldId ==
                                  static_cast<std::uint8_t>(CapabilityField::inFlightBytes)
                              ? "limits.inFlightBytes"
                              : "limits.reliableMessageBytes");
    }
    const auto bytes = referenceMetadataBytes(configuration);
    if (bytes.value > config.metadataBudget.value) {
        return std::unexpected{ConfigurationFailure{ConfigurationError::metadataBudgetExceeded,
                                                    "metadataBudgetBytes"}};
    }
    return bytes;
}

auto defaultConfiguration(EndpointRole role) -> std::expected<Configuration, ConfigurationFailure> {
    LocalStorage storage{nativeModelCacheEntries, defaultModelReserve, {}, {}, {}};
    if (role == EndpointRole::client) {
        storage.soundDescriptors = nativeSoundDescriptors;
        storage.localSounds = defaultSoundReserve;
        storage.hudSprites = nativeHudEntries;
    }
    return Configuration::create({role,
                                  ConnectionPolicy::legacyOnly,
                                  legacyLimits(),
                                  storage,
                                  defaultMetadataBudget,
                                  {defaultTimeout, {maxTranscriptBytes}},
                                  std::nullopt});
}

auto readConfiguration(const std::filesystem::path &path, EndpointRole expectedRole)
    -> std::expected<Configuration, ConfigurationFailure> try {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        return std::unexpected{ConfigurationFailure{ConfigurationError::ioFailure}};
    }
    if (status.type() == std::filesystem::file_type::not_found) {
        return defaultConfiguration(expectedRole);
    }
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::unexpected{ConfigurationFailure{ConfigurationError::ioFailure}};
    }
    std::string text(maxConfigurationBytes + 1, '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (input.bad()) {
        return std::unexpected{ConfigurationFailure{ConfigurationError::ioFailure}};
    }
    text.resize(static_cast<std::size_t>(input.gcount()));
    auto result = parseConfiguration(text);
    if (result && result->values().role != expectedRole) {
        return std::unexpected{ConfigurationFailure{ConfigurationError::roleMismatch, "role"}};
    }
    return result;
} catch (const std::bad_alloc &) {
    return std::unexpected{ConfigurationFailure{ConfigurationError::allocationFailure}};
}
} // namespace extendedPrecache::session
