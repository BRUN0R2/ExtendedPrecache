#pragma once

#include <extendedPrecache/session/sessionTypes.hpp>

namespace extendedPrecache::session {

class CapabilityVector final {
  public:
    explicit constexpr CapabilityVector(
        std::array<std::uint32_t, capabilityCount> wireValues) noexcept
        : values{wireValues} {}
    template <CapabilityField field>
    [[nodiscard]] constexpr auto get() const noexcept -> CapabilityValue<field> {
        static_assert(validField(field));
        return {values[fieldIndex(field)]};
    }
    [[nodiscard]] constexpr auto wireValues() const noexcept
        -> const std::array<std::uint32_t, capabilityCount> & {
        return values;
    }
    [[nodiscard]] auto operator==(const CapabilityVector &) const -> bool = default;

  private:
    std::array<std::uint32_t, capabilityCount> values;
};

class CapabilityLimits final {
  public:
    [[nodiscard]] static auto create(CapabilityVector values) noexcept
        -> std::expected<CapabilityLimits, Rejection>;
    [[nodiscard]] auto vector() const noexcept -> const CapabilityVector & {
        return values;
    }

  private:
    friend auto intersectLimits(const CapabilityLimits &, const CapabilityLimits &) noexcept
        -> CapabilityLimits;
    friend auto legacyLimits() noexcept -> CapabilityLimits;
    explicit CapabilityLimits(CapabilityVector input) noexcept : values{input} {}
    CapabilityVector values;
};

class MapRequirements final {
  public:
    [[nodiscard]] static auto create(CapabilityVector values) noexcept
        -> std::expected<MapRequirements, Rejection>;
    [[nodiscard]] auto vector() const noexcept -> const CapabilityVector & {
        return values;
    }

  private:
    explicit MapRequirements(CapabilityVector input) noexcept : values{input} {}
    CapabilityVector values;
};

[[nodiscard]] auto intersectLimits(const CapabilityLimits &server,
                                   const CapabilityLimits &client) noexcept -> CapabilityLimits;
[[nodiscard]] auto admitRequirements(const MapRequirements &requirements,
                                     const CapabilityLimits &selected) noexcept
    -> std::expected<void, Rejection>;
[[nodiscard]] auto validateChunkBudget(const CapabilityLimits &limits,
                                       ByteCount framingBytes) noexcept
    -> std::expected<void, Rejection>;
[[nodiscard]] auto legacyLimits() noexcept -> CapabilityLimits;

} // namespace extendedPrecache::session
