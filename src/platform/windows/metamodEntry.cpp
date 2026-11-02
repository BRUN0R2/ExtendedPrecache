#include <extendedPrecache/metamod/lifecycle.hpp>
#include <extendedPrecache/metamod/pluginAbi.hpp>

#include <extendedPrecache/projectVersion.hpp>

#include <windows.h>

#include <cstdlib>
#include <type_traits>

namespace {

using namespace extendedPrecache::metamod;

constexpr auto foundationNotice =
    "foundation build attached: precache extension inactive, no engine hooks installed";

constexpr auto pluginInfo = PluginInfo{
    .interfaceVersion = requiredInterfaceText,
    .name = "ExtendedPrecache foundation",
    .version = extendedPrecache::projectVersionText,
    .date = "unreleased",
    .author = "ExtendedPrecache contributors",
    .url = "https://github.com/BRUN0R2/ExtendedPrecache",
    .logTag = "EPC",
    // Map catalogs must be observed from their first precache, so later attachment is refused.
    .loadable = encodeLoadPhase(LoadPhase::startup),
    // Pausing would stop the callbacks that future bindings depend on while keeping them live.
    .unloadable = encodeLoadPhase(LoadPhase::anyTime),
};

// The host's entries carry no context, so its contract forces this single module-wide owner.
constinit PluginLifecycle lifecycle;
constinit const UtilityFunctions *hostUtilities{};

// The host's file logging is silent unless the server enabled it; state must stay visible.
void report(const char *text) noexcept {
    hostUtilities->logConsole(&pluginInfo, "[EPC] %s", text);
}

void reportError(const char *text) noexcept {
    report(text);
    hostUtilities->logError(&pluginInfo, "%s", text);
}

auto describe(LifecycleError error) noexcept -> const char * {
    switch (error) {
    case LifecycleError::malformedInterface:
        return "refused: the host reported a malformed interface version";
    case LifecycleError::incompatibleInterface:
        return "refused: the host interface is older or from another major version";
    case LifecycleError::unexpectedCall:
        return "refused: the host called an entry outside the expected order";
    case LifecycleError::unsupportedLoadPhase:
        return "refused: this plugin can only attach at server startup";
    case LifecycleError::abandoned:
        return "refused: a forced unload left live uses; restart the server to load again";
    case LifecycleError::usesExhausted:
        return "refused: the retained use counter is exhausted";
    }
    return "refused: unknown lifecycle error";
}

// Forced unloads ignore the refusal and free the module, so live uses must keep it mapped.
void pinModule() noexcept {
    auto module = HMODULE{};
    const auto flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN;
    if (GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(&pluginInfo), &module) == FALSE) {
        // Continuing would leave callers with pointers into an unmapped module.
        reportError("fatal: could not pin the module while uses are still live");
        std::abort();
    }
}

} // namespace

extern "C" void __stdcall GiveFnptrsToDll(const EngineFunctions *engine,
                                          GlobalVariables *globals) noexcept {
    if (engine != nullptr && globals != nullptr) {
        lifecycle.bindEngine();
    }
}

extern "C" auto Meta_Query(const char *hostInterface, const PluginInfo **info,
                           const UtilityFunctions *utilities) noexcept -> HostResult {
    if (info == nullptr) {
        return hostRefused;
    }
    // A host may keep going after a refused result when it finds an info structure.
    *info = nullptr;
    if (hostInterface == nullptr || utilities == nullptr) {
        return hostRefused;
    }
    hostUtilities = utilities;
    if (const auto queried = lifecycle.query(hostInterface); !queried) {
        reportError(describe(queried.error()));
        return hostRefused;
    }
    *info = &pluginInfo;
    return hostAccepted;
}

extern "C" auto Meta_Attach(HostLoadPhase now, HostFunctionTables *tables, HostGlobals *globals,
                            GameDllFunctions *gameDll) noexcept -> HostResult {
    if (hostUtilities == nullptr) {
        return hostRefused;
    }
    if (tables == nullptr || globals == nullptr || gameDll == nullptr) {
        reportError("refused: the host omitted a required attach table");
        return hostRefused;
    }
    const auto phase = decodeLoadPhase(now);
    if (!phase) {
        reportError(describe(LifecycleError::unsupportedLoadPhase));
        return hostRefused;
    }
    if (const auto attached = lifecycle.attach(*phase); !attached) {
        reportError(describe(attached.error()));
        return hostRefused;
    }
    report(foundationNotice);
    return hostAccepted;
}

extern "C" auto Meta_Detach(HostLoadPhase, HostUnloadReason reason) noexcept -> HostResult {
    switch (lifecycle.detach(decodeUnloadReason(reason))) {
    case DetachResult::released:
        return hostAccepted;
    case DetachResult::busy:
        reportError("unload refused: uses are still live");
        return hostRefused;
    case DetachResult::pinRequired:
        reportError("forced unload with live uses: module kept mapped until the process ends");
        pinModule();
        return hostRefused;
    }
    return hostRefused;
}

static_assert(std::is_same_v<decltype(&GiveFnptrsToDll), BindEngineEntry>);
static_assert(std::is_same_v<decltype(&Meta_Query), QueryEntry>);
static_assert(std::is_same_v<decltype(&Meta_Attach), AttachEntry>);
static_assert(std::is_same_v<decltype(&Meta_Detach), DetachEntry>);
