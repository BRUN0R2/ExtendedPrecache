#include <extendedPrecache/session/capabilities.hpp>

namespace extendedPrecache::session {
namespace {
constexpr auto signedCountMaximum = std::uint32_t{2147483647};
constexpr auto maximumValues = std::array<std::uint32_t, capabilityCount>{signedCountMaximum - 1,
                                                                          signedCountMaximum - 1,
                                                                          signedCountMaximum,
                                                                          65535,
                                                                          signedCountMaximum,
                                                                          signedCountMaximum,
                                                                          signedCountMaximum,
                                                                          signedCountMaximum,
                                                                          65536,
                                                                          signedCountMaximum,
                                                                          16777215,
                                                                          65536};
constexpr auto minimumValues =
    std::array<std::uint32_t, capabilityCount>{1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1};
} // namespace

auto CapabilityLimits::create(CapabilityVector values) noexcept
    -> std::expected<CapabilityLimits, Rejection> {
    for (std::size_t index = 0; index < capabilityCount; ++index) {
        const auto value = values.wireValues()[index];
        if (value < minimumValues[index] || value > maximumValues[index]) {
            return std::unexpected{Rejection{RejectCode::invalidConfiguration,
                                             static_cast<std::uint8_t>(index + 1), value,
                                             maximumValues[index]}};
        }
    }
    return CapabilityLimits{values};
}

auto MapRequirements::create(CapabilityVector values) noexcept
    -> std::expected<MapRequirements, Rejection> {
    const auto &input = values.wireValues();
    std::uint64_t total{};
    constexpr auto categoryCount = 5U;
    for (std::size_t index = 0; index < categoryCount; ++index) {
        total += input[index];
    }
    if (input[fieldIndex(CapabilityField::modelEntries)] == 0 ||
        total != input[fieldIndex(CapabilityField::resourceEntries)] ||
        input[fieldIndex(CapabilityField::consistencyEntries)] > total) {
        return std::unexpected{Rejection{RejectCode::invalidConfiguration}};
    }
    return MapRequirements{values};
}

auto intersectLimits(const CapabilityLimits &server, const CapabilityLimits &client) noexcept
    -> CapabilityLimits {
    std::array<std::uint32_t, capabilityCount> values{};
    for (std::size_t index = 0; index < capabilityCount; ++index) {
        values[index] =
            std::min(server.vector().wireValues()[index], client.vector().wireValues()[index]);
    }
    return CapabilityLimits{CapabilityVector{values}};
}

auto admitRequirements(const MapRequirements &requirements,
                       const CapabilityLimits &selected) noexcept
    -> std::expected<void, Rejection> {
    for (std::size_t index = 0; index < capabilityCount; ++index) {
        const auto needed = requirements.vector().wireValues()[index];
        const auto offered = selected.vector().wireValues()[index];
        if (needed > offered) {
            return std::unexpected{Rejection{RejectCode::capacityInsufficient,
                                             static_cast<std::uint8_t>(index + 1), offered,
                                             needed}};
        }
    }
    return {};
}

auto validateChunkBudget(const CapabilityLimits &limits, ByteCount framingBytes) noexcept
    -> std::expected<void, Rejection> {
    const auto required = std::uint64_t{limits.vector().get<CapabilityField::chunkBytes>().value} +
                          framingBytes.value;
    for (const auto field :
         {CapabilityField::reliableMessageBytes, CapabilityField::inFlightBytes}) {
        const auto offered = limits.vector().wireValues()[fieldIndex(field)];
        if (required > offered) {
            return std::unexpected{Rejection{
                RejectCode::invalidConfiguration, static_cast<std::uint8_t>(field), offered,
                static_cast<std::uint32_t>(std::min(required, std::uint64_t{signedCountMaximum}))}};
        }
    }
    return {};
}

auto legacyLimits() noexcept -> CapabilityLimits {
    return CapabilityLimits{CapabilityVector{
        {511, 511, 4095, 255, 512, 4095, 512, 4194304, 16384, 65536, 16777215, 65536}}};
}
} // namespace extendedPrecache::session
