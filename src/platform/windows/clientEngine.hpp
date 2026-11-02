#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

// Own declarations of what the selected hw.dll keeps in writable tables and of the engine
// functions it hands to its client library. These positions hold only for that exact build.
namespace extendedPrecache::platform::windows::client {

inline constexpr auto selectedEngineSha256 =
    std::string_view{"9ba9a2db5e07598fd59afa35507a98c86162e4e15b3835177b78c11842cd2295"};

using ServiceParser = void (*)();
using FrameFunction = void (*)(double time);
// By value, this occupies the stack exactly as the engine's three separate arguments do on x86.
struct VariableDeclaration final {
    const char *name;
    const char *value;
    std::int32_t flags;
};
static_assert(sizeof(VariableDeclaration) == 12, "The engine's x86 layout is fixed");
using VariableRegistration = void *(*)(VariableDeclaration variable);
using UserMessageHandler = std::int32_t (*)(const char *name, std::int32_t size, void *data);

enum class Service : std::uint8_t { serverInfo, registerUserMessage };

struct ConnectionStatus final {
    bool connected;
    double connectionTime;
};

class ClientEngine final {
  public:
    explicit ClientEngine(void *base) noexcept : base{static_cast<std::byte *>(base)} {}

    // Whether the tables hold what this build was verified to hold, so replacing is safe.
    [[nodiscard]] auto holdsVerifiedTables() const noexcept -> bool;
    [[nodiscard]] auto replaceService(Service service, ServiceParser replacement) const noexcept
        -> ServiceParser;
    [[nodiscard]] auto replaceVariableRegistration(VariableRegistration replacement) const noexcept
        -> VariableRegistration;
    // Empty until the engine has loaded the client library.
    [[nodiscard]] auto replaceFrame(FrameFunction replacement) const noexcept -> FrameFunction;

    void hookUserMessage(const char *name, UserMessageHandler handler) const noexcept;
    void serverCommand(const char *text) const noexcept;
    void clientCommand(const char *text) const noexcept;
    void consolePrint(const char *text) const noexcept;
    [[nodiscard]] auto localInfo(const char *key) const noexcept -> const char *;
    [[nodiscard]] auto status() const noexcept -> ConnectionStatus;
    // What the server declared in its challenge reply, which comes before the connect request.
    [[nodiscard]] auto serverIsSecure() const noexcept -> bool;

    // The message being parsed, from the read cursor to its end.
    [[nodiscard]] auto unreadMessage() const noexcept -> std::span<const std::byte>;
    // Leaves the message at its end, so the engine parses nothing more of it.
    void skipMessage() const noexcept;

  private:
    template <typename Type> [[nodiscard]] auto at(std::ptrdiff_t offset) const noexcept -> Type *;
    template <typename Function>
    [[nodiscard]] auto engineFunction(std::size_t entry) const noexcept -> Function;

    std::byte *base;
};

} // namespace extendedPrecache::platform::windows::client
