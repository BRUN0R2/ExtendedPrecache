#include "rehldsApi.hpp"

#include <algorithm>
#include <array>

namespace extendedPrecache::platform::windows::rehlds {
namespace {
constexpr std::int32_t requiredMajor = 3;
constexpr std::int32_t requiredMinor = 15;
// The hook runs ahead of other plugins, which must not see what the barrier holds or consumes.
constexpr std::int32_t firstHookPriority = 255;
constexpr std::size_t registerHookSlot = 0;

// Virtual slots count the destructor where the interface declares one.
namespace apiSlot {
constexpr std::size_t majorVersion = 1;
constexpr std::size_t minorVersion = 2;
constexpr std::size_t functions = 3;
constexpr std::size_t hookchains = 4;
constexpr std::size_t serverStatic = 5;
} // namespace apiSlot
namespace hookSlot {
constexpr std::size_t clientConnected = 16;
constexpr std::size_t netCommand = 17;
constexpr std::size_t stringCommand = 20;
constexpr std::size_t dropClient = 28;
constexpr std::size_t activateServer = 29;
constexpr std::size_t frame = 41;
} // namespace hookSlot
namespace clientSlot {
constexpr std::size_t id = 0;
constexpr std::size_t netChannel = 5;
constexpr std::size_t isConnected = 10;
constexpr std::size_t isFakeClient = 19;
constexpr std::size_t userInfo = 68;
} // namespace clientSlot
constexpr std::size_t channelMessageSlot = 1;
namespace serverSlot {
constexpr std::size_t maxClients = 1;
constexpr std::size_t spawnCount = 8;
} // namespace serverSlot
namespace functionSlot {
constexpr std::size_t dropClient = 0;
constexpr std::size_t netMessage = 3;
constexpr std::size_t readCount = 5;
constexpr std::size_t commandSource = 20;
constexpr std::size_t writeBuffer = 35;
constexpr std::size_t firstCommand = 98;
} // namespace functionSlot

struct CommandNode final {
    CommandNode *next;
    const char *name;
    CommandFunction function;
    std::int32_t flags;
};

// Channel fields, relative to the message buffer that the API does expose.
constexpr std::ptrdiff_t reliableLengthOffset = 0xfac;
constexpr std::ptrdiff_t waitListsOffset = 0x1f48;
constexpr std::ptrdiff_t fragmentsOffset = 0x1f60;
constexpr std::size_t streamCount = 2;

template <typename Function>
auto functionAt(void *const *table, std::size_t slot) noexcept -> Function {
    return reinterpret_cast<Function>(table[slot]);
}

auto noStreams(const std::byte *field) noexcept -> bool {
    const auto &streams = *reinterpret_cast<const std::array<const void *, streamCount> *>(field);
    return std::ranges::all_of(streams, [](const void *stream) { return stream == nullptr; });
}

template <typename Hook> void registerHook(void *hookchains, std::size_t slot, Hook hook) noexcept {
    auto *const registry = callSlot<void *>(hookchains, slot);
    callSlot<void>(registry, registerHookSlot, hook, firstHookPriority);
}
} // namespace

auto GameClient::id() const noexcept -> std::int32_t {
    return callSlot<std::int32_t>(object, clientSlot::id);
}
auto GameClient::isConnected() const noexcept -> bool {
    return callSlot<bool>(object, clientSlot::isConnected);
}
auto GameClient::isFakeClient() const noexcept -> bool {
    return callSlot<bool>(object, clientSlot::isFakeClient);
}
auto GameClient::userInfo() const noexcept -> const char * {
    return callSlot<const char *>(object, clientSlot::userInfo);
}
auto GameClient::messageBuffer() const noexcept -> SizeBuffer * {
    auto *const channel = callSlot<void *>(object, clientSlot::netChannel);
    return callSlot<SizeBuffer *>(channel, channelMessageSlot);
}

auto channelDrained(const SizeBuffer &message) noexcept -> bool {
    const auto *const base = reinterpret_cast<const std::byte *>(&message);
    const auto reliableLength =
        *reinterpret_cast<const std::int32_t *>(base + reliableLengthOffset);
    return message.used == 0 && reliableLength == 0 && noStreams(base + waitListsOffset) &&
           noStreams(base + fragmentsOffset);
}

auto Api::acquire(void *object) noexcept -> std::optional<Api> {
    if (object == nullptr ||
        callSlot<std::int32_t>(object, apiSlot::majorVersion) != requiredMajor ||
        callSlot<std::int32_t>(object, apiSlot::minorVersion) < requiredMinor) {
        return std::nullopt;
    }
    return Api{Tables{.hookchains = callSlot<void *>(object, apiSlot::hookchains),
                      .serverStatic = callSlot<void *>(object, apiSlot::serverStatic),
                      .functions = callSlot<void *const *>(object, apiSlot::functions)}};
}

void Api::install(const Hooks &hooks) const noexcept {
    registerHook(tables.hookchains, hookSlot::clientConnected, hooks.clientConnected);
    registerHook(tables.hookchains, hookSlot::netCommand, hooks.netCommand);
    registerHook(tables.hookchains, hookSlot::stringCommand, hooks.stringCommand);
    registerHook(tables.hookchains, hookSlot::dropClient, hooks.dropClient);
    registerHook(tables.hookchains, hookSlot::activateServer, hooks.activateServer);
    registerHook(tables.hookchains, hookSlot::frame, hooks.frame);
}

auto Api::maxClients() const noexcept -> std::int32_t {
    return callSlot<std::int32_t>(tables.serverStatic, serverSlot::maxClients);
}
auto Api::spawnCount() const noexcept -> std::int32_t {
    return callSlot<std::int32_t>(tables.serverStatic, serverSlot::spawnCount);
}
auto Api::netMessage() const noexcept -> SizeBuffer * {
    return functionAt<SizeBuffer *(*)()>(tables.functions, functionSlot::netMessage)();
}
auto Api::readCount() const noexcept -> std::int32_t * {
    return functionAt<std::int32_t *(*)()>(tables.functions, functionSlot::readCount)();
}
auto Api::commandSource() const noexcept -> std::int32_t * {
    return functionAt<std::int32_t *(*)()>(tables.functions, functionSlot::commandSource)();
}
auto Api::findCommand(std::string_view name) const noexcept -> CommandFunction {
    const auto *node =
        functionAt<CommandNode *(*)()>(tables.functions, functionSlot::firstCommand)();
    for (; node != nullptr; node = node->next) {
        if (node->name != nullptr && name == node->name) {
            return node->function;
        }
    }
    return nullptr;
}
void Api::writeBuffer(SizeBuffer &buffer, std::int32_t size, const void *data) const noexcept {
    using WriteBuffer = void (*)(SizeBuffer *, std::int32_t, const void *);
    functionAt<WriteBuffer>(tables.functions, functionSlot::writeBuffer)(&buffer, size, data);
}
void Api::dropClient(GameClient client, const char *reason) const noexcept {
    using DropClient = void (*)(void *, bool, const char *, ...);
    functionAt<DropClient>(tables.functions, functionSlot::dropClient)(client.handle(), false, "%s",
                                                                       reason);
}

} // namespace extendedPrecache::platform::windows::rehlds
