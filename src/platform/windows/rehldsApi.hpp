#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

// Own declarations of the binary layouts used from the engine's ReHLDS API, major 3, on x86.
// Nothing is copied from its headers, and this is not a general binding of that API.
namespace extendedPrecache::platform::windows::rehlds {

struct SizeBuffer final {
    const char *name;
    std::uint16_t flags;
    std::byte *data;
    std::int32_t capacity;
    std::int32_t used;
};
static_assert(sizeof(SizeBuffer) == 20, "The engine's x86 layout is fixed");

using CommandFunction = void (*)();

inline constexpr std::int32_t clientCommandSource = 0;
inline constexpr std::uint8_t stringCommandOpcode = 3;

template <typename Result, typename... Arguments>
auto callSlot(void *object, std::size_t slot, Arguments... arguments) noexcept -> Result {
    using Method = Result(__thiscall *)(void *, Arguments...);
    const auto *const table = *static_cast<void *const *const *>(object);
    return reinterpret_cast<Method>(table[slot])(object, arguments...);
}

// Continues a hook chain; every hook must do this unless it means to stop the engine's own step.
template <typename... Arguments> void callNext(void *chain, Arguments... arguments) noexcept {
    constexpr std::size_t nextSlot = 1;
    callSlot<void>(chain, nextSlot, arguments...);
}

class GameClient final {
  public:
    explicit GameClient(void *object) noexcept : object{object} {}
    [[nodiscard]] auto handle() const noexcept -> void * {
        return object;
    }
    [[nodiscard]] auto id() const noexcept -> std::int32_t;
    [[nodiscard]] auto isConnected() const noexcept -> bool;
    [[nodiscard]] auto isFakeClient() const noexcept -> bool;
    [[nodiscard]] auto userInfo() const noexcept -> const char *;
    [[nodiscard]] auto messageBuffer() const noexcept -> SizeBuffer *;

  private:
    void *object;
};

// True when nothing reliable is staged, in flight, fragmented or waiting to be fragmented. These
// channel fields have no accessor in the API, so this holds only for the selected engine build.
[[nodiscard]] auto channelDrained(const SizeBuffer &message) noexcept -> bool;

struct Hooks final {
    void (*netCommand)(void *chain, void *client, std::uint8_t opcode);
    void (*stringCommand)(void *chain, const char *name, std::int32_t source, void *client);
    void (*clientConnected)(void *chain, void *client);
    void (*dropClient)(void *chain, void *client, bool crash, const char *reason);
    void (*activateServer)(void *chain, std::int32_t runPhysics);
    void (*frame)(void *chain);
};

class Api final {
  public:
    // Empty when the object reports another major version or an older minor one.
    [[nodiscard]] static auto acquire(void *object) noexcept -> std::optional<Api>;

    void install(const Hooks &hooks) const noexcept;
    [[nodiscard]] auto maxClients() const noexcept -> std::int32_t;
    [[nodiscard]] auto spawnCount() const noexcept -> std::int32_t;
    [[nodiscard]] auto netMessage() const noexcept -> SizeBuffer *;
    [[nodiscard]] auto readCount() const noexcept -> std::int32_t *;
    [[nodiscard]] auto commandSource() const noexcept -> std::int32_t *;
    [[nodiscard]] auto findCommand(std::string_view name) const noexcept -> CommandFunction;
    void writeBuffer(SizeBuffer &buffer, std::int32_t size, const void *data) const noexcept;
    void dropClient(GameClient client, const char *reason) const noexcept;

  private:
    struct Tables final {
        void *hookchains;
        void *serverStatic;
        void *const *functions;
    };
    explicit Api(Tables tables) noexcept : tables{tables} {}

    Tables tables;
};

} // namespace extendedPrecache::platform::windows::rehlds
