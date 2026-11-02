#include "clientBuild.hpp"
#include "testSuite.hpp"

#include <extendedPrecache/session/negotiation.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace {
using extendedPrecache::platform::windows::BuildRefusal;
using extendedPrecache::platform::windows::EngineTable;
using extendedPrecache::platform::windows::nativeModelSlots;
using extendedPrecache::platform::windows::wireModelSlots;
using extendedPrecache::platform::windows::client::ClientBuild;
using extendedPrecache::platform::windows::client::identifyClientBuild;
using extendedPrecache::platform::windows::client::knownClientBuilds;
using extendedPrecache::session::parseDigest;
using extendedPrecache::tests::TestSuite;

constexpr std::uint32_t operandBytes = 4;

auto operandsOf(const ClientBuild &build) -> std::vector<std::uint32_t> {
    auto operands = std::vector<std::uint32_t>{};
    const auto addTable = [&](const EngineTable &table) {
        for (const auto &reference : table.references) {
            operands.push_back(reference.operandRva);
        }
    };
    addTable(build.modelTable.table);
    addTable(build.modelDescriptors.descriptors);
    addTable(build.modelDescriptors.crcRecords);
    for (const auto &bound : build.modelTable.bounds) {
        operands.push_back(bound.operandRva);
    }
    operands.push_back(build.modelDescriptors.cellCountBoundRva);
    return operands;
}

void checkData(TestSuite &suite, const ClientBuild &build) {
    std::ignore = suite.check(!build.executable.empty(), "a build names the program to start");
    std::ignore = suite.check(parseDigest(build.executableSha256).has_value(),
                              "the program's hash is a whole SHA-256");
    std::ignore = suite.check(parseDigest(build.engineSha256).has_value(),
                              "the engine's hash is a whole SHA-256");
    std::ignore = suite.check(build.slotsWithNativeDescriptors > nativeModelSlots,
                              "native descriptors still back more than the native table");
    std::ignore = suite.check(build.slotsWithNativeDescriptors <= wireModelSlots,
                              "no limit goes past what the wire can name");

    auto operands = operandsOf(build);
    std::ranges::sort(operands);
    const auto overlapping = std::ranges::adjacent_find(
        operands, [](std::uint32_t low, std::uint32_t high) { return high - low < operandBytes; });
    std::ignore = suite.check(overlapping == operands.end(),
                              "every operand to rewrite stands apart from the others");
}

void checkEnginesAreDistinct(TestSuite &suite) {
    auto engines = std::set<std::string_view>{};
    for (const auto &build : knownClientBuilds()) {
        engines.insert(build.engineSha256);
    }
    std::ignore = suite.check(engines.size() == knownClientBuilds().size(),
                              "an engine's hash selects exactly one build");
}

void checkRefusals(TestSuite &suite, const std::filesystem::path &ownFile) {
    const auto unknown = identifyClientBuild(ownFile);
    std::ignore = suite.check(!unknown && unknown.error() == BuildRefusal::unknownBuild,
                              "a readable file with another hash is refused as unknown");
    auto missingFile = ownFile;
    missingFile += ".missing";
    const auto missing = identifyClientBuild(missingFile);
    std::ignore = suite.check(!missing && missing.error() == BuildRefusal::unreadable,
                              "a file that cannot be read is told apart from an unknown one");
}
} // namespace

auto main(int, char **values) -> int {
    return extendedPrecache::tests::runTests([&] {
        auto suite = TestSuite{};
        std::ignore = suite.check(!knownClientBuilds().empty(), "at least one build is known");
        for (const auto &build : knownClientBuilds()) {
            checkData(suite, build);
        }
        checkEnginesAreDistinct(suite);
        checkRefusals(suite, std::filesystem::path{values[0]});
        return suite.finish();
    });
}
