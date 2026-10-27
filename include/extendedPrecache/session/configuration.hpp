#pragma once

#include <extendedPrecache/session/capabilities.hpp>
#include <filesystem>
#include <optional>

namespace extendedPrecache::session {

enum class ConfigurationError : std::uint8_t {
    invalidJson,
    duplicateKey,
    invalidNumberToken,
    invalidEncoding,
    schemaViolation,
    invalidDependency,
    metadataBudgetExceeded,
    inputTooLarge,
    ioFailure,
    roleMismatch,
    allocationFailure
};
struct ConfigurationFailure final {
    ConfigurationError code;
    std::string_view field{};
    std::size_t offset{};
};
struct LocalStorage final {
    ModelCacheEntries knownModels;
    LocalModelReserve localModels;
    SoundDescriptors soundDescriptors;
    LocalSoundReserve localSounds;
    HudSpriteEntries hudSprites;
};
struct NegotiationLimits final {
    DurationMs timeout;
    ByteCount maxBytes;
};
struct ConfigurationValues final {
    EndpointRole role;
    ConnectionPolicy policy;
    CapabilityLimits limits;
    LocalStorage storage;
    MetadataBytes metadataBudget;
    NegotiationLimits negotiation;
    std::optional<Digest> applicationContract;
};
class Configuration final {
  public:
    [[nodiscard]] static auto create(ConfigurationValues input)
        -> std::expected<Configuration, ConfigurationFailure>;
    [[nodiscard]] auto values() const noexcept -> const ConfigurationValues & {
        return data;
    }

  private:
    explicit Configuration(ConfigurationValues input) : data{input} {}
    ConfigurationValues data;
};
struct ConfigurationRequirements final {
    LocalModelReserve localModels;
    LocalSoundReserve localSounds;
    HudSpriteEntries hudSprites;
    ByteCount chunkOverhead;
};
[[nodiscard]] auto defaultConfiguration(EndpointRole role)
    -> std::expected<Configuration, ConfigurationFailure>;
[[nodiscard]] auto parseConfiguration(std::string_view text)
    -> std::expected<Configuration, ConfigurationFailure>;
[[nodiscard]] auto readConfiguration(const std::filesystem::path &path, EndpointRole expectedRole)
    -> std::expected<Configuration, ConfigurationFailure>;
[[nodiscard]] auto validateConfiguration(const Configuration &configuration,
                                         const ConfigurationRequirements &requirements) noexcept
    -> std::expected<MetadataBytes, ConfigurationFailure>;
[[nodiscard]] auto referenceMetadataBytes(const Configuration &configuration) noexcept
    -> MetadataBytes;

} // namespace extendedPrecache::session
