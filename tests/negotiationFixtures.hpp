#pragma once

#include "sessionFixtures.hpp"
#include "testSuite.hpp"

#include <extendedPrecache/platform/windows/cngDigest.hpp>
#include <extendedPrecache/session/negotiation.hpp>

namespace extendedPrecache::tests {
inline auto requireOutbound(NegotiationSession &session, SessionContext context) -> EncodedFrame {
    const auto frame = session.prepareOutbound(context).value();
    if (!frame) {
        throw std::runtime_error{"Expected outbound frame"};
    }
    return *frame;
}
struct PreparationProbe final {
    bool valid{true};
    bool allocationFailure{};
    bool nullLease{};
    std::optional<MetadataBytes> quote;
    std::optional<Rejection> failure;
    std::size_t calls{};
    std::size_t active{};
};
class SyntheticLease final : public PreparedSession {
  public:
    SyntheticLease(PreparationProbe &input, MetadataBytes bytes) : probe{input}, peak{bytes} {
        ++probe.active;
    }
    ~SyntheticLease() override {
        --probe.active;
    }
    auto peakMetadataBytes() const noexcept -> MetadataBytes override {
        return peak;
    }
    auto stillValid(const SessionAgreement &) const noexcept -> bool override {
        return probe.valid;
    }

  private:
    PreparationProbe &probe;
    MetadataBytes peak;
};
class SyntheticPreparer final : public SessionPreparer {
  public:
    PreparationProbe probe;
    auto prepare(const Configuration &config, const SessionAgreement &)
        -> std::expected<std::unique_ptr<PreparedSession>, Rejection> override {
        ++probe.calls;
        if (probe.allocationFailure) {
            throw std::bad_alloc{};
        }
        if (probe.failure) {
            return std::unexpected{*probe.failure};
        }
        if (probe.nullLease) {
            return std::unique_ptr<PreparedSession>{};
        }
        return std::make_unique<SyntheticLease>(
            probe, probe.quote.value_or(minimumSessionMetadataBytes(config)));
    }
};

class DuplexFixture final {
  public:
    FixtureJson corpus;
    extendedPrecache::platform::windows::CngDigest digests;
    SyntheticPreparer serverStorage;
    SyntheticPreparer clientStorage;
    Configuration serverConfig;
    Configuration clientConfig;
    ContractProfile profile;
    SessionContext serverContext;
    SessionContext clientContext;
    MapSnapshot map;
    NegotiationSession server;
    NegotiationSession client;
    std::size_t progress{};

    explicit DuplexFixture(const std::filesystem::path &docs)
        : corpus(readFixture(docs / "protocol/negotiationVectors.json")),
          serverConfig{
              parseConfiguration(readText(docs / "examples/serverExtendedDraft.json")).value()},
          clientConfig{
              parseConfiguration(readText(docs / "examples/clientExtendedDraft.json")).value()},
          profile{ProfileAvailability::available,
                  {1},
                  parseDigest(
                      corpus.at("identities").at("applicationContractSha256").get<std::string>())
                      .value(),
                  parseDigest(corpus.at("identities").at("dataContractSha256").get<std::string>())
                      .value(),
                  {{64}, {64}, {256}, {64}}},
          serverContext{context()}, clientContext{context()},
          map{{37},
              {9},
              parseDigest(corpus.at("identities").at("catalogSha256").get<std::string>()).value(),
              MapRequirements::create(fixtureVector(corpus.at("requirements"))).value()},
          server{NegotiationSession::createServer(serverConfig, profile, map, nonce(),
                                                  serverContext, digests, serverStorage)
                     .value()},
          client{NegotiationSession::createClient(clientConfig, profile, clientIdentity(),
                                                  clientContext, digests, clientStorage)
                     .value()} {}

    static auto context() -> SessionContext {
        return {{fixtureIdentity<ConnectionToken>("01000000000000000000000000000000"),
                 fixtureIdentity<PeerToken>("02000000000000000000000000000000"),
                 {1}},
                {100},
                ChannelState::pending};
    }
    auto nonce() const -> SessionNonce {
        return fixtureIdentity<SessionNonce>(
            corpus.at("identities").at("serverNonceHex").get<std::string>());
    }
    auto clientIdentity() const -> ClientNonce {
        return fixtureIdentity<ClientNonce>(
            corpus.at("identities").at("clientNonceHex").get<std::string>());
    }
    auto direction(std::size_t index) const -> Direction {
        return corpus.at("frames").at(index).at("direction") == "server" ? Direction::server
                                                                         : Direction::client;
    }
    auto golden(std::size_t index) const -> EncodedFrame {
        return encodeFrame(
                   decodeFrame(
                       decodeHex(corpus.at("frames").at(index).at("frameHex").get<std::string>())
                           .value())
                       .value())
            .value();
    }
    auto sender(std::size_t index) -> NegotiationSession & {
        return direction(index) == Direction::server ? server : client;
    }
    auto receiver(std::size_t index) -> NegotiationSession & {
        return direction(index) == Direction::server ? client : server;
    }
    auto senderContext(std::size_t index) -> SessionContext & {
        return direction(index) == Direction::server ? serverContext : clientContext;
    }
    auto receiverContext(std::size_t index) -> SessionContext & {
        return direction(index) == Direction::server ? clientContext : serverContext;
    }
    auto advance(TestSuite &suite) -> EncodedFrame {
        const auto index = progress;
        auto &out = sender(index);
        const auto prepared = requireOutbound(out, senderContext(index));
        static_cast<void>(suite.check(prepared == golden(index),
                                      "generated frame matches immutable reference bytes"));
        static_cast<void>(suite.check(out.prepareOutbound(senderContext(index)).value() == prepared,
                                      "preparing twice does not advance"));
        static_cast<void>(suite.check(
            out.commitOutbound(prepared, senderContext(index)).has_value(), "commit queued frame"));
        const auto accepted =
            receiver(index).receive(direction(index), prepared.bytes(), receiverContext(index));
        static_cast<void>(suite.check(accepted && *accepted == ReceiveDisposition::accepted,
                                      "peer accepts generated frame"));
        ++progress;
        return prepared;
    }
    void advanceTo(TestSuite &suite, std::size_t stop) {
        while (progress < stop) {
            static_cast<void>(advance(suite));
        }
    }
};
inline auto changedFrame(EncodedFrame input, auto mutate) -> EncodedFrame {
    auto frame = decodeFrame(input.bytes()).value();
    mutate(frame);
    return encodeFrame(frame).value();
}
inline void expectFailure(TestSuite &suite, const auto &result, RejectCode code,
                          std::string_view message) {
    static_cast<void>(suite.check(!result && result.error().code == code, message));
}
} // namespace extendedPrecache::tests
