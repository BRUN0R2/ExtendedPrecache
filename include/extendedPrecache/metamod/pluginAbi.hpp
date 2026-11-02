#pragma once

#include <extendedPrecache/metamod/lifecycle.hpp>

#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

namespace extendedPrecache::metamod {

// Tables owned by the host; this plugin only checks their presence.
struct EngineFunctions;
struct GlobalVariables;
struct HostFunctionTables;
struct HostGlobals;
struct GameDllFunctions;

// The host's C enums and truth values are int-sized on this ABI.
using HostResult = std::int32_t;
using HostLoadPhase = std::int32_t;
using HostUnloadReason = std::int32_t;
inline constexpr HostResult hostRefused = 0;
inline constexpr HostResult hostAccepted = 1;

struct PluginInfo final {
    const char *interfaceVersion;
    const char *name;
    const char *version;
    const char *date;
    const char *author;
    const char *url;
    const char *logTag;
    HostLoadPhase loadable;
    HostLoadPhase unloadable;
};

using LogFunction = void (*)(const PluginInfo *plugin, const char *format, ...);

// Declares the leading entries used here; the host owns the rest of its larger table.
struct UtilityFunctions final {
    LogFunction logConsole;
    LogFunction logMessage;
    LogFunction logError;
};

static_assert(std::is_standard_layout_v<PluginInfo> && std::is_trivially_copyable_v<PluginInfo>);
static_assert(std::is_standard_layout_v<UtilityFunctions>);
static_assert(sizeof(void *) != 4 || sizeof(PluginInfo) == 36, "The host's x86 layout is fixed");

[[nodiscard]] constexpr auto encodeLoadPhase(LoadPhase phase) noexcept -> HostLoadPhase {
    return std::to_underlying(phase);
}
[[nodiscard]] constexpr auto decodeLoadPhase(HostLoadPhase raw) noexcept
    -> std::optional<LoadPhase> {
    if (raw < encodeLoadPhase(LoadPhase::never) || raw > encodeLoadPhase(LoadPhase::anyPause)) {
        return std::nullopt;
    }
    return static_cast<LoadPhase>(raw);
}
[[nodiscard]] constexpr auto decodeUnloadReason(HostUnloadReason raw) noexcept -> UnloadReason {
    if (raw < std::to_underlying(UnloadReason::none) ||
        raw >= std::to_underlying(UnloadReason::unrecognized)) {
        return UnloadReason::unrecognized;
    }
    return static_cast<UnloadReason>(raw);
}

#if defined(_WIN32)
#define extendedPrecacheHostCall __stdcall
#else
#define extendedPrecacheHostCall
#endif

using BindEngineEntry = void(extendedPrecacheHostCall *)(const EngineFunctions *engine,
                                                         GlobalVariables *globals) noexcept;
#undef extendedPrecacheHostCall
using QueryEntry = HostResult (*)(const char *hostInterface, const PluginInfo **info,
                                  const UtilityFunctions *utilities) noexcept;
using AttachEntry = HostResult (*)(HostLoadPhase now, HostFunctionTables *tables,
                                   HostGlobals *globals, GameDllFunctions *gameDll) noexcept;
using DetachEntry = HostResult (*)(HostLoadPhase now, HostUnloadReason reason) noexcept;

} // namespace extendedPrecache::metamod
