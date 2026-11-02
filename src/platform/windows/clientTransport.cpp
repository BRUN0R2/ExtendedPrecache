#include "clientTransport.hpp"

#include "clientModelTables.hpp"

#include <extendedPrecache/platform/windows/cngDigest.hpp>
#include <extendedPrecache/transport/clientBarrier.hpp>
#include <extendedPrecache/transport/clientNegotiator.hpp>

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

namespace extendedPrecache::platform::windows::client {
namespace {
using session::ChannelState;
using session::ConnectionBinding;
using session::MonotonicTime;
using session::NegotiationSession;
using session::RejectCode;
using session::Rejection;
using transport::ClientNegotiator;

// The engine takes terminated strings, which a view does not promise.
constexpr auto controlMessageText = "ExtPrecache";
constexpr auto optInName = "epc";
constexpr auto optInValue = "1";
constexpr auto optInCommand = "epc 1\n";
constexpr auto disconnectCommand = "disconnect\n";
constexpr auto logPrefix = "[EPC] ";
constexpr auto lineEnd = "\n";
constexpr auto secureServerRefusal = "ExtendedPrecache: refusing a server secured by VAC";
constexpr auto optInFailure = "ExtendedPrecache: the opt-in does not fit in the userinfo";
static_assert(session::controlMessageName == std::string_view{controlMessageText});
static_assert(transport::clientOptInKey == std::string_view{optInName});

// Writes the variable into the userinfo. The engine saves neither it, since it is not archived, nor
// the key, since a variable has that name: no later session of the game inherits the opt-in.
constexpr std::int32_t userInfoVariable = 2;

constexpr std::size_t registrationBytes = 2 + transport::registrationNameBytes;
constexpr std::size_t serverInfoHeadBytes = 8;
constexpr std::size_t maxCommandBytes = 127;

auto monotonicNow() noexcept -> MonotonicTime {
    return {GetTickCount64()};
}

template <typename Identity> auto randomIdentity() noexcept -> std::optional<Identity> {
    const auto nonce = generateSessionNonce();
    if (!nonce) {
        return std::nullopt;
    }
    auto identity = Identity{};
    std::ranges::copy(nonce->bytes, identity.bytes.begin());
    return identity;
}

auto isControlRegistration(std::span<const std::byte> bytes) noexcept -> bool {
    if (bytes.size() < registrationBytes) {
        return false;
    }
    const auto name = bytes.subspan(2, transport::registrationNameBytes);
    const auto expected = std::as_bytes(std::span{session::controlMessageName});
    const auto [nameRest, expectedRest] = std::ranges::mismatch(name, expected);
    return expectedRest == expected.end() && nameRest != name.end() && *nameRest == std::byte{};
}

auto readUnsigned(std::span<const std::byte> bytes) noexcept -> std::uint32_t {
    auto value = std::uint32_t{};
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

class ClientTransport final : public transport::SessionSource,
                              public transport::ReliableChannel,
                              public session::SessionPreparer {
  public:
    ClientTransport(ClientEngine engine, std::unique_ptr<ClientProfile> profile,
                    ConnectionBinding binding) noexcept
        : engine{engine}, profile{std::move(profile)},
          negotiator{this->profile->policy(), binding, *this, *this}, models{engine.image()} {}

    [[nodiscard]] auto open(session::SessionContext context)
        -> std::expected<NegotiationSession, Rejection> override {
        return profile->open(context, *this);
    }
    [[nodiscard]] auto prepare(const session::Configuration &configuration,
                               const session::SessionAgreement &agreement)
        -> std::expected<std::unique_ptr<session::PreparedSession>, Rejection> override {
        if (const auto reserved = models.reserve(requiredModelEntries(agreement)); !reserved) {
            return std::unexpected{reserved.error()};
        }
        return leaseModelTables(configuration, agreement, models.reservedBytes());
    }
    [[nodiscard]] auto state() const noexcept -> ChannelState override {
        return engine.status().connected ? ChannelState::pending : ChannelState::unavailable;
    }
    [[nodiscard]] auto queue(std::span<const std::byte> message) noexcept -> bool override;

    [[nodiscard]] auto hookEngine() noexcept -> bool;
    [[nodiscard]] auto onRegisterVariable(VariableDeclaration variable) noexcept -> void *;
    void onRegistration() noexcept;
    void onServerInfo() noexcept;
    void onControlMessage(std::int32_t size, const void *data) noexcept;
    void onFrame(double time) noexcept;

  private:
    void refuse(Rejection reason) const noexcept;
    void say(const char *text) const noexcept;
    void startPumping() noexcept;
    void reportOptIn() noexcept;
    // The engine's code goes back to the original before anything else can happen on this client.
    void connectionLost() noexcept;
    // True while the connection belongs to a secure server, which this client must not stay on.
    [[nodiscard]] auto leavingSecureServer() noexcept -> bool;

    // Runs one step of the negotiation; a refusal or exhausted memory closes the connection.
    template <typename Step>
    [[nodiscard]] auto settle(Step step) const noexcept -> std::invoke_result_t<Step> {
        auto result = [&]() -> std::invoke_result_t<Step> {
            try {
                return step();
            } catch (const std::bad_alloc &) {
                return std::unexpected{Rejection{RejectCode::storageUnavailable}};
            }
        }();
        if (!result) {
            refuse(result.error());
        }
        return result;
    }

    ClientEngine engine;
    std::unique_ptr<ClientProfile> profile;
    ClientNegotiator negotiator;
    ClientModelTables models;
    ServiceParser nativeRegistration{};
    ServiceParser nativeServerInfo{};
    VariableRegistration nativeVariable{};
    FrameFunction nativeFrame{};
    double lastConnectionTime{};
    bool messageHooked{};
    bool optInRequested{};
    bool optInReported{};
    bool secureRefused{};
};

// The engine's tables carry no context, so their contract forces this single module-wide owner.
constinit std::unique_ptr<ClientTransport> activeTransport;

void frameEntry(double time) {
    activeTransport->onFrame(time);
}

auto ClientTransport::queue(std::span<const std::byte> message) noexcept -> bool {
    // A control command is bounded well below this, so there is no partial case to answer.
    auto text = std::array<char, maxCommandBytes + 1>{};
    const auto size = std::min(message.size(), maxCommandBytes);
    std::memcpy(text.data(), message.data(), size);
    engine.serverCommand(text.data());
    return true;
}

void ClientTransport::say(const char *text) const noexcept {
    engine.consolePrint(logPrefix);
    engine.consolePrint(text);
    engine.consolePrint(lineEnd);
}

void ClientTransport::refuse(Rejection reason) const noexcept {
    say(transport::refusalReason(reason.code));
    engine.clientCommand(disconnectCommand);
}

void ClientTransport::startPumping() noexcept {
    if (nativeFrame == nullptr) {
        nativeFrame = engine.replaceFrame(frameEntry);
    }
}

auto ClientTransport::onRegisterVariable(VariableDeclaration variable) noexcept -> void * {
    // The client library's first variable: the engine's variables and commands work, and no
    // connection can have started, because the connect request reads the userinfo later.
    if (!optInRequested) {
        optInRequested = true;
        nativeVariable({.name = optInName, .value = optInValue, .flags = userInfoVariable});
        engine.clientCommand(optInCommand);
        startPumping();
    }
    return nativeVariable(variable);
}

void ClientTransport::reportOptIn() noexcept {
    if (optInReported) {
        return;
    }
    optInReported = true;
    if (std::string_view{engine.localInfo(optInName)} != optInValue) {
        say(optInFailure);
    }
}

auto ClientTransport::leavingSecureServer() noexcept -> bool {
    if (!engine.serverIsSecure()) {
        return false;
    }
    if (!secureRefused) {
        secureRefused = true;
        say(secureServerRefusal);
        engine.clientCommand(disconnectCommand);
        connectionLost();
    }
    return true;
}

void ClientTransport::connectionLost() noexcept {
    negotiator.connectionLost();
    models.leave();
}

void ClientTransport::onRegistration() noexcept {
    startPumping();
    if (leavingSecureServer()) {
        engine.skipMessage();
        return;
    }
    const auto bytes = engine.unreadMessage();
    if (isControlRegistration(bytes)) {
        if (!messageHooked) {
            engine.hookUserMessage(controlMessageText,
                                   [](const char *, std::int32_t size, void *data) -> std::int32_t {
                                       activeTransport->onControlMessage(size, data);
                                       return 1;
                                   });
            messageHooked = true;
        }
        const auto registration =
            transport::ObservedRegistration{.id = std::to_integer<std::int32_t>(bytes[0]),
                                            .size = std::to_integer<std::int32_t>(bytes[1])};
        const auto observed =
            settle([&] { return negotiator.observeRegistration(registration, monotonicNow()); });
        if (!observed) {
            engine.skipMessage();
            return;
        }
    }
    nativeRegistration();
}

void ClientTransport::onServerInfo() noexcept {
    startPumping();
    // The parser overwrites the flag with what the serverinfo says, so this is the last look at it.
    if (leavingSecureServer()) {
        engine.skipMessage();
        return;
    }
    const auto bytes = engine.unreadMessage();
    if (bytes.size() < serverInfoHeadBytes) {
        nativeServerInfo();
        return;
    }
    const auto protocol = session::ProtocolVersion{readUnsigned(bytes)};
    const auto spawn = session::SpawnCount{readUnsigned(bytes.subspan(sizeof(std::uint32_t)))};
    const auto admitted =
        settle([&] { return negotiator.admitServerInfo(protocol, spawn, monotonicNow()); });
    if (!admitted) {
        // The parser resets the client state before anything else, so it must not start.
        engine.skipMessage();
        return;
    }
    if (!models.enterMap()) {
        refuse({RejectCode::storageUnavailable});
        engine.skipMessage();
        return;
    }
    nativeServerInfo();
}

void ClientTransport::onControlMessage(std::int32_t size, const void *data) noexcept {
    const auto payload = std::span{static_cast<const std::byte *>(data),
                                   static_cast<std::size_t>(std::max(size, 0))};
    std::ignore = settle([&] { return negotiator.receiveFrame(payload, monotonicNow()); });
}

void ClientTransport::onFrame(double time) noexcept {
    nativeFrame(time);
    reportOptIn();
    const auto current = engine.status();
    if (!current.connected) {
        secureRefused = false;
    } else if (leavingSecureServer()) {
        return;
    }
    if (!current.connected || current.connectionTime < lastConnectionTime) {
        connectionLost();
    }
    lastConnectionTime = current.connected ? current.connectionTime : 0.0;
    std::ignore = settle([&] { return negotiator.pump(monotonicNow()); });
}

auto ClientTransport::hookEngine() noexcept -> bool {
    if (!engine.holdsVerifiedTables()) {
        return false;
    }
    nativeRegistration = engine.replaceService(Service::registerUserMessage,
                                               [] { activeTransport->onRegistration(); });
    nativeServerInfo =
        engine.replaceService(Service::serverInfo, [] { activeTransport->onServerInfo(); });
    nativeVariable = engine.replaceVariableRegistration([](VariableDeclaration variable) -> void * {
        return activeTransport->onRegisterVariable(variable);
    });
    return true;
}
} // namespace

auto installClientTransport(ClientEngine engine, std::unique_ptr<ClientProfile> profile)
    -> std::expected<void, TransportRefusal> {
    const auto connection = randomIdentity<session::ConnectionToken>();
    const auto peer = randomIdentity<session::PeerToken>();
    if (!connection || !peer) {
        return std::unexpected{TransportRefusal::noBinding};
    }
    activeTransport = std::make_unique<ClientTransport>(engine, std::move(profile),
                                                        ConnectionBinding{*connection, *peer, {1}});
    if (!activeTransport->hookEngine()) {
        activeTransport.reset();
        return std::unexpected{TransportRefusal::unexpectedTables};
    }
    return {};
}

} // namespace extendedPrecache::platform::windows::client
