#include "buildIdentity.hpp"
#include "clientBuild.hpp"
#include "clientLoadStatus.hpp"
#include "clientTransport.hpp"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace extendedPrecache::platform::windows;
using client::AttachResult;
using client::AttachStatusReader;

constexpr auto engineModule = L"hw.dll";
constexpr auto clientModule = L"extendedPrecacheClient.dll";
constexpr auto gameDirectoryOption = std::wstring_view{L"--game-dir"};
constexpr auto passThroughOption = std::wstring_view{L"--"};
constexpr auto defaultArguments = L"-game cstrike";
constexpr auto vacWarning =
    "ExtendedPrecache: WARNING: this program changes the memory of the running game, and VAC may "
    "ban the Steam account that uses it. VAC bans cannot be appealed. Never use it on VAC-secured "
    "servers or on your main account, and use a copy of the game outside the Steam folder.";
constexpr DWORD reportTimeoutMs = 120000;
constexpr DWORD longestPath = 32768;

enum class ExitCode : std::uint8_t {
    ready = 0,
    badArguments,
    noApprovedProfile,
    gameNotFound,
    unknownGameBuild,
    clientModuleMissing,
    cannotStart,
    cannotLoad,
    gameClosed,
    attachFailed
};

auto fail(ExitCode code, const char *text) noexcept -> ExitCode {
    std::fprintf(stderr, "ExtendedPrecache: %s\n", text);
    return code;
}

auto refuseBuild(BuildRefusal refusal) noexcept -> ExitCode {
    return refusal == BuildRefusal::unreadable
               ? fail(ExitCode::gameNotFound, "the game files were not found in that directory")
               : fail(ExitCode::unknownGameBuild, "this game build has not been verified");
}

struct Options final {
    std::optional<std::filesystem::path> gameDirectory;
    std::vector<std::wstring_view> gameArguments;
};

auto parseOptions(std::span<wchar_t *const> arguments) -> std::optional<Options> {
    auto options = Options{};
    for (auto index = std::size_t{1}; index < arguments.size(); ++index) {
        const auto argument = std::wstring_view{arguments[index]};
        if (argument == passThroughOption) {
            options.gameArguments.assign(arguments.begin() + static_cast<std::ptrdiff_t>(index) + 1,
                                         arguments.end());
            return options;
        }
        if (argument != gameDirectoryOption || index + 1 == arguments.size()) {
            return std::nullopt;
        }
        options.gameDirectory = std::filesystem::path{arguments[++index]};
    }
    return options;
}

auto launcherDirectory() -> std::optional<std::filesystem::path> {
    auto text = std::wstring(longestPath, L'\0');
    const auto length = GetModuleFileNameW(nullptr, text.data(), longestPath);
    if (length == 0 || length == longestPath) {
        return std::nullopt;
    }
    text.resize(length);
    return std::filesystem::path{text}.parent_path();
}

// Follows the rules the C runtime uses to split a command line back into arguments.
void appendArgument(std::wstring &line, std::wstring_view argument) {
    line += L' ';
    if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring_view::npos) {
        line += argument;
        return;
    }
    line += L'"';
    auto backslashes = std::size_t{};
    for (const auto character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        line.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        backslashes = 0;
        line += character;
    }
    line.append(backslashes * 2, L'\\');
    line += L'"';
}

auto commandLine(const std::filesystem::path &executable,
                 std::span<const std::wstring_view> arguments) -> std::wstring {
    auto line = std::wstring{L'"'} + executable.native() + L"\" " + defaultArguments;
    for (const auto argument : arguments) {
        appendArgument(line, argument);
    }
    return line;
}

class UniqueHandle final {
  public:
    explicit UniqueHandle(HANDLE handle) noexcept : handle{handle} {}
    UniqueHandle(const UniqueHandle &) = delete;
    auto operator=(const UniqueHandle &) -> UniqueHandle & = delete;
    ~UniqueHandle() {
        CloseHandle(handle);
    }
    [[nodiscard]] auto get() const noexcept -> HANDLE {
        return handle;
    }

  private:
    HANDLE handle;
};

// Runs the module's load as the first thing the game's main thread does, ahead of its entry point.
auto queueModuleLoad(const PROCESS_INFORMATION &game, const std::filesystem::path &module) noexcept
    -> bool {
    const auto &text = module.native();
    const auto bytes = (text.size() + 1) * sizeof(wchar_t);
    auto *const remote =
        VirtualAllocEx(game.hProcess, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote == nullptr ||
        WriteProcessMemory(game.hProcess, remote, text.c_str(), bytes, nullptr) == FALSE) {
        return false;
    }
    const auto load = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    return load != nullptr && QueueUserAPC(reinterpret_cast<PAPCFUNC>(load), game.hThread,
                                           reinterpret_cast<ULONG_PTR>(remote)) != 0;
}

auto waitForReport(HANDLE process, const AttachStatusReader &status) noexcept -> ExitCode {
    const auto handles = std::array{status.signal(), process};
    const auto signaled = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(),
                                                 FALSE, reportTimeoutMs);
    if (signaled == WAIT_OBJECT_0 + 1) {
        return fail(ExitCode::gameClosed, "the game closed before reporting back");
    }
    const auto result = status.result();
    if (signaled != WAIT_OBJECT_0 || result != AttachResult::installed) {
        // The game keeps running: without the negotiation it is an ordinary legacy client.
        return fail(ExitCode::attachFailed, client::describe(result));
    }
    std::puts("ExtendedPrecache: negotiation ready in the game.");
    return ExitCode::ready;
}

struct GameStart final {
    std::filesystem::path directory;
    std::filesystem::path executable;
    std::filesystem::path module;
    std::span<const std::wstring_view> arguments;
};

auto startGame(const GameStart &start) -> ExitCode {
    auto line = commandLine(start.executable, start.arguments);
    auto startup = STARTUPINFOW{};
    startup.cb = sizeof(startup);
    auto created = PROCESS_INFORMATION{};
    if (CreateProcessW(start.executable.c_str(), line.data(), nullptr, nullptr, FALSE,
                       CREATE_SUSPENDED, nullptr, start.directory.c_str(), &startup,
                       &created) == FALSE) {
        return fail(ExitCode::cannotStart, "the game could not be started");
    }
    const auto process = UniqueHandle{created.hProcess};
    const auto thread = UniqueHandle{created.hThread};
    const auto status = AttachStatusReader::create(created.dwProcessId);
    if (!status || !queueModuleLoad(created, start.module)) {
        // Nothing of it has run yet, so this leaves nothing behind.
        TerminateProcess(process.get(), 1);
        return fail(ExitCode::cannotLoad, "the client module could not be loaded into the game");
    }
    ResumeThread(thread.get());
    return waitForReport(process.get(), *status);
}

auto run(std::span<wchar_t *const> arguments) -> ExitCode {
    std::fprintf(stderr, "%s\n", vacWarning);
    const auto options = parseOptions(arguments);
    // Never defaults to Steam's own folder: the copy this runs on has to be a deliberate choice.
    const auto directory = options ? options->gameDirectory : std::nullopt;
    if (!directory) {
        return fail(ExitCode::badArguments,
                    "usage: extendedPrecacheLauncher --game-dir <path> [-- <game arguments>]");
    }
    if (!client::approvedClientProfile()) {
        return fail(ExitCode::noApprovedProfile,
                    "no contract profile has been approved, so there is nothing to negotiate");
    }
    const auto build = client::identifyClientBuild(*directory / engineModule);
    if (!build) {
        return refuseBuild(build.error());
    }
    const auto executable = *directory / (*build)->executable;
    if (const auto verified = verifyFileBuild(executable, (*build)->executableSha256); !verified) {
        return refuseBuild(verified.error());
    }
    const auto own = launcherDirectory();
    if (!own || !std::filesystem::exists(*own / clientModule)) {
        return fail(ExitCode::clientModuleMissing,
                    "extendedPrecacheClient.dll must be next to this program");
    }
    return startGame({.directory = *directory,
                      .executable = executable,
                      .module = *own / clientModule,
                      .arguments = options->gameArguments});
}
} // namespace

auto wmain(int count, wchar_t **values) -> int {
    return std::to_underlying(run({values, static_cast<std::size_t>(count)}));
}
