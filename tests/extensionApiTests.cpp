#include <extendedPrecache/abi/extensionApi.hpp>

#include <windows.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <type_traits>
#include <utility>

#include "testSuite.hpp"

namespace {

using namespace extendedPrecache;
using extendedPrecache::tests::TestSuite;

using QueryInfo = decltype(&extendedPrecacheQueryInfo);

void testQuery(TestSuite &suite, QueryInfo query) {
    constexpr auto infoSize = static_cast<std::uint32_t>(sizeof(ExtensionInfo));
    constexpr auto unsupportedVersion = AbiVersion::invalid;
    constexpr auto sentinelValue = 0xA5A5A5A5U;
    ExtensionInfo output{
        .structSize = sentinelValue,
        .abiVersion = sentinelValue,
        .versionMajor = sentinelValue,
        .versionMinor = sentinelValue,
        .versionPatch = sentinelValue,
        .stage = std::to_underlying(RuntimeStage::foundation),
        .engineExtensionActive = sentinelValue,
    };

    static_cast<void>(
        suite.check(query(currentAbiVersion, infoSize, nullptr) == QueryStatus::invalidArgument,
                    "reject null output"));
    static_cast<void>(
        suite.check(query(unsupportedVersion, 0, nullptr) == QueryStatus::invalidArgument,
                    "null validation precedes ABI and size validation"));
    const auto originalBytes = std::bit_cast<std::array<std::byte, sizeof(ExtensionInfo)>>(output);
    static_cast<void>(
        suite.check(query(unsupportedVersion, infoSize, &output) == QueryStatus::unsupportedAbi,
                    "reject unsupported ABI"));
    static_cast<void>(
        suite.check(query(unsupportedVersion, 0, &output) == QueryStatus::unsupportedAbi,
                    "ABI validation precedes size validation"));
    static_cast<void>(
        suite.check(query(currentAbiVersion, infoSize - 1, &output) == QueryStatus::outputTooSmall,
                    "reject undersized output"));
    static_cast<void>(suite.check(
        std::bit_cast<std::array<std::byte, sizeof(ExtensionInfo)>>(output) == originalBytes,
        "rejected queries do not write output"));
    if (!suite.check(query(currentAbiVersion, infoSize, &output) == QueryStatus::success,
                     "query current ABI")) {
        return;
    }
    static_cast<void>(suite.check(output.structSize == infoSize, "report struct size"));
    static_cast<void>(suite.check(output.abiVersion == std::to_underlying(currentAbiVersion),
                                  "report ABI version"));
    static_cast<void>(suite.check(output.stage == std::to_underlying(RuntimeStage::foundation) &&
                                      output.engineExtensionActive == 0,
                                  "never claim engine extension is active"));

    constexpr auto guardValue = 0xA5A5A5A5U;
    struct GuardedOutput final {
        ExtensionInfo info;
        std::uint32_t guard;
    };
    GuardedOutput guarded{.info = {}, .guard = guardValue};
    static_cast<void>(suite.check(query(currentAbiVersion, sizeof(guarded), &guarded.info) ==
                                      QueryStatus::success,
                                  "accept larger output buffer"));
    static_cast<void>(suite.check(guarded.guard == guardValue, "write only defined ABI prefix"));
}

} // namespace

auto main(int argumentCount, char *arguments[]) -> int {
    static_assert(sizeof(void *) == 4, "The engine DLL and its test must be x86");
    TestSuite suite;
    if (!suite.check(argumentCount == 2, "provide DLL path")) {
        return suite.finish();
    }
    const auto path = std::filesystem::absolute(std::filesystem::path{arguments[1]});
    const auto library = std::unique_ptr<std::remove_pointer_t<HMODULE>, decltype(&FreeLibrary)>{
        LoadLibraryExW(path.c_str(), nullptr,
                       LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS),
        &FreeLibrary};
    if (!suite.check(library != nullptr, "load generated x86 DLL")) {
        std::cerr << "Windows error: " << GetLastError() << '\n';
        return suite.finish();
    }
    const auto symbol = GetProcAddress(library.get(), "extendedPrecacheQueryInfo");
    if (!suite.check(symbol != nullptr, "resolve undecorated C export")) {
        return suite.finish();
    }
    testQuery(suite, reinterpret_cast<QueryInfo>(symbol));
    return suite.finish();
}
