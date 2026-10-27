#pragma once

#include <cstdint>
#include <type_traits>

namespace extendedPrecache {

enum class AbiVersion : std::uint8_t { invalid = 0, v1 = 1 };

inline constexpr auto currentAbiVersion = AbiVersion::v1;

enum class QueryStatus : std::uint8_t {
    success = 0,
    invalidArgument = 1,
    unsupportedAbi = 2,
    outputTooSmall = 3
};

enum class RuntimeStage : std::uint8_t { foundation = 0 };

struct ExtensionInfo final {
    std::uint32_t structSize;
    std::uint32_t abiVersion;
    std::uint32_t versionMajor;
    std::uint32_t versionMinor;
    std::uint32_t versionPatch;
    std::uint32_t stage;
    std::uint32_t engineExtensionActive;
};

static_assert(std::is_standard_layout_v<ExtensionInfo>);
static_assert(std::is_trivially_copyable_v<ExtensionInfo>);
static_assert(sizeof(ExtensionInfo) == 7 * sizeof(std::uint32_t));

} // namespace extendedPrecache

#if defined(_WIN32)
#define extendedPrecacheCall __cdecl
#else
#define extendedPrecacheCall
#endif

extern "C" auto extendedPrecacheCall extendedPrecacheQueryInfo(
    extendedPrecache::AbiVersion requestedAbiVersion, std::uint32_t outputSize,
    extendedPrecache::ExtensionInfo *output) noexcept -> extendedPrecache::QueryStatus;

#undef extendedPrecacheCall
