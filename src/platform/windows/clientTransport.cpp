#include "clientTransport.hpp"

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

// The engine takes a terminated string, which a view does not promise.
constexpr auto controlMessageText = "ExtPrecache";
constexpr auto disconnectCommand = "disconnect\n";
constexpr auto logPrefix = "[EPC] ";
constexpr auto lineEnd = "\n";
static_assert(session::controlMessageName == std::string_view{controlMessageText});

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

class ClientTransport final : public transport::SessionSource, public transport::ReliableChannel {
  public:
    ClientTransport(ClientEngine engine, std::unique_ptr<ClientProfile> profile,
                    ConnectionBinding binding) noexcept
        : engine{engine}, profile{std::move(profile)},
          negotiator{this->profile->policy(), binding, *this, *this} {}

    [[nodiscard]] auto open(session::SessionContext context)
        -> std::expected<NegotiationSession, Rejection> override {
        return profile->open(context);
    }
    [[nodiscard]] auto state() const noexcept -> ChannelState override {
        return engine.status().connected ? ChannelState::pending : ChannelState::unavailable;
    }
    [[nodiscard]] auto queue(std::span<const std::byte> message) noexcept -> bool override;

    [[nodiscard]] auto hookServices(ServiceParser registration, ServiceParser serverInfo) noexcept
        -> bool;
    void onRegistration(FrameFunction frame) noexcept;
    void onServerInfo(FrameFunction frame) noexcept;
    void onControlMessage(std::int32_t size, const void *data) noexcept;
    void onFrame(double time) noexcept;

  private:
    void refuse(Rejection reason) const noexcept;
    void startPumping(FrameFunction frame) noexcept;

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
    ServiceParser nativeRegistration{};
    ServiceParser nativeServerInfo{};
    FrameFunction nativeFrame{};
    double lastConnectionTime{};
    bool messageHooked{};
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

void ClientTransport::refuse(Rejection reason) const noexcept {
    engine.consolePrint(logPrefix);
    engine.consolePrint(transport::refusalReason(reason.code));
    engine.consolePrint(lineEnd);
    engine.clientCommand(disconnectCommand);
}

void ClientTransport::startPumping(FrameFunction frame) noexcept {
    if (nativeFrame == nullptr) {
        nativeFrame = engine.replaceFrame(frame);
    }
}

void ClientTransport::onRegistration(FrameFunction frame) noexcept {
    startPumping(frame);
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

void ClientTransport::onServerInfo(FrameFunction frame) noexcept {
    startPumping(frame);
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
    nativeServerInfo();
}

void ClientTransport::onControlMessage(std::int32_t size, const void *data) noexcept {
    const auto payload = std::span{static_cast<const std::byte *>(data),
                                   static_cast<std::size_t>(std::max(size, 0))};
    std::ignore = settle([&] { return negotiator.receiveFrame(payload, monotonicNow()); });
}

void ClientTransport::onFrame(double time) noexcept {
    nativeFrame(time);
    const auto current = engine.status();
    if (!current.connected || current.connectionTime < lastConnectionTime) {
        negotiator.connectionLost();
    }
    lastConnectionTime = current.connected ? current.connectionTime : 0.0;
    std::ignore = settle([&] { return negotiator.pump(monotonicNow()); });
}

auto ClientTransport::hookServices(ServiceParser registration, ServiceParser serverInfo) noexcept
    -> bool {
    nativeRegistration = engine.replaceService(Service::registerUserMessage, registration);
    if (nativeRegistration == nullptr) {
        return false;
    }
    nativeServerInfo = engine.replaceService(Service::serverInfo, serverInfo);
    if (nativeServerInfo == nullptr) {
        // Nothing is left half installed: the first replacement goes back.
        std::ignore = engine.replaceService(Service::registerUserMessage, nativeRegistration);
        return false;
    }
    return true;
}
} // namespace

auto installClientTransport(ClientEngine engine, std::unique_ptr<ClientProfile> profile)
    -> std::expected<void, TransportRefusal> {
    // The engine's tables already point at the first one, which must outlive any later call.
    if (activeTransport) {
        return {};
    }
    const auto connection = randomIdentity<session::ConnectionToken>();
    const auto peer = randomIdentity<session::PeerToken>();
    if (!connection || !peer) {
        return std::unexpected{TransportRefusal::noBinding};
    }
    activeTransport = std::make_unique<ClientTransport>(engine, std::move(profile),
                                                        ConnectionBinding{*connection, *peer, {1}});
    const auto hooked =
        activeTransport->hookServices([] { activeTransport->onRegistration(frameEntry); },
                                      [] { activeTransport->onServerInfo(frameEntry); });
    if (!hooked) {
        activeTransport.reset();
        return std::unexpected{TransportRefusal::unexpectedTables};
    }
    return {};
}

} // namespace extendedPrecache::platform::windows::client
