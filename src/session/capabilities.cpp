#include <extendedPrecache/session/capabilities.hpp>

#include <extendedPrecache/domain/resourceTypes.hpp>

#include <utility>

namespace extendedPrecache::session {
namespace {
constexpr auto maxMessageBytes = std::uint32_t{65536};
constexpr auto maxFileBytes = std::uint32_t{16777215};

struct FieldRange final {
    std::uint32_t minimum;
    std::uint32_t maximum;
};
constexpr auto fieldRange(CapabilityField field) noexcept -> FieldRange {
    switch (field) {
    case CapabilityField::modelEntries:
        return {1, maxResourceEntries(ResourceKind::model)};
    case CapabilityField::soundEntries:
        return {0, maxResourceEntries(ResourceKind::sound)};
    case CapabilityField::genericEntries:
        return {0, maxResourceEntries(ResourceKind::generic)};
    case CapabilityField::eventEntries:
        return {0, maxResourceEntries(ResourceKind::event)};
    case CapabilityField::decalEntries:
        return {0, maxResourceEntries(ResourceKind::decal)};
    case CapabilityField::resourceEntries:
        return {1, signedMaximum};
    case CapabilityField::consistencyEntries:
        return {0, signedMaximum};
    case CapabilityField::manifestBytes:
    case CapabilityField::inFlightBytes:
        return {1, signedMaximum};
    case CapabilityField::chunkBytes:
    case CapabilityField::reliableMessageBytes:
        return {1, maxMessageBytes};
    case CapabilityField::fileBytes:
        return {1, maxFileBytes};
    }
    return {};
}
} // namespace

auto CapabilityLimits::create(CapabilityVector values) noexcept
    -> std::expected<CapabilityLimits, Rejection> {
    for (const auto field : capabilityFields) {
        const auto value = values.wireValues()[fieldIndex(field)];
        const auto range = fieldRange(field);
        if (value < range.minimum || value > range.maximum) {
            return std::unexpected{
                Rejection{RejectCode::invalidConfiguration, field, value,
                          value < range.minimum ? range.minimum : range.maximum}};
        }
    }
    return CapabilityLimits{values};
}

auto MapRequirements::create(CapabilityVector values) noexcept
    -> std::expected<MapRequirements, Rejection> {
    const auto total = std::uint64_t{values.get<CapabilityField::modelEntries>().value} +
                       values.get<CapabilityField::soundEntries>().value +
                       values.get<CapabilityField::genericEntries>().value +
                       values.get<CapabilityField::eventEntries>().value +
                       values.get<CapabilityField::decalEntries>().value;
    if (values.get<CapabilityField::modelEntries>().value == 0 ||
        total != values.get<CapabilityField::resourceEntries>().value ||
        values.get<CapabilityField::consistencyEntries>().value > total) {
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
    for (const auto field : capabilityFields) {
        const auto needed = requirements.vector().wireValues()[fieldIndex(field)];
        const auto offered = selected.vector().wireValues()[fieldIndex(field)];
        if (needed > offered) {
            return std::unexpected{
                Rejection{RejectCode::capacityInsufficient, field, offered, needed}};
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
                RejectCode::invalidConfiguration, field, offered,
                static_cast<std::uint32_t>(std::min(required, std::uint64_t{signedMaximum}))}};
        }
    }
    return {};
}

// The proven native limits from capabilities.md, in wire order.
auto legacyLimits() noexcept -> CapabilityLimits {
    return CapabilityLimits{CapabilityVector{{
        511,      // modelEntries
        511,      // soundEntries
        4095,     // genericEntries
        255,      // eventEntries
        512,      // decalEntries
        4095,     // resourceEntries
        512,      // consistencyEntries
        4194304,  // manifestBytes
        16384,    // chunkBytes
        65536,    // inFlightBytes
        16777215, // fileBytes
        65536,    // reliableMessageBytes
    }}};
}
} // namespace extendedPrecache::session
