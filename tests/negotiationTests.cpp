#include "negotiationFixtures.hpp"

namespace {
using namespace extendedPrecache::session;
using namespace extendedPrecache::tests;
constexpr std::size_t offerComplete = 11;
constexpr std::size_t selectionComplete = 23;
constexpr std::size_t readyComplete = 24;
constexpr std::size_t commitComplete = 25;
constexpr std::size_t exchangeComplete = 26;

void testExchange(TestSuite &suite, const std::filesystem::path &docs) {
    DuplexFixture f{docs};
    static_cast<void>(suite.check(f.server.state() == SessionState::awaitCapabilities &&
                                      f.client.state() == SessionState::awaitOffer,
                                  "initial states"));
    static_cast<void>(suite.check(!f.client.prepareOutbound(f.clientContext).value(),
                                  "client cannot send before offer"));
    while (f.progress < exchangeComplete) {
        static_cast<void>(suite.check(!f.server.releaseSignon(f.serverContext),
                                      "signon remains gated during exchange"));
        static_cast<void>(f.advance(suite));
        if (f.progress == offerComplete) {
            static_cast<void>(
                suite.check(f.client.state() == SessionState::awaitSelection,
                            "complete offer, not a partial page, changes client phase"));
        }
        if (f.progress == readyComplete) {
            static_cast<void>(suite.check(f.clientStorage.probe.active == 1 &&
                                              f.client.state() == SessionState::awaitCommit,
                                          "ready holds reservation without arming"));
        }
        if (f.progress == commitComplete) {
            static_cast<void>(suite.check(f.server.state() == SessionState::awaitCommitAck &&
                                              f.client.state() != SessionState::armed,
                                          "server commit alone does not activate either gate"));
        }
    }
    static_cast<void>(suite.check(f.server.state() == SessionState::committed &&
                                      f.client.state() == SessionState::armed,
                                  "both peers finish after ACK queue and receipt"));
    static_cast<void>(suite.check(f.server.transcriptBytes().value == normalTranscriptBytes &&
                                      f.client.transcriptBytes().value == normalTranscriptBytes,
                                  "unique transcript counts both directions"));
    static_cast<void>(suite.check(!f.server.releaseSignon(f.serverContext),
                                  "ACK alone cannot release a pending channel"));
    f.serverContext.channel = ChannelState::drained;
    const auto agreement = f.server.releaseSignon(f.serverContext).value();
    static_cast<void>(suite.check(
        agreement.selected.vector() == fixtureVector(f.corpus.at("selectedLimits")) &&
            agreement.selectionHash ==
                parseDigest(f.corpus.at("digests").at("selectionSha256").get<std::string>())
                    .value(),
        "exact selection and three externally fixed transcript hashes"));
    static_cast<void>(suite.check(agreement.nonce == f.nonce() &&
                                      agreement.clientNonce == f.clientIdentity() &&
                                      agreement.map.catalog == f.map.catalog,
                                  "agreement binds map and both identities"));
    static_cast<void>(suite.check(f.client.confirmServerInfo(48, {37}, f.clientContext).has_value(),
                                  "armed client validates first serverinfo"));
    static_cast<void>(
        suite.check(f.serverStorage.probe.calls == 1 && f.clientStorage.probe.calls == 1,
                    "preparation runs once per endpoint"));
    static_cast<void>(
        suite.check(f.serverStorage.probe.active == 1 && f.clientStorage.probe.active == 1,
                    "reservations survive commit"));
    f.server.close();
    f.client.close();
    static_cast<void>(
        suite.check(f.serverStorage.probe.active == 0 && f.clientStorage.probe.active == 0,
                    "disconnect releases both reservations"));
    static_cast<void>(suite.check(!f.server.releaseSignon(f.serverContext),
                                  "closed session cannot release signon"));
}

void testSequencingCorpus(TestSuite &suite, const std::filesystem::path &docs) {
    const auto corpus = readFixture(docs / "protocol/negotiationVectors.json");
    for (const auto &item : corpus.at("sequencingCases")) {
        const auto name = item.at("name").get<std::string>();
        DuplexFixture f{docs};
        if (name == "exactDuplicate") {
            const auto first = f.advance(suite);
            const auto bytes = f.client.transcriptBytes();
            f.clientContext.now.value += 100;
            const auto repeated =
                f.client.receive(Direction::server, first.bytes(), f.clientContext);
            static_cast<void>(suite.check(repeated && *repeated == ReceiveDisposition::duplicate &&
                                              f.client.transcriptBytes() == bytes,
                                          name));
            f.clientContext.now.value = 15100;
            expectFailure(suite, f.client.poll(f.clientContext), RejectCode::deadlineExceeded,
                          "duplicate never renews deadline");
        } else if (name == "alteredDuplicate") {
            const auto first = f.advance(suite);
            const auto changed = changedFrame(first, [](Frame &frame) {
                ++std::get<ServerOfferBody>(frame.payload).spawn.value;
            });
            expectFailure(suite,
                          f.client.receive(Direction::server, changed.bytes(), f.clientContext),
                          RejectCode::unexpectedSequence, name);
        } else if (name == "gap") {
            static_cast<void>(f.advance(suite));
            expectFailure(suite,
                          f.client.receive(Direction::server, f.golden(2).bytes(), f.clientContext),
                          RejectCode::unexpectedSequence, name);
        } else if (name == "staleSession") {
            static_cast<void>(f.advance(suite));
            const auto bytes = f.client.transcriptBytes();
            const auto old = changedFrame(
                f.golden(1), [](Frame &frame) { frame.nonce.bytes[0] ^= std::byte{1}; });
            const auto stale = f.client.receive(Direction::server, old.bytes(), f.clientContext);
            static_cast<void>(suite.check(stale && *stale == ReceiveDisposition::staleNonce &&
                                              f.client.transcriptBytes() == bytes,
                                          name));
            static_cast<void>(f.advance(suite));
        } else if (name == "prematureCommit") {
            static_cast<void>(f.advance(suite));
            const auto early = changedFrame(f.golden(commitComplete - 1),
                                            [](Frame &frame) { frame.sequence = {1}; });
            expectFailure(suite,
                          f.client.receive(Direction::server, early.bytes(), f.clientContext),
                          RejectCode::unexpectedMessage, name);
        } else if (name == "wrongSelectionEcho") {
            f.advanceTo(suite, selectionComplete);
            const auto wrong = changedFrame(f.golden(selectionComplete), [](Frame &frame) {
                std::get<DigestBody<MessageKind::clientReady>>(frame.payload).value.bytes[0] ^=
                    std::byte{1};
            });
            expectFailure(suite,
                          f.server.receive(Direction::client, wrong.bytes(), f.serverContext),
                          RejectCode::transcriptMismatch, name);
        } else if (name == "expired") {
            f.advanceTo(suite, commitComplete);
            f.serverContext.now.value = 15100;
            expectFailure(suite, f.server.poll(f.serverContext), RejectCode::deadlineExceeded,
                          name);
        } else if (name == "mapChanged") {
            f.advanceTo(suite, exchangeComplete);
            f.serverContext.binding.generation.value = 2;
            f.serverContext.channel = ChannelState::drained;
            expectFailure(suite, f.server.releaseSignon(f.serverContext), RejectCode::mapChanged,
                          name);
        } else if (name == "readyOnly") {
            f.advanceTo(suite, readyComplete);
            f.serverContext.channel = ChannelState::drained;
            static_cast<void>(suite.check(!f.server.releaseSignon(f.serverContext), name));
        } else if (name == "unavailableDataContract") {
            f.profile.availability = ProfileAvailability::unavailable;
            expectFailure(suite,
                          NegotiationSession::createClient(f.clientConfig, f.profile,
                                                           f.clientIdentity(), f.clientContext,
                                                           f.digests, f.clientStorage),
                          RejectCode::unsupportedContract, name);
            expectFailure(suite,
                          NegotiationSession::createServer(f.serverConfig, f.profile, f.map,
                                                           f.nonce(), f.serverContext, f.digests,
                                                           f.serverStorage),
                          RejectCode::unsupportedContract, name);
        } else if (name == "wrongOfferHash") {
            f.advanceTo(suite, offerComplete - 1);
            const auto wrong = changedFrame(f.golden(offerComplete - 1), [](Frame &frame) {
                std::get<DigestBody<MessageKind::offerEnd>>(frame.payload).value.bytes[0] ^=
                    std::byte{1};
            });
            expectFailure(suite,
                          f.client.receive(Direction::server, wrong.bytes(), f.clientContext),
                          RejectCode::transcriptMismatch, name);
        } else if (name == "noOfferDeadline") {
            f.clientContext.now.value = 15100;
            expectFailure(suite, f.client.poll(f.clientContext), RejectCode::deadlineExceeded,
                          name);
        } else {
            static_cast<void>(
                suite.check(false, "every sequencing fixture must have an executable scenario"));
        }
    }
}

void testPreparation(TestSuite &suite, const std::filesystem::path &docs) {
    for (const auto scenario :
         {"failure", "allocation", "null", "underquote", "overquote", "invalid"}) {
        DuplexFixture f{docs};
        f.advanceTo(suite, selectionComplete);
        auto &probe = f.clientStorage.probe;
        if (scenario == std::string_view{"failure"}) {
            probe.failure = Rejection{RejectCode::storageUnavailable};
        } else if (scenario == std::string_view{"allocation"}) {
            probe.allocationFailure = true;
        } else if (scenario == std::string_view{"null"}) {
            probe.nullLease = true;
        } else if (scenario == std::string_view{"underquote"}) {
            probe.quote = MetadataBytes{referenceMetadataBytes(f.clientConfig).value - 1};
        } else if (scenario == std::string_view{"overquote"}) {
            probe.quote = MetadataBytes{f.clientConfig.values().metadataBudget.value + 1};
        } else {
            probe.valid = false;
        }
        expectFailure(suite, f.client.prepareOutbound(f.clientContext),
                      scenario == std::string_view{"invalid"} ? RejectCode::mapChanged
                                                              : RejectCode::storageUnavailable,
                      scenario);
        static_cast<void>(
            suite.check(probe.active == 0 && f.client.state() == SessionState::rejected,
                        "failed preparation releases reservation and never emits Ready"));
    }
    {
        DuplexFixture f{docs};
        f.advanceTo(suite, selectionComplete);
        const auto ready = requireOutbound(f.client, f.clientContext);
        f.clientStorage.probe.valid = false;
        expectFailure(suite, f.client.commitOutbound(ready, f.clientContext),
                      RejectCode::mapChanged,
                      "revalidate reservation after queueing and before advancing");
        static_cast<void>(
            suite.check(f.clientStorage.probe.active == 0, "invalidation tears down reservation"));
    }
    {
        DuplexFixture f{docs};
        f.advanceTo(suite, readyComplete);
        f.serverStorage.probe.failure = Rejection{RejectCode::storageUnavailable};
        expectFailure(suite, f.server.prepareOutbound(f.serverContext),
                      RejectCode::storageUnavailable, "server prepares before Commit");
        static_cast<void>(suite.check(!f.server.releaseSignon(f.serverContext),
                                      "failed server cannot release signon"));
    }
    {
        DuplexFixture f{docs};
        f.advanceTo(suite, exchangeComplete);
        f.serverContext.channel = ChannelState::drained;
        f.serverStorage.probe.valid = false;
        expectFailure(suite, f.server.releaseSignon(f.serverContext), RejectCode::mapChanged,
                      "revalidate frozen snapshot immediately before release");
    }
    {
        DuplexFixture f{docs};
        f.advanceTo(suite, commitComplete);
        const auto ack = requireOutbound(f.client, f.clientContext);
        static_cast<void>(suite.check(f.client.state() == SessionState::awaitCommit &&
                                          !f.server.releaseSignon(f.serverContext),
                                      "preparing ACK does not arm decoder"));
        f.client.abort({RejectCode::transportUnavailable});
        expectFailure(suite, f.client.commitOutbound(ack, f.clientContext),
                      RejectCode::transportUnavailable, "failed ACK queue closes client");
    }
}

void testBindingsAndRejection(TestSuite &suite, const std::filesystem::path &docs) {
    {
        DuplexFixture f{docs};
        auto foreign = f.clientContext;
        foreign.binding.connection.bytes[0] ^= std::byte{1};
        const auto result = f.client.receive(Direction::server, f.golden(0).bytes(), foreign);
        static_cast<void>(suite.check(result && *result == ReceiveDisposition::foreignConnection &&
                                          f.client.transcriptBytes().value == 0,
                                      "foreign connection cannot bind an offer"));
        foreign = f.clientContext;
        foreign.binding.peer.bytes[0] ^= std::byte{1};
        static_cast<void>(
            suite.check(f.client.receive(Direction::server, f.golden(0).bytes(), foreign) ==
                            ReceiveDisposition::foreignConnection,
                        "foreign peer cannot advance session"));
        expectFailure(suite,
                      f.client.receive(Direction::client, f.golden(0).bytes(), f.clientContext),
                      RejectCode::unexpectedMessage, "wrong direction rejected");
    }
    {
        DuplexFixture f{docs};
        static_cast<void>(f.advance(suite));
        f.clientContext.now.value = 99;
        expectFailure(suite, f.client.poll(f.clientContext), RejectCode::transportUnavailable,
                      "monotonic clock regression fails closed");
    }
    {
        DuplexFixture f{docs};
        static_cast<void>(f.advance(suite));
        f.client.abort({RejectCode::storageUnavailable});
        const auto reject = requireOutbound(f.client, f.clientContext);
        static_cast<void>(suite.check(
            std::get<RejectBody>(decodeFrame(reject.bytes()).value().payload).value.code ==
                RejectCode::storageUnavailable,
            "preserve first local failure"));
        f.client.abort({RejectCode::deadlineExceeded});
        const auto prepared = requireOutbound(f.client, f.clientContext);
        static_cast<void>(
            suite.check(prepared == reject, "later failures cannot replace rejection reason"));
        static_cast<void>(suite.check(
            f.client.commitOutbound(prepared, f.clientContext).has_value(), "queue one rejection"));
        static_cast<void>(suite.check(!f.client.prepareOutbound(f.clientContext).value(),
                                      "at most one terminal rejection"));
        expectFailure(suite, f.server.receive(Direction::client, prepared.bytes(), f.serverContext),
                      RejectCode::storageUnavailable, "peer rejection is terminal");
        static_cast<void>(suite.check(!f.server.prepareOutbound(f.serverContext).value(),
                                      "never reply to rejection with rejection"));
    }
    {
        DuplexFixture f{docs};
        const auto pending = requireOutbound(f.server, f.serverContext);
        expectFailure(suite, f.server.commitOutbound(f.golden(1), f.serverContext),
                      RejectCode::transportUnavailable,
                      "different queue ticket cannot commit prepared offer");
        static_cast<void>(
            suite.check(pending.bytes().size() != 0 && f.server.transcriptBytes().value == 0,
                        "uncommitted offer is not transcript progress"));
    }
    {
        DuplexFixture f{docs};
        f.advanceTo(suite, exchangeComplete);
        expectFailure(suite, f.client.confirmServerInfo(47, {37}, f.clientContext),
                      RejectCode::unsupportedContract,
                      "protocol mismatch after ACK invalidates agreement");
    }
    {
        DuplexFixture f{docs};
        f.advanceTo(suite, exchangeComplete);
        expectFailure(suite, f.client.confirmServerInfo(48, {38}, f.clientContext),
                      RejectCode::unsupportedContract, "spawn mismatch invalidates agreement");
    }
}

void testPeerValidation(TestSuite &suite, const std::filesystem::path &docs) {
    for (const auto kind : {MessageKind::applicationContract, MessageKind::dataContract}) {
        DuplexFixture f{docs};
        const auto index = kind == MessageKind::applicationContract ? 1U : 2U;
        f.advanceTo(suite, index);
        const auto wrong = changedFrame(f.golden(index), [kind](Frame &frame) {
            if (kind == MessageKind::applicationContract) {
                std::get<DigestBody<MessageKind::applicationContract>>(frame.payload)
                    .value.bytes[0] ^= std::byte{1};
            } else {
                std::get<DigestBody<MessageKind::dataContract>>(frame.payload).value.bytes[0] ^=
                    std::byte{1};
            }
        });
        expectFailure(suite, f.client.receive(Direction::server, wrong.bytes(), f.clientContext),
                      RejectCode::unsupportedContract,
                      "peer identities must match approved contracts");
    }
    {
        DuplexFixture f{docs};
        f.advanceTo(suite, 19);
        const auto low = changedFrame(f.golden(19), [](Frame &frame) {
            --std::get<LimitPageBody<MessageKind::selectionPage>>(frame.payload).wireValues[0];
        });
        static_cast<void>(suite.check(
            f.client.receive(Direction::server, low.bytes(), f.clientContext).has_value(),
            "selection is checked after its complete vector"));
        static_cast<void>(suite.check(
            f.client.receive(Direction::server, f.golden(20).bytes(), f.clientContext).has_value(),
            "second selection page"));
        expectFailure(
            suite, f.client.receive(Direction::server, f.golden(21).bytes(), f.clientContext),
            RejectCode::invalidConfiguration, "server cannot select below the exact minimum");
    }
    {
        DuplexFixture f{docs};
        auto offer = decodeFrame(f.golden(0).bytes()).value();
        std::get<ServerOfferBody>(offer.payload).timeout = {1000};
        f.clientContext.now.value = 1100;
        expectFailure(suite,
                      f.client.receive(Direction::server, encodeFrame(offer).value().bytes(),
                                       f.clientContext),
                      RejectCode::deadlineExceeded,
                      "smaller peer deadline applies to original start");
    }
    {
        DuplexFixture f{docs};
        f.advanceTo(suite, 15);
        const auto low = changedFrame(f.golden(15), [](Frame &frame) {
            std::get<LimitPageBody<MessageKind::clientLimitsPage>>(frame.payload).wireValues[0] =
                599;
        });
        static_cast<void>(suite.check(
            f.server.receive(Direction::client, low.bytes(), f.serverContext).has_value(),
            "first peer capacity page"));
        static_cast<void>(suite.check(
            f.server.receive(Direction::client, f.golden(16).bytes(), f.serverContext).has_value(),
            "second peer capacity page"));
        const auto result =
            f.server.receive(Direction::client, f.golden(17).bytes(), f.serverContext);
        expectFailure(suite, result, RejectCode::capacityInsufficient,
                      "full map refused without resource dropping");
        static_cast<void>(suite.check(!result && result.error().fieldId == 1 &&
                                          result.error().offered == 599 &&
                                          result.error().required == 600,
                                      "capacity error identifies exact field and values"));
    }
}

void testBoundedFailures(TestSuite &suite, const std::filesystem::path &docs) {
    {
        DuplexFixture f{docs};
        f.advanceTo(suite, exchangeComplete);
        f.serverContext.now.value = 15100;
        expectFailure(suite, f.server.poll(f.serverContext), RejectCode::deadlineExceeded,
                      "deadline also bounds pending drain after ACK");
        static_cast<void>(suite.check(f.serverStorage.probe.active == 0,
                                      "drain timeout releases prepared storage"));
    }
    {
        DuplexFixture f{docs};
        f.advanceTo(suite, exchangeComplete);
        f.serverContext.channel = ChannelState::drained;
        static_cast<void>(suite.check(f.server.releaseSignon(f.serverContext).has_value(),
                                      "handoff within deadline"));
        f.serverContext.now.value = 100000;
        static_cast<void>(
            suite.check(f.server.poll(f.serverContext).has_value(),
                        "handshake deadline does not time out asset loading after handoff"));
    }
    {
        DuplexFixture f{docs};
        auto server = f.serverConfig.values();
        auto client = f.clientConfig.values();
        server.negotiation.maxBytes = {normalTranscriptBytes};
        client.negotiation.maxBytes = {normalTranscriptBytes};
        f.serverConfig = Configuration::create(server).value();
        f.clientConfig = Configuration::create(client).value();
        f.server = NegotiationSession::createServer(f.serverConfig, f.profile, f.map, f.nonce(),
                                                    f.serverContext, f.digests, f.serverStorage)
                       .value();
        f.client = NegotiationSession::createClient(f.clientConfig, f.profile, f.clientIdentity(),
                                                    f.clientContext, f.digests, f.clientStorage)
                       .value();
        f.advanceTo(suite, exchangeComplete);
        const auto extra =
            encodeFrame({f.nonce(), {10}, RejectBody{{RejectCode::storageUnavailable}}}).value();
        expectFailure(suite, f.server.receive(Direction::client, extra.bytes(), f.serverContext),
                      RejectCode::budgetExceeded,
                      "unique bytes accounted before accepting a terminal peer frame");
        static_cast<void>(
            suite.check(!f.server.prepareOutbound(f.serverContext).value() &&
                            f.server.transcriptBytes().value == normalTranscriptBytes,
                        "exhausted budget cannot emit a rejection or retain excess bytes"));
    }
    {
        DuplexFixture f{docs};
        f.advanceTo(suite, 4);
        const auto reordered = changedFrame(f.golden(4), [](Frame &frame) {
            std::get<LimitPageBody<MessageKind::requirementsPage>>(frame.payload).ordinal = {1};
        });
        expectFailure(suite,
                      f.client.receive(Direction::server, reordered.bytes(), f.clientContext),
                      RejectCode::unexpectedMessage,
                      "valid page shape in wrong ordinal is still a protocol failure");
    }
    {
        DuplexFixture f{docs};
        auto config = f.clientConfig.values();
        config.metadataBudget = referenceMetadataBytes(f.clientConfig);
        const auto base = Configuration::create(config).value();
        expectFailure(suite,
                      NegotiationSession::createClient(base, f.profile, f.clientIdentity(),
                                                       f.clientContext, f.digests, f.clientStorage),
                      RejectCode::storageUnavailable,
                      "known control buffers must fit beyond the published base estimate");
    }
    {
        class UnavailableDigest final : public DigestProvider {
            auto sha256(std::span<const std::byte>) const noexcept
                -> std::expected<Digest, CryptoError> override {
                return std::unexpected{CryptoError::providerFailure};
            }
        } provider;
        DuplexFixture f{docs};
        f.server = NegotiationSession::createServer(f.serverConfig, f.profile, f.map, f.nonce(),
                                                    f.serverContext, provider, f.serverStorage)
                       .value();
        f.advanceTo(suite, offerComplete - 1);
        expectFailure(suite, f.server.prepareOutbound(f.serverContext),
                      RejectCode::transportUnavailable,
                      "cryptographic provider failure cannot produce a transcript digest");
    }
    {
        DuplexFixture f{docs};
        const auto foreignId = changedFrame(f.golden(0), [](Frame &frame) {
            std::get<ServerOfferBody>(frame.payload).dataContract = {2};
        });
        expectFailure(
            suite, f.client.receive(Direction::server, foreignId.bytes(), f.clientContext),
            RejectCode::unsupportedContract, "data contract integer must match as well as digest");
    }
}

void testLegacy(TestSuite &suite, const std::filesystem::path &docs) {
    const auto config = defaultConfiguration(EndpointRole::client).value();
    auto values = legacyLimits().vector().wireValues();
    values.fill(0);
    values[0] = 1;
    values[5] = 1;
    const auto map = MapRequirements::create(CapabilityVector{values}).value();
    static_cast<void>(
        suite.check(checkLegacyAdmission(config, map, LegacyProof::complete, false).has_value(),
                    "explicit complete legacy proof before offer"));
    expectFailure(suite, checkLegacyAdmission(config, map, LegacyProof::unverified, false),
                  RejectCode::legacyIncompatible, "numeric fit alone is not legacy compatibility");
    expectFailure(suite, checkLegacyAdmission(config, map, LegacyProof::complete, true),
                  RejectCode::legacyIncompatible, "no downgrade after offer");
    const auto required =
        parseConfiguration(readText(docs / "examples/serverExtendedDraft.json")).value();
    expectFailure(suite, checkLegacyAdmission(required, map, LegacyProof::complete, false),
                  RejectCode::legacyIncompatible, "required extension never accepts legacy");
    values[0] = 512;
    values[5] = 512;
    expectFailure(suite,
                  checkLegacyAdmission(config,
                                       MapRequirements::create(CapabilityVector{values}).value(),
                                       LegacyProof::complete, false),
                  RejectCode::legacyIncompatible, "legacy complete map must fit native indices");
}
} // namespace

auto main(int argc, char **argv) -> int {
    if (argc != 2) {
        return 2;
    }
    return runTests([&] {
        TestSuite suite;
        testExchange(suite, argv[1]);
        testSequencingCorpus(suite, argv[1]);
        testPreparation(suite, argv[1]);
        testBindingsAndRejection(suite, argv[1]);
        testPeerValidation(suite, argv[1]);
        testBoundedFailures(suite, argv[1]);
        testLegacy(suite, argv[1]);
        return suite.finish();
    });
}
