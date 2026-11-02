#pragma once

#include "buildIdentity.hpp"
#include "modelRelocation.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string_view>

// What the client adapter knows about each exact build of the game, as data. A position holds only
// for the hashes it sits next to, so one is never tried on a build it was not verified on.
namespace extendedPrecache::platform::windows::client {

struct EngineLayout final {
    std::ptrdiff_t serviceTable;
    std::ptrdiff_t serverInfoParser;
    std::ptrdiff_t registerUserMessageParser;
    std::ptrdiff_t engineFunctions;
    std::ptrdiff_t variableRegistration;
    std::ptrdiff_t frameEntry;
    std::ptrdiff_t messageData;
    std::ptrdiff_t messageSize;
    std::ptrdiff_t readCursor;
    std::ptrdiff_t secureServer;
};

struct ModelTableLayout final {
    EngineTable table;
    std::span<const CapacityBound> bounds;
};

struct ModelDescriptorLayout final {
    EngineTable descriptors;
    EngineTable crcRecords;
    std::uint32_t cellCountBoundRva;
};

struct ClientBuild final {
    std::wstring_view executable;
    std::string_view executableSha256;
    std::string_view engineSha256;
    EngineLayout engine;
    ModelTableLayout modelTable;
    ModelDescriptorLayout modelDescriptors;
    // The most a client can take while its 1,024 descriptors of known models stay in place. They
    // also hold what the client loads by itself, so this depends on the game.
    std::uint32_t slotsWithNativeDescriptors;
};

[[nodiscard]] auto knownClientBuilds() noexcept -> std::span<const ClientBuild>;

// The build whose engine is exactly this file. Nothing close counts: an unknown hash is refused.
[[nodiscard]] auto identifyClientBuild(const std::filesystem::path &engineFile)
    -> std::expected<const ClientBuild *, BuildRefusal>;

} // namespace extendedPrecache::platform::windows::client
