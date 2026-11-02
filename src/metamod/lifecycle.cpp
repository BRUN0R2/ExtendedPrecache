#include <extendedPrecache/metamod/lifecycle.hpp>

#include <charconv>
#include <limits>
#include <memory>
#include <system_error>

namespace extendedPrecache::metamod {
namespace {
constexpr auto interfaceSeparator = ':';

auto parseNumber(std::string_view text) noexcept -> std::optional<std::uint32_t> {
    auto value = std::uint32_t{};
    const auto *const last = std::to_address(text.end());
    const auto parsed = std::from_chars(std::to_address(text.begin()), last, value);
    if (parsed.ec != std::errc{} || parsed.ptr != last) {
        return std::nullopt;
    }
    return value;
}

// For these reasons the host ignores a refused detach and frees the module anyway; an unknown
// reason is treated the same way because assuming otherwise could leave callers dangling.
constexpr auto hostOverridesRefusal(UnloadReason reason) noexcept -> bool {
    return reason == UnloadReason::commandForced || reason == UnloadReason::pluginForced ||
           reason == UnloadReason::reload || reason == UnloadReason::unrecognized;
}
} // namespace

auto parseInterfaceVersion(std::string_view text) noexcept -> std::optional<InterfaceVersion> {
    const auto separator = text.find(interfaceSeparator);
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const auto majorVersion = parseNumber(text.substr(0, separator));
    const auto minorVersion = parseNumber(text.substr(separator + 1));
    if (!majorVersion || !minorVersion) {
        return std::nullopt;
    }
    return InterfaceVersion{.majorVersion = *majorVersion, .minorVersion = *minorVersion};
}

auto RetainedUse::operator=(RetainedUse &&other) noexcept -> RetainedUse & {
    if (this != &other) {
        reset();
        owner = std::exchange(other.owner, nullptr);
    }
    return *this;
}

RetainedUse::~RetainedUse() {
    reset();
}

void RetainedUse::reset() noexcept {
    if (owner != nullptr) {
        --owner->retainedUseCount;
        owner = nullptr;
    }
}

void PluginLifecycle::bindEngine() noexcept {
    if (currentPhase != PluginPhase::attached && currentPhase != PluginPhase::abandoned) {
        currentPhase = PluginPhase::engineBound;
    }
}

auto PluginLifecycle::query(std::string_view hostInterface) noexcept
    -> std::expected<void, LifecycleError> {
    if (currentPhase == PluginPhase::abandoned) {
        return std::unexpected{LifecycleError::abandoned};
    }
    if (currentPhase != PluginPhase::engineBound) {
        return std::unexpected{LifecycleError::unexpectedCall};
    }
    const auto host = parseInterfaceVersion(hostInterface);
    if (!host) {
        return std::unexpected{LifecycleError::malformedInterface};
    }
    if (host->majorVersion != requiredInterface.majorVersion ||
        host->minorVersion < requiredInterface.minorVersion) {
        return std::unexpected{LifecycleError::incompatibleInterface};
    }
    currentPhase = PluginPhase::queried;
    return {};
}

auto PluginLifecycle::attach(LoadPhase now) noexcept -> std::expected<void, LifecycleError> {
    if (currentPhase == PluginPhase::abandoned) {
        return std::unexpected{LifecycleError::abandoned};
    }
    if (currentPhase != PluginPhase::queried) {
        return std::unexpected{LifecycleError::unexpectedCall};
    }
    if (now != LoadPhase::startup) {
        return std::unexpected{LifecycleError::unsupportedLoadPhase};
    }
    currentPhase = PluginPhase::attached;
    return {};
}

auto PluginLifecycle::detach(UnloadReason reason) noexcept -> DetachResult {
    if (currentPhase == PluginPhase::abandoned) {
        return DetachResult::pinRequired;
    }
    if (currentPhase == PluginPhase::attached && retainedUseCount != 0) {
        if (!hostOverridesRefusal(reason)) {
            return DetachResult::busy;
        }
        currentPhase = PluginPhase::abandoned;
        return DetachResult::pinRequired;
    }
    currentPhase = PluginPhase::unbound;
    return DetachResult::released;
}

auto PluginLifecycle::retainUse() noexcept -> std::expected<RetainedUse, LifecycleError> {
    if (currentPhase != PluginPhase::attached) {
        return std::unexpected{LifecycleError::unexpectedCall};
    }
    if (retainedUseCount == std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected{LifecycleError::usesExhausted};
    }
    ++retainedUseCount;
    return RetainedUse{*this};
}

} // namespace extendedPrecache::metamod
