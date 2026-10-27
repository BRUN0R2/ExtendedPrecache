#include "sessionFixtures.hpp"
#include "testSuite.hpp"
#include <extendedPrecache/platform/windows/cngDigest.hpp>

namespace {
using namespace extendedPrecache::session;
using namespace extendedPrecache::tests;
using extendedPrecache::platform::windows::CngDigest;

void testFrames(TestSuite &suite, const FixtureJson &corpus) {
    for (const auto &item : corpus.at("frames")) {
        const auto bytes = decodeHex(item.at("frameHex").get<std::string>()).value();
        const auto parsed = decodeFrame(bytes);
        if (!suite.check(parsed.has_value(), "decode fixed reference frame")) {
            continue;
        }
        const auto kindName = item.at("kind").get<std::string>();
        static_cast<void>(
            suite.check(static_cast<std::uint8_t>(messageKind(parsed->payload)) ==
                            corpus.at("messageKinds").at(kindName).get<std::uint8_t>(),
                        "message tag and typed body"));
        static_cast<void>(
            suite.check(parsed->sequence.value == item.at("sequence").get<std::uint32_t>(),
                        "little endian sequence"));
        const auto encoded = encodeFrame(*parsed);
        static_cast<void>(suite.check(encoded && std::ranges::equal(encoded->bytes(), bytes),
                                      "encode matches independent golden bytes"));
        const auto command = encodeClientCommand(*encoded);
        static_cast<void>(
            suite.check(command.size() <= 125 && decodeClientCommand(command) == parsed,
                        "command transport preserves exact frame"));
        for (std::size_t size = 0; size < bytes.size(); ++size) {
            static_cast<void>(suite.check(!decodeFrame(std::span{bytes}.first(size)),
                                          "truncated frame is never partially accepted"));
        }
        auto trailing = bytes;
        trailing.push_back(std::byte{});
        static_cast<void>(suite.check(!decodeFrame(trailing), "reject trailing bytes"));
    }
    for (const auto &item : corpus.at("framingCases")) {
        const auto bytes = decodeHex(item.at("frameHex").get<std::string>()).value();
        const auto result = decodeFrame(bytes);
        const auto expected = item.at("expected").get<std::string>();
        static_cast<void>(suite.check(
            expected == "valid" ? result.has_value()
                                : !result && result.error().code == fixtureRejection(expected),
            item.at("name").get<std::string>()));
    }
    const auto first =
        decodeFrame(decodeHex(corpus.at("frames").at(0).at("frameHex").get<std::string>()).value())
            .value();
    auto bad = first;
    bad.sequence.value = maxFramesPerDirection;
    static_cast<void>(suite.check(!encodeFrame(bad), "no outbound sequence wrap"));
    bad = first;
    bad.nonce = {};
    static_cast<void>(suite.check(!encodeFrame(bad), "no zero nonce outbound"));
    bad = first;
    std::get<ServerOfferBody>(bad.payload).policy = ConnectionPolicy::legacyOnly;
    static_cast<void>(suite.check(!encodeFrame(bad), "legacy policy cannot emit an offer"));
    for (const auto command :
         {"epc1 ABCD", "epc1 0", "epc1 00 00", "epc1 00\n", "epc1;quit", "echo hi"}) {
        static_cast<void>(
            suite.check(!decodeClientCommand(command), "strict non-executable command grammar"));
    }
}

void testDigests(TestSuite &suite, const std::filesystem::path &docs, const FixtureJson &corpus) {
    CngDigest provider;
    static_cast<void>(suite.check(
        provider.sha256({}).value() ==
            parseDigest("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855").value(),
        "SHA-256 empty known answer"));
    const std::string abc{"abc"};
    static_cast<void>(suite.check(
        provider.sha256(std::as_bytes(std::span{abc})).value() ==
            parseDigest("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad").value(),
        "SHA-256 abc known answer"));
    for (const auto &[file, name] : std::array<std::pair<std::string_view, std::string_view>, 2>{
             {{"applicationContractExample.json", "applicationContractSha256"},
              {"dataContractExample.json", "dataContractSha256"}}}) {
        const auto bytes = readText(docs / "protocol" / file);
        static_cast<void>(suite.check(
            provider.sha256(std::as_bytes(std::span{bytes})).value() ==
                parseDigest(corpus.at("identities").at(name).get<std::string>()).value(),
            "artifact bytes hash without reserialization"));
    }
    const auto nonce = extendedPrecache::platform::windows::generateSessionNonce();
    const auto client = extendedPrecache::platform::windows::generateClientNonce();
    static_cast<void>(suite.check(nonce && !nonce->isZero() && client && !client->isZero(),
                                  "platform CSPRNG returns valid nonce types"));
}
} // namespace

auto main(int argc, char **argv) -> int {
    if (argc != 2) {
        return 2;
    }
    return runTests([&] {
        TestSuite suite;
        const auto corpus =
            readFixture(std::filesystem::path{argv[1]} / "protocol/negotiationVectors.json");
        testFrames(suite, corpus);
        testDigests(suite, argv[1], corpus);
        return suite.finish();
    });
}
