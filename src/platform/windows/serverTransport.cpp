#include "serverTransport.hpp"

#include "modelRelocation.hpp"
#include "serverModelTables.hpp"

#include <extendedPrecache/platform/windows/cngDigest.hpp>
#include <extendedPrecache/transport/clientBarrier.hpp>

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

namespace extendedPrecache::platform::windows {
namespace {
using session::ChannelState;
using session::ConnectionBinding;
using session::MonotonicTime;
using session::NegotiationSession;
using session::RejectCode;
using session::Rejection;
using transport::ClientBarrier;
using transport::ClientOptIn;
using transport::CommandVerdict;
using transport::ControlMessageId;
using transport::ReleaseDecision;

constexpr std::size_t maxClientSlots = 32;
constexpr std::int32_t fewestClientsForNegotiation = 2;
constexpr auto connectionCommandName = std::string_view{"new"};

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

class EngineChannel final : public transport::ReliableChannel {
  public:
    EngineChannel(rehlds::Api api, rehlds::GameClient client) noexcept : api{api}, client{client} {}

    [[nodiscard]] auto state() const noexcept -> ChannelState override {
        if (!client.isConnected()) {
            return ChannelState::unavailable;
        }
        return rehlds::channelDrained(*client.messageBuffer()) ? ChannelState::drained
                                                               : ChannelState::pending;
    }
    [[nodiscard]] auto queue(std::span<const std::byte> message) noexcept -> bool override {
        auto &buffer = *client.messageBuffer();
        const auto size = static_cast<std::int32_t>(message.size());
        if (buffer.capacity - buffer.used < size) {
            return false;
        }
        api.writeBuffer(buffer, size, message.data());
        return true;
    }

  private:
    rehlds::Api api;
    rehlds::GameClient client;
};

struct ClientSlot final {
    ClientSlot(rehlds::Api api, rehlds::GameClient client, ClientOptIn optIn,
               ConnectionBinding binding, ControlMessageId messageId,
               transport::SessionSource &sessions,
               const transport::LegacyAdmission &legacy) noexcept
        : client{client}, channel{api, client},
          barrier{optIn, binding, messageId, sessions, legacy, channel} {}
    ClientSlot(const ClientSlot &) = delete;
    auto operator=(const ClientSlot &) -> ClientSlot & = delete;

    rehlds::GameClient client;
    EngineChannel channel;
    ClientBarrier barrier;
};

class ServerTransport final : public transport::SessionSource,
                              public transport::LegacyAdmission,
                              public session::SessionPreparer {
  public:
    ServerTransport(rehlds::Api api, std::unique_ptr<TransportProfile> profile,
                    std::unique_ptr<ServerModelTables> models, TransportHost host,
                    metamod::RetainedUse use) noexcept
        : api{api}, profile{std::move(profile)}, models{std::move(models)}, host{host},
          use{std::move(use)} {}

    [[nodiscard]] auto open(session::SessionContext context)
        -> std::expected<NegotiationSession, Rejection> override {
        return profile->open(context, currentMap(), *this);
    }
    [[nodiscard]] auto admitLegacy() const -> std::expected<void, Rejection> override {
        return profile->admitLegacy(currentMap());
    }
    [[nodiscard]] auto prepare(const session::Configuration &configuration,
                               const session::SessionAgreement &agreement)
        -> std::expected<std::unique_ptr<session::PreparedSession>, Rejection> override {
        return leaseModelTables(configuration, agreement, models->movedBytes());
    }

    void onNetCommand(void *chain, rehlds::GameClient client, std::uint8_t opcode) noexcept;
    void onStringCommand(void *chain, const char *name, std::int32_t source, void *client) noexcept;
    void onClientConnected(void *chain, rehlds::GameClient client) noexcept;
    void onDropClient(void *chain, rehlds::GameClient client, bool crash,
                      const char *reason) noexcept;
    void onActivateServer(void *chain, std::int32_t runPhysics) noexcept;
    void onFrame(void *chain) noexcept;

  private:
    [[nodiscard]] auto currentMap() const noexcept -> MapFacts {
        return {{static_cast<std::uint32_t>(api.spawnCount())}, models->entriesInUse()};
    }
    [[nodiscard]] auto slotOf(rehlds::GameClient client) noexcept -> std::optional<ClientSlot> *;
    [[nodiscard]] auto liveSlot(rehlds::GameClient client) noexcept -> ClientSlot *;
    [[nodiscard]] auto peekCommandText() const noexcept -> std::optional<std::string_view>;
    void openSlot(rehlds::GameClient client) noexcept;
    void refreshAvailability() noexcept;
    void refuse(rehlds::GameClient client, Rejection reason) const noexcept;
    void releaseIfReady(ClientSlot &slot) noexcept;
    void continueSignon() const noexcept;

    // Runs one step of a barrier; a refusal or exhausted memory closes that connection. The slot
    // may be gone when this returns, because closing a connection reaches the drop hook.
    template <typename Step>
    [[nodiscard]] auto settle(rehlds::GameClient client, Step step) const noexcept
        -> std::invoke_result_t<Step> {
        auto result = [&]() -> std::invoke_result_t<Step> {
            try {
                return step();
            } catch (const std::bad_alloc &) {
                return std::unexpected{Rejection{RejectCode::storageUnavailable}};
            }
        }();
        if (!result) {
            refuse(client, result.error());
        }
        return result;
    }

    rehlds::Api api;
    std::unique_ptr<TransportProfile> profile;
    std::unique_ptr<ServerModelTables> models;
    TransportHost host;
    metamod::RetainedUse use;
    std::optional<ControlMessageId> messageId;
    rehlds::CommandFunction connectionCommand{};
    session::LocalGeneration generation{};
    bool enabled{};
    std::array<std::optional<ClientSlot>, maxClientSlots> slots;
};

// The engine's hooks carry no context, so their contract forces this single module-wide owner.
constinit std::unique_ptr<ServerTransport> activeTransport;

auto ServerTransport::slotOf(rehlds::GameClient client) noexcept -> std::optional<ClientSlot> * {
    const auto index = client.id();
    if (index < 0 || std::cmp_greater_equal(index, slots.size())) {
        return nullptr;
    }
    return &slots[static_cast<std::size_t>(index)];
}

auto ServerTransport::liveSlot(rehlds::GameClient client) noexcept -> ClientSlot * {
    auto *const slot = slotOf(client);
    if (slot == nullptr || !slot->has_value()) {
        return nullptr;
    }
    return std::addressof(**slot);
}

void ServerTransport::refuse(rehlds::GameClient client, Rejection reason) const noexcept {
    // The engine shows this text only to the client, and the operator needs it as well.
    const auto *const text = transport::refusalReason(reason.code);
    host.report(text);
    api.dropClient(client, text);
}

// The text under the read cursor, left unread; empty when the engine would find no terminator.
auto ServerTransport::peekCommandText() const noexcept -> std::optional<std::string_view> {
    const auto &message = *api.netMessage();
    const auto cursor = *api.readCount();
    if (cursor < 0 || cursor >= message.used) {
        return std::nullopt;
    }
    const auto *const text = reinterpret_cast<const char *>(message.data + cursor);
    const auto available = static_cast<std::size_t>(message.used - cursor);
    const auto *const end = static_cast<const char *>(std::memchr(text, 0, available));
    if (end == nullptr) {
        return std::nullopt;
    }
    return std::string_view{text, static_cast<std::size_t>(end - text)};
}

void ServerTransport::continueSignon() const noexcept {
    // The engine selected this connection for the packet being parsed, as for the native command,
    // but the source can be left over from the server's own command buffer.
    auto *const source = api.commandSource();
    const auto previous = *source;
    *source = rehlds::clientCommandSource;
    connectionCommand();
    *source = previous;
}

void ServerTransport::releaseIfReady(ClientSlot &slot) noexcept {
    const auto decision =
        settle(slot.client, [&] { return slot.barrier.tryRelease(monotonicNow()); });
    if (decision && *decision == ReleaseDecision::continueSignon) {
        continueSignon();
    }
}

void ServerTransport::onNetCommand(void *chain, rehlds::GameClient client,
                                   std::uint8_t opcode) noexcept {
    auto *slot = liveSlot(client);
    if (slot == nullptr) {
        rehlds::callNext(chain, client.handle(), opcode);
        return;
    }
    // The acknowledgements of this packet are already processed, and only they drain the channel.
    releaseIfReady(*slot);
    slot = liveSlot(client);
    if (slot == nullptr) {
        return;
    }
    const auto text = opcode == rehlds::stringCommandOpcode ? peekCommandText() : std::nullopt;
    if (!text || !transport::isControlCommand(*text)) {
        rehlds::callNext(chain, client.handle(), opcode);
        return;
    }
    const auto command = *text;
    *api.readCount() += static_cast<std::int32_t>(command.size()) + 1;
    const auto received =
        settle(client, [&] { return slot->barrier.receiveCommand(command, monotonicNow()); });
    if (received) {
        releaseIfReady(*slot);
    }
}

void ServerTransport::onStringCommand(void *chain, const char *name, std::int32_t source,
                                      void *client) noexcept {
    const auto command = source == rehlds::clientCommandSource && client != nullptr
                             ? transport::classifySignonCommand(name)
                             : std::nullopt;
    auto *const slot = command ? liveSlot(rehlds::GameClient{client}) : nullptr;
    if (!command || slot == nullptr) {
        rehlds::callNext(chain, name, source, client);
        return;
    }
    const auto signon = *command;
    const auto verdict =
        settle(slot->client, [&] { return slot->barrier.admit(signon, monotonicNow()); });
    if (verdict && *verdict == CommandVerdict::forward) {
        rehlds::callNext(chain, name, source, client);
    }
}

void ServerTransport::openSlot(rehlds::GameClient client) noexcept {
    auto *const slot = slotOf(client);
    if (slot == nullptr || !enabled || !messageId || client.isFakeClient()) {
        return;
    }
    // Whatever this slot held belonged to the connection that this one replaces.
    slot->reset();
    const auto optIn = transport::parseClientOptIn(client.userInfo());
    if (!optIn) {
        refuse(client, optIn.error());
        return;
    }
    const auto connection = randomIdentity<session::ConnectionToken>();
    const auto peer = randomIdentity<session::PeerToken>();
    if (!connection || !peer) {
        refuse(client, {RejectCode::transportUnavailable});
        return;
    }
    slot->emplace(api, client, *optIn, ConnectionBinding{*connection, *peer, generation},
                  *messageId, *this, *this);
}

void ServerTransport::onClientConnected(void *chain, rehlds::GameClient client) noexcept {
    rehlds::callNext(chain, client.handle());
    openSlot(client);
}

void ServerTransport::onDropClient(void *chain, rehlds::GameClient client, bool crash,
                                   const char *reason) noexcept {
    if (auto *const slot = slotOf(client); slot != nullptr) {
        slot->reset();
    }
    rehlds::callNext(chain, client.handle(), crash, reason);
}

void ServerTransport::refreshAvailability() noexcept {
    const auto allowed = api.maxClients() >= fewestClientsForNegotiation;
    // After the game's own registrations, which keep their identifiers, and before the first
    // client: a registration made with clients connected would reach all of them.
    if (allowed && !messageId) {
        messageId = host.registerControlMessage();
    }
    if (connectionCommand == nullptr) {
        connectionCommand = api.findCommand(connectionCommandName);
    }
    const auto ready = allowed && messageId && connectionCommand != nullptr;
    if (ready != enabled) {
        host.report(ready ? "transport active on this map" : "transport inactive on this map");
    }
    enabled = ready;
    if (!enabled) {
        std::ranges::for_each(slots, [](auto &slot) { slot.reset(); });
    }
}

void ServerTransport::onActivateServer(void *chain, std::int32_t runPhysics) noexcept {
    ++generation.value;
    for (auto &slot : slots) {
        if (slot) {
            slot->barrier.beginGeneration(generation);
        }
    }
    rehlds::callNext(chain, runPhysics);
    refreshAvailability();
}

void ServerTransport::onFrame(void *chain) noexcept {
    for (auto &entry : slots) {
        if (entry) {
            auto &slot = *entry;
            std::ignore = settle(slot.client, [&] { return slot.barrier.pump(monotonicNow()); });
        }
    }
    rehlds::callNext(chain);
}

void netCommandHook(void *chain, void *client, std::uint8_t opcode) {
    activeTransport->onNetCommand(chain, rehlds::GameClient{client}, opcode);
}
void stringCommandHook(void *chain, const char *name, std::int32_t source, void *client) {
    activeTransport->onStringCommand(chain, name, source, client);
}
void clientConnectedHook(void *chain, void *client) {
    activeTransport->onClientConnected(chain, rehlds::GameClient{client});
}
void dropClientHook(void *chain, void *client, bool crash, const char *reason) {
    activeTransport->onDropClient(chain, rehlds::GameClient{client}, crash, reason);
}
void activateServerHook(void *chain, std::int32_t runPhysics) {
    activeTransport->onActivateServer(chain, runPhysics);
}
void frameHook(void *chain) {
    activeTransport->onFrame(chain);
}
} // namespace

auto installServerTransport(SelectedEngine engine, std::unique_ptr<TransportProfile> profile,
                            TransportHost host, metamod::RetainedUse use)
    -> std::expected<void, RewriteRefusal> {
    auto models = ServerModelTables::install(engine.image, profile->modelEntries());
    if (!models) {
        return std::unexpected{models.error()};
    }
    activeTransport = std::make_unique<ServerTransport>(engine.api, std::move(profile),
                                                        std::move(*models), host, std::move(use));
    engine.api.install({.netCommand = netCommandHook,
                        .stringCommand = stringCommandHook,
                        .clientConnected = clientConnectedHook,
                        .dropClient = dropClientHook,
                        .activateServer = activateServerHook,
                        .frame = frameHook});
    return {};
}

} // namespace extendedPrecache::platform::windows
