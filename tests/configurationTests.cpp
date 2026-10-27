#include "sessionFixtures.hpp"
#include "testSuite.hpp"

#include <concepts>

namespace {
using namespace extendedPrecache::session;
using namespace extendedPrecache::tests;
static_assert(!std::convertible_to<DurationMs, ByteCount>);
static_assert(!std::same_as<CapabilityValue<CapabilityField::modelEntries>,
                            CapabilityValue<CapabilityField::soundEntries>>);
static_assert(!std::same_as<SessionNonce, ClientNonce>);

void testCorpus(TestSuite &suite, const std::filesystem::path &docs) {
    const auto corpus = readFixture(docs / "protocol/configurationVectors.json");
    const ConfigurationRequirements profile{{64}, {64}, {256}, {64}};
    for (const auto &item : corpus.at("cases")) {
        const auto source = docs / "protocol" / item.at("base").get<std::string>();
        const auto changed =
            changedFixture(readFixture(source), FixtureChanges{item.at("changes")});
        const auto parsed = parseConfiguration(changed.dump());
        const auto name = item.at("name").get<std::string>();
        const auto expected = item.at("expected").get<std::string>();
        if (expected == "schemaError") {
            static_cast<void>(suite.check(
                !parsed && parsed.error().code == ConfigurationError::schemaViolation, name));
            continue;
        }
        std::optional<ConfigurationFailure> failure;
        if (!parsed) {
            failure = parsed.error();
        } else if (const auto validated = validateConfiguration(*parsed, profile); !validated) {
            failure = validated.error();
        }
        if (expected == "semanticError") {
            static_cast<void>(suite.check(
                failure && failure->field == item.at("field").get<std::string>(), name));
        } else {
            static_cast<void>(suite.check(parsed && !failure, name));
        }
    }
    for (const auto &item : corpus.at("lexicalCases")) {
        const auto parsed = parseConfiguration(item.at("rawJson").get<std::string>());
        const auto expected = item.at("expected").get<std::string>();
        const auto error = expected == "duplicateKey" ? ConfigurationError::duplicateKey
                           : expected == "invalidNumberToken"
                               ? ConfigurationError::invalidNumberToken
                           : expected == "invalidEncoding" ? ConfigurationError::invalidEncoding
                                                           : ConfigurationError::invalidJson;
        static_cast<void>(suite.check(!parsed && parsed.error().code == error,
                                      item.at("name").get<std::string>()));
    }
}

void testBoundaries(TestSuite &suite, const std::filesystem::path &docs,
                    const std::filesystem::path &output) {
    const auto text = readText(docs / "examples/clientLegacy.json");
    auto embeddedNul = text;
    embeddedNul.push_back('\0');
    embeddedNul += "unparsed";
    static_cast<void>(suite.check(!parseConfiguration(embeddedNul),
                                  "embedded NUL cannot truncate validation before trailing input"));
    auto padded = text;
    padded.resize(maxConfigurationBytes, ' ');
    static_cast<void>(
        suite.check(parseConfiguration(padded).has_value(), "exact configuration byte bound"));
    padded.push_back(' ');
    const auto oversized = parseConfiguration(padded);
    static_cast<void>(
        suite.check(!oversized && oversized.error().code == ConfigurationError::inputTooLarge,
                    "reject oversized configuration"));
    for (const auto malformed : {"{} {}", "{\"role\":\"\xff\"}", "{\"a\":{\"b\":{\"c\":{}}}}", "[]",
                                 "true", "{\"x\":1,}"}) {
        static_cast<void>(
            suite.check(!parseConfiguration(malformed), "strict syntax, depth and shape"));
    }
    static_cast<void>(
        suite.check(!parseConfiguration("{\"schemaVersion\":1,\"\\u0073chemaVersion\":1}"),
                    "duplicate decoded keys"));
    const auto negativeZero = parseConfiguration("{\"schemaVersion\":-0}");
    static_cast<void>(suite.check(!negativeZero && negativeZero.error().code ==
                                                       ConfigurationError::invalidNumberToken,
                                  "reject signed zero before schema normalization"));
    const auto defaults = defaultConfiguration(EndpointRole::client).value();
    const auto example = parseConfiguration(text).value();
    static_cast<void>(suite.check(
        defaults.values().limits.vector() == example.values().limits.vector() &&
            defaults.values().storage.knownModels == example.values().storage.knownModels &&
            defaults.values().negotiation.timeout == example.values().negotiation.timeout,
        "documented defaults"));
    const auto missing = readConfiguration(output / "missing/config.json", EndpointRole::client);
    static_cast<void>(
        suite.check(missing && missing->values().policy == ConnectionPolicy::legacyOnly,
                    "missing file uses explicit role defaults"));
    const auto mismatch =
        readConfiguration(docs / "examples/clientLegacy.json", EndpointRole::server);
    static_cast<void>(
        suite.check(!mismatch && mismatch.error().code == ConfigurationError::roleMismatch,
                    "role mismatch never coerced"));
    const auto directory = readConfiguration(docs, EndpointRole::client);
    static_cast<void>(
        suite.check(!directory && directory.error().code == ConfigurationError::ioFailure,
                    "directory is not a missing configuration"));
    std::filesystem::create_directories(output);
    const auto malformedPath = output / "presentInvalid.json";
    {
        std::ofstream file{malformedPath, std::ios::binary};
        file << '{';
    }
    const auto invalid = readConfiguration(malformedPath, EndpointRole::client);
    static_cast<void>(
        suite.check(!invalid && invalid.error().code == ConfigurationError::invalidJson,
                    "invalid present file never falls back"));
    std::filesystem::remove(malformedPath);
    for (const auto &[name, expected] : std::array<std::pair<std::string_view, std::uint64_t>, 4>{
             {{"serverLegacy", 1568262},
              {"clientLegacy", 1325944},
              {"serverExtendedDraft", 10860103},
              {"clientExtendedDraft", 5452824}}}) {
        const auto configuration =
            parseConfiguration(readText(docs / "examples" / (std::string{name} + ".json"))).value();
        static_cast<void>(suite.check(referenceMetadataBytes(configuration).value == expected,
                                      "reference allocation arithmetic"));
    }
}

void testAdmission(TestSuite &suite, const std::filesystem::path &docs) {
    const auto vectors = readFixture(docs / "protocol/negotiationVectors.json");
    const auto server =
        parseConfiguration(readText(docs / "examples/serverExtendedDraft.json")).value();
    const auto client =
        parseConfiguration(readText(docs / "examples/clientExtendedDraft.json")).value();
    const auto selected = intersectLimits(server.values().limits, client.values().limits);
    static_cast<void>(suite.check(selected.vector() == fixtureVector(vectors.at("selectedLimits")),
                                  "exact minimum, not a cropped catalog"));
    const auto corpus = readFixture(docs / "protocol/capacityVectors.json");
    for (const auto &item : corpus.at("cases")) {
        const auto changed =
            changedFixture(vectors.at("requirements"), FixtureChanges{item.at("changes")});
        const auto requirement = MapRequirements::create(fixtureVector(changed));
        const auto expected = item.at("expected").get<std::string>();
        if (expected == "invalidSnapshot") {
            static_cast<void>(suite.check(!requirement, item.at("name").get<std::string>()));
            continue;
        }
        if (!suite.check(requirement.has_value(), "well-formed requirement fixture")) {
            continue;
        }
        const auto result = admitRequirements(*requirement, selected);
        if (expected == "admit") {
            static_cast<void>(suite.check(result.has_value(), item.at("name").get<std::string>()));
        } else {
            static_cast<void>(
                suite.check(!result && result.error().code == RejectCode::capacityInsufficient &&
                                result.error().fieldId == item.at("fieldId").get<std::uint8_t>(),
                            item.at("name").get<std::string>()));
        }
    }
}
} // namespace

auto main(int argc, char **argv) -> int {
    if (argc != 3) {
        return 2;
    }
    return runTests([&] {
        TestSuite suite;
        testCorpus(suite, argv[1]);
        testBoundaries(suite, argv[1], argv[2]);
        testAdmission(suite, argv[1]);
        return suite.finish();
    });
}
