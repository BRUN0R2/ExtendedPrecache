#pragma once

#include <compare>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <utility>

namespace extendedPrecache::metamod {

// Values and order mirror the host's plugin contract, which compares them numerically.
enum class LoadPhase : std::uint8_t { never, startup, changeLevel, anyTime, anyPause };
enum class UnloadReason : std::uint8_t {
    none,
    iniDeleted,
    fileNewer,
    command,
    commandForced,
    delayed,
    plugin,
    pluginForced,
    reload,
    unrecognized
};

struct InterfaceVersion final {
    std::uint32_t majorVersion{};
    std::uint32_t minorVersion{};
    auto operator<=>(const InterfaceVersion &) const = default;
};

inline constexpr auto requiredInterface = InterfaceVersion{.majorVersion = 5, .minorVersion = 13};
inline constexpr auto requiredInterfaceText = "5:13";

[[nodiscard]] auto parseInterfaceVersion(std::string_view text) noexcept
    -> std::optional<InterfaceVersion>;

enum class PluginPhase : std::uint8_t { unbound, engineBound, queried, attached, abandoned };
enum class LifecycleError : std::uint8_t {
    malformedInterface,
    incompatibleInterface,
    unexpectedCall,
    unsupportedLoadPhase,
    abandoned,
    usesExhausted
};
enum class DetachResult : std::uint8_t { released, busy, pinRequired };

class PluginLifecycle;

// Keeps unload refused while something outlives the current call into the plugin.
class RetainedUse final {
  public:
    RetainedUse(const RetainedUse &) = delete;
    auto operator=(const RetainedUse &) -> RetainedUse & = delete;
    RetainedUse(RetainedUse &&other) noexcept : owner{std::exchange(other.owner, nullptr)} {}
    auto operator=(RetainedUse &&other) noexcept -> RetainedUse &;
    ~RetainedUse();

  private:
    friend class PluginLifecycle;
    explicit RetainedUse(PluginLifecycle &lifecycle) noexcept : owner{&lifecycle} {}
    void reset() noexcept;

    PluginLifecycle *owner;
};

// Calls are serialized on the engine's thread by the host contract.
class PluginLifecycle final {
  public:
    constexpr PluginLifecycle() noexcept = default;
    PluginLifecycle(const PluginLifecycle &) = delete;
    auto operator=(const PluginLifecycle &) -> PluginLifecycle & = delete;

    // Has no effect once attached or abandoned; the next query reports that state.
    void bindEngine() noexcept;
    [[nodiscard]] auto query(std::string_view hostInterface) noexcept
        -> std::expected<void, LifecycleError>;
    [[nodiscard]] auto attach(LoadPhase now) noexcept -> std::expected<void, LifecycleError>;
    [[nodiscard]] auto detach(UnloadReason reason) noexcept -> DetachResult;
    [[nodiscard]] auto retainUse() noexcept -> std::expected<RetainedUse, LifecycleError>;
    [[nodiscard]] constexpr auto phase() const noexcept -> PluginPhase {
        return currentPhase;
    }
    [[nodiscard]] constexpr auto retainedUses() const noexcept -> std::uint32_t {
        return retainedUseCount;
    }

  private:
    friend class RetainedUse;

    PluginPhase currentPhase{PluginPhase::unbound};
    std::uint32_t retainedUseCount{};
};

} // namespace extendedPrecache::metamod
