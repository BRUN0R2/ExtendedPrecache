#include <extendedPrecache/abi/extensionApi.hpp>

#include <extendedPrecache/projectVersion.hpp>

#include <utility>

extern "C" auto __cdecl extendedPrecacheQueryInfo(extendedPrecache::AbiVersion requestedAbiVersion,
                                                  std::uint32_t outputSize,
                                                  extendedPrecache::ExtensionInfo *output) noexcept
    -> extendedPrecache::QueryStatus {
    using namespace extendedPrecache;

    if (output == nullptr) {
        return QueryStatus::invalidArgument;
    }
    if (requestedAbiVersion != currentAbiVersion) {
        return QueryStatus::unsupportedAbi;
    }
    if (outputSize < sizeof(ExtensionInfo)) {
        return QueryStatus::outputTooSmall;
    }

    *output = ExtensionInfo{
        .structSize = sizeof(ExtensionInfo),
        .abiVersion = std::to_underlying(currentAbiVersion),
        .versionMajor = projectVersionMajor,
        .versionMinor = projectVersionMinor,
        .versionPatch = projectVersionPatch,
        .stage = std::to_underlying(RuntimeStage::foundation),
        .engineExtensionActive = 0,
    };
    return QueryStatus::success;
}
