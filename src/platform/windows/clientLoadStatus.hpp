#pragma once

#include <windows.h>

#include <cstdint>
#include <optional>

// How the client DLL tells the launcher what happened, since the engine's console does not exist
// yet when it attaches. The launcher owns both named objects, per game process.
namespace extendedPrecache::platform::windows::client {

enum class AttachResult : std::uint8_t {
    pending,
    installed,
    noApprovedProfile,
    engineUnreadable,
    engineUnknownBuild,
    noBinding,
    unexpectedTables,
    outOfMemory
};

[[nodiscard]] auto describe(AttachResult result) noexcept -> const char *;

// Does nothing in a process that no launcher is waiting for.
void publishAttachResult(AttachResult result) noexcept;

class AttachStatusReader final {
  public:
    [[nodiscard]] static auto create(DWORD gameProcess) noexcept
        -> std::optional<AttachStatusReader>;
    AttachStatusReader(const AttachStatusReader &) = delete;
    auto operator=(const AttachStatusReader &) -> AttachStatusReader & = delete;
    AttachStatusReader(AttachStatusReader &&other) noexcept;
    auto operator=(AttachStatusReader &&) -> AttachStatusReader & = delete;
    ~AttachStatusReader();

    [[nodiscard]] auto signal() const noexcept -> HANDLE {
        return owned.published;
    }
    [[nodiscard]] auto result() const noexcept -> AttachResult;

  private:
    struct Owned final {
        HANDLE mapping;
        HANDLE published;
        void *view;
    };
    explicit AttachStatusReader(Owned owned) noexcept : owned{owned} {}

    Owned owned;
};

} // namespace extendedPrecache::platform::windows::client
