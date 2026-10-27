#include "sessionFixtures.hpp"
#include "testSuite.hpp"

#include <extendedPrecache/domain/resourceTypes.hpp>

#include <cstdint>
#pragma pack(push, 1)
#include "integrationAbiV2.h"
struct PackSentinel final {
    std::uint8_t first;
    std::uint32_t second;
};
#pragma pack(pop)

#include <concepts>
#include <limits>
#include <set>
#include <type_traits>

namespace {
using namespace extendedPrecache::tests;
using extendedPrecache::ResourceKind;

static_assert(sizeof(PackSentinel) == 5);
static_assert(std::is_standard_layout_v<EpcResourceRecordV2>);
static_assert(std::is_trivially_copyable_v<EpcIntegrationApiV2>);
static_assert(sizeof(EpcAbiStatusV2) == sizeof(std::uint32_t));
static_assert(!std::same_as<EpcGenerationV2, EpcLeaseSerialV2>);
static_assert(!std::same_as<EpcGenerationV2, EpcMapEpochV2>);
static_assert(!std::same_as<EpcMapKeyV2, EpcSnapshotHandleV2>);
static_assert(!std::same_as<EpcApiLeaseV2, EpcSnapshotHandleV2>);
static_assert(!std::same_as<EpcResourceRefV2, EpcSnapshotHandleV2>);
static_assert(std::same_as<decltype(EpcIntegrationApiV2{}.readResource), EpcReadResourceV2>);

struct SpecCategory final {
    std::string_view name;
    ResourceKind cppKind;
    std::uint32_t abiKind;
    std::uint32_t nativeType;
    std::uint64_t base;
    std::uint64_t minimum;
    std::uint64_t maximum;
};
constexpr auto signedMaximum = std::uint64_t{2147483647};
constexpr auto eventMaximum = std::uint64_t{65535};
constexpr auto categories = std::array{
    SpecCategory{"model", ResourceKind::model, epcKindModel, 2, 1, 1, signedMaximum - 1},
    SpecCategory{"sound", ResourceKind::sound, epcKindSound, 0, 1, 0, signedMaximum - 1},
    SpecCategory{"generic", ResourceKind::generic, epcKindGeneric, 4, 0, 0, signedMaximum},
    SpecCategory{"event", ResourceKind::event, epcKindEvent, 5, 1, 0, eventMaximum},
    SpecCategory{"decal", ResourceKind::decal, epcKindDecal, 3, 0, 0, signedMaximum}};

// These are reference equations for the document, not an adapter implementation.
auto referenceTranslation(const FixtureJson &item)
    -> std::expected<std::uint64_t, std::string_view> {
    const auto name = item.at("kind").get<std::string>();
    const auto found = std::ranges::find(categories, name, &SpecCategory::name);
    if (found == categories.end()) {
        return std::unexpected{"wrongKind"};
    }
    const auto entries = item.at("entries").get<std::uint64_t>();
    if (entries < found->minimum || entries > found->maximum) {
        return std::unexpected{"invalidCapacity"};
    }
    if (entries == 0) {
        return std::unexpected{"categoryDisabled"};
    }
    const auto input = item.at("input").get<std::uint64_t>();
    const auto direction = item.at("direction").get<std::string>();
    if (direction == "toCategory") {
        if (input == 0 || input > entries) {
            return std::unexpected{"outOfRange"};
        }
        return input - 1 + found->base;
    }
    if (direction != "toDomain") {
        throw std::runtime_error{"Unknown conversion direction"};
    }
    if (input < found->base || input - found->base >= entries) {
        return std::unexpected{"outOfRange"};
    }
    return input - found->base + 1;
}

void testReferenceEquations(TestSuite &suite, const FixtureJson &corpus) {
    for (std::size_t index = 0; index < categories.size(); ++index) {
        const auto &item = corpus.at("categories").at(index);
        const auto &expected = categories[index];
        static_cast<void>(suite.check(
            item.at("name").get_ref<const std::string &>() == expected.name &&
                item.at("cppKind") == static_cast<std::uint32_t>(expected.cppKind) &&
                item.at("abiKind") == expected.abiKind &&
                item.at("nativeType") == expected.nativeType && item.at("base") == expected.base &&
                item.at("minimum") == expected.minimum && item.at("maximum") == expected.maximum,
            "explicit category registry, without enum casts"));
    }
    for (const auto &item : corpus.at("indexCases")) {
        const auto actual = referenceTranslation(item);
        static_cast<void>(
            suite.check(item.contains("error")
                            ? !actual && actual.error() == item.at("error").get<std::string>()
                            : actual && *actual == item.at("value").get<std::uint64_t>(),
                        item.at("name").get<std::string>()));
    }
    for (const auto &item : corpus.at("slotCases")) {
        const auto category =
            std::ranges::find(categories, item.at("kind").get<std::string>(), &SpecCategory::name);
        if (!suite.check(category != categories.end(), "known category in slot fixture")) {
            continue;
        }
        const auto slots = item.at("entries").get<std::uint64_t>() + category->base;
        static_cast<void>(
            suite.check(slots == item.at("slots").get<std::uint64_t>() && slots <= signedMaximum,
                        "physical slots include only the category's sentinel"));
    }
    const auto &counts = corpus.at("ordinalCounts");
    for (const auto &item : corpus.at("ordinalCases")) {
        const auto category =
            std::ranges::find(categories, item.at("kind").get<std::string>(), &SpecCategory::name);
        if (!suite.check(category != categories.end(), "known category in ordinal fixture")) {
            continue;
        }
        const auto index = static_cast<std::size_t>(std::distance(categories.begin(), category));
        std::uint64_t prefix{};
        for (std::size_t before = 0; before < index; ++before) {
            prefix += counts.at(before).get<std::uint64_t>();
        }
        const auto domainId = item.at("domainId").get<std::uint64_t>();
        static_cast<void>(
            suite.check(domainId > 0 && domainId <= counts.at(index).get<std::uint64_t>() &&
                            prefix + domainId - 1 == item.at("ordinal").get<std::uint64_t>(),
                        "logical ordinal uses category prefix, not native index"));
    }
    for (const auto &item : corpus.at("inlineNames")) {
        const auto number = item.at("number").get<std::uint64_t>();
        const auto name = "*" + std::to_string(number);
        static_cast<void>(
            suite.check(number > 0 && name == item.at("name").get<std::string>() &&
                            name.size() + 1 == item.at("bytesWithNul").get<std::size_t>(),
                        "inline name bytes include star, all digits and NUL"));
    }
}

template <typename T>
void addLayout(FixtureJson &layouts, std::string_view name,
               std::initializer_list<std::pair<std::string_view, std::size_t>> fields) {
    auto &value = layouts[name];
    value["bytes"] = sizeof(T);
    value["alignment"] = alignof(T);
    value["fields"] = FixtureJson::object();
    for (const auto &[field, offset] : fields) {
        value["fields"][field] = offset;
    }
}

auto compiledLayouts() -> FixtureJson {
    auto layouts = FixtureJson::object();
    addLayout<EpcRuntimeNonceV2>(layouts, "EpcRuntimeNonceV2",
                                 {{"bytes", offsetof(EpcRuntimeNonceV2, bytes)}});
    addLayout<EpcGenerationV2>(
        layouts, "EpcGenerationV2",
        {{"low", offsetof(EpcGenerationV2, low)}, {"high", offsetof(EpcGenerationV2, high)}});
    addLayout<EpcLeaseSerialV2>(
        layouts, "EpcLeaseSerialV2",
        {{"low", offsetof(EpcLeaseSerialV2, low)}, {"high", offsetof(EpcLeaseSerialV2, high)}});
    addLayout<EpcMapEpochV2>(
        layouts, "EpcMapEpochV2",
        {{"low", offsetof(EpcMapEpochV2, low)}, {"high", offsetof(EpcMapEpochV2, high)}});
    addLayout<EpcMapKeyV2>(layouts, "EpcMapKeyV2",
                           {{"runtime", offsetof(EpcMapKeyV2, runtime)},
                            {"generation", offsetof(EpcMapKeyV2, generation)}});
    addLayout<EpcSnapshotHandleV2>(layouts, "EpcSnapshotHandleV2",
                                   {{"runtime", offsetof(EpcSnapshotHandleV2, runtime)},
                                    {"serial", offsetof(EpcSnapshotHandleV2, serial)}});
    addLayout<EpcApiLeaseV2>(layouts, "EpcApiLeaseV2",
                             {{"runtime", offsetof(EpcApiLeaseV2, runtime)},
                              {"serial", offsetof(EpcApiLeaseV2, serial)}});
    addLayout<EpcResourceRefV2>(layouts, "EpcResourceRefV2",
                                {{"map", offsetof(EpcResourceRefV2, map)},
                                 {"kind", offsetof(EpcResourceRefV2, kind)},
                                 {"domainId", offsetof(EpcResourceRefV2, domainId)}});
    addLayout<EpcMapInfoV2>(layouts, "EpcMapInfoV2",
                            {{"structSize", offsetof(EpcMapInfoV2, structSize)},
                             {"map", offsetof(EpcMapInfoV2, map)},
                             {"phase", offsetof(EpcMapInfoV2, phase)},
                             {"modelCount", offsetof(EpcMapInfoV2, modelCount)},
                             {"soundCount", offsetof(EpcMapInfoV2, soundCount)},
                             {"genericCount", offsetof(EpcMapInfoV2, genericCount)},
                             {"eventCount", offsetof(EpcMapInfoV2, eventCount)},
                             {"decalCount", offsetof(EpcMapInfoV2, decalCount)},
                             {"resourceCount", offsetof(EpcMapInfoV2, resourceCount)},
                             {"mapEpoch", offsetof(EpcMapInfoV2, mapEpoch)},
                             {"spawnCount", offsetof(EpcMapInfoV2, spawnCount)},
                             {"reserved", offsetof(EpcMapInfoV2, reserved)},
                             {"catalogSha256", offsetof(EpcMapInfoV2, catalogSha256)}});
    addLayout<EpcResourceRecordV2>(
        layouts, "EpcResourceRecordV2",
        {{"structSize", offsetof(EpcResourceRecordV2, structSize)},
         {"resource", offsetof(EpcResourceRecordV2, resource)},
         {"resourceOrdinal", offsetof(EpcResourceRecordV2, resourceOrdinal)},
         {"categoryIndex", offsetof(EpcResourceRecordV2, categoryIndex)},
         {"nameBytes", offsetof(EpcResourceRecordV2, nameBytes)},
         {"reserved", offsetof(EpcResourceRecordV2, reserved)}});
    addLayout<EpcIntegrationApiV2>(
        layouts, "EpcIntegrationApiV2",
        {{"structSize", offsetof(EpcIntegrationApiV2, structSize)},
         {"abiMajor", offsetof(EpcIntegrationApiV2, abiMajor)},
         {"abiMinor", offsetof(EpcIntegrationApiV2, abiMinor)},
         {"pointerBits", offsetof(EpcIntegrationApiV2, pointerBits)},
         {"role", offsetof(EpcIntegrationApiV2, role)},
         {"reserved", offsetof(EpcIntegrationApiV2, reserved)},
         {"context", offsetof(EpcIntegrationApiV2, context)},
         {"lease", offsetof(EpcIntegrationApiV2, lease)},
         {"getCurrentMap", offsetof(EpcIntegrationApiV2, getCurrentMap)},
         {"acquireSnapshot", offsetof(EpcIntegrationApiV2, acquireSnapshot)},
         {"findResource", offsetof(EpcIntegrationApiV2, findResource)},
         {"readResource", offsetof(EpcIntegrationApiV2, readResource)},
         {"copyResourceName", offsetof(EpcIntegrationApiV2, copyResourceName)},
         {"releaseSnapshot", offsetof(EpcIntegrationApiV2, releaseSnapshot)},
         {"releaseApi", offsetof(EpcIntegrationApiV2, releaseApi)}});
    return layouts;
}

void testLayouts(TestSuite &suite, const FixtureJson &corpus) {
    const auto layouts = compiledLayouts();
    for (const auto &item : corpus.at("abiLayouts")) {
        if (item.contains("pointerBits") &&
            sizeof(void *) * std::numeric_limits<unsigned char>::digits !=
                item.at("pointerBits").get<std::size_t>()) {
            static_cast<void>(
                suite.check(sizeof(EpcIntegrationApiV2) != item.at("bytes").get<std::size_t>(),
                            "x64 table is not declared compatible with x86 ABI"));
            continue;
        }
        const auto &actual = layouts.at(item.at("name").get<std::string>());
        static_cast<void>(suite.check(actual.at("bytes") == item.at("bytes") &&
                                          actual.at("alignment") == item.at("alignment") &&
                                          actual.at("fields") == item.at("fields"),
                                      item.at("name").get<std::string>()));
    }
    static_cast<void>(
        suite.check(corpus.at("integrationAbi").at("major") == epcIntegrationMajor &&
                        corpus.at("integrationAbi").at("minor") == epcIntegrationMinor &&
                        corpus.at("integrationAbi").at("pointerBits") == epcIntegrationPointerBits,
                    "integration version remains distinct from diagnostic/network versions"));
}

void testAcceptanceInventory(TestSuite &suite, const FixtureJson &corpus) {
    static_cast<void>(suite.check(corpus.at("specificationOnly") == true &&
                                      corpus.at("availability") == "unavailable",
                                  "specification does not approve an engine profile"));
    const auto phaseIds = std::array{epcMapEmpty,  epcMapBuilding, epcMapFrozen,  epcMapLoading,
                                     epcMapActive, epcMapRetiring, epcMapRetired, epcMapFailed};
    constexpr auto names = std::array<std::string_view, 8>{
        "empty", "building", "frozen", "loading", "active", "retiring", "retired", "failed"};
    for (std::size_t index = 0; index < names.size(); ++index) {
        static_cast<void>(suite.check(
            static_cast<std::size_t>(phaseIds[index]) == index &&
                corpus.at("mapPhases").at(index).get_ref<const std::string &>() == names[index],
            "map phase IDs match the declared ABI"));
    }
    std::set<std::string> seen;
    for (const auto &item : corpus.at("nativeAcceptance")) {
        static_cast<void>(suite.check(seen.insert(item.at("name").get<std::string>()).second &&
                                          item.at("nativeStatus") == "pending" &&
                                          !item.at("required").get<std::string>().empty(),
                                      "native requirement is unique, explicit and still pending"));
    }
    static_cast<void>(
        suite.check(seen.size() == 21, "all native acceptance requirements remain inventoried"));
}
} // namespace

auto main(int argc, char **argv) -> int {
    if (argc != 2) {
        return 2;
    }
    return runTests([&] {
        TestSuite suite;
        const auto corpus =
            readFixture(std::filesystem::path{argv[1]} / "contracts/integrationVectors.json");
        testReferenceEquations(suite, corpus);
        testLayouts(suite, corpus);
        testAcceptanceInventory(suite, corpus);
        return suite.finish();
    });
}
