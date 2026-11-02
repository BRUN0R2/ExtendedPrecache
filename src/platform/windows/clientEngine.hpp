#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

// Own declarations of what the selected hw.dll keeps in writable tables and of the engine
// functions it hands to its client library. These positions hold only for that exact build.
namespace extendedPrecache::platform::windows::client {

using ServiceParser = void (*)();
using FrameFunction = void (*)(double time);
using UserMessageHandler = std::int32_t (*)(const char *name, std::int32_t size, void *data);

enum class Service : std::uint8_t { serverInfo, registerUserMessage };

struct ConnectionStatus final {
    bool connected;
    double connectionTime;
};

class ClientEngine final {
  public:
    explicit ClientEngine(void *base) noexcept : base{static_cast<std::byte *>(base)} {}

    // Empty when the table does not hold what this build was verified to hold.
    [[nodiscard]] auto replaceService(Service service, ServiceParser replacement) const noexcept
        -> ServiceParser;
    // Empty until the engine has loaded the client library.
    [[nodiscard]] auto replaceFrame(FrameFunction replacement) const noexcept -> FrameFunction;

    void hookUserMessage(const char *name, UserMessageHandler handler) const noexcept;
    void serverCommand(const char *text) const noexcept;
    void clientCommand(const char *text) const noexcept;
    void consolePrint(const char *text) const noexcept;
    [[nodiscard]] auto status() const noexcept -> ConnectionStatus;

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
