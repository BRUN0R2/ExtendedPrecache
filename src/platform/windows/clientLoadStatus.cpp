#include "clientLoadStatus.hpp"

#include <array>
#include <cstdio>
#include <utility>

namespace extendedPrecache::platform::windows::client {
namespace {
constexpr auto mappingFormat = L"Local\\ExtendedPrecacheAttach%lu";
constexpr auto eventFormat = L"Local\\ExtendedPrecacheAttached%lu";
constexpr std::size_t nameCharacters = 64;
constexpr DWORD statusBytes = sizeof(AttachResult);

using ObjectName = std::array<wchar_t, nameCharacters>;

auto objectName(const wchar_t *format, DWORD gameProcess) noexcept -> ObjectName {
    auto name = ObjectName{};
    std::swprintf(name.data(), name.size(), format, gameProcess);
    return name;
}
} // namespace

auto describe(AttachResult result) noexcept -> const char * {
    switch (result) {
    case AttachResult::pending:
        return "the game did not report back";
    case AttachResult::installed:
        return "negotiation ready in the game";
    case AttachResult::noApprovedProfile:
        return "no contract profile has been approved";
    case AttachResult::engineUnreadable:
        return "the game engine file could not be verified";
    case AttachResult::engineUnknownBuild:
        return "this game engine build has not been verified";
    case AttachResult::noBinding:
        return "the system could not provide random identities";
    case AttachResult::unexpectedTables:
        return "the game engine does not hold what this build was verified to hold";
    case AttachResult::outOfMemory:
        return "out of memory";
    }
    return "unknown result";
}

void publishAttachResult(AttachResult result) noexcept {
    const auto process = GetCurrentProcessId();
    auto *const mapping =
        OpenFileMappingW(FILE_MAP_WRITE, FALSE, objectName(mappingFormat, process).data());
    if (mapping == nullptr) {
        return;
    }
    if (auto *const view = MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, statusBytes);
        view != nullptr) {
        *static_cast<AttachResult *>(view) = result;
        UnmapViewOfFile(view);
    }
    CloseHandle(mapping);
    auto *const published =
        OpenEventW(EVENT_MODIFY_STATE, FALSE, objectName(eventFormat, process).data());
    if (published != nullptr) {
        SetEvent(published);
        CloseHandle(published);
    }
}

auto AttachStatusReader::create(DWORD gameProcess) noexcept -> std::optional<AttachStatusReader> {
    auto *const mapping =
        CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, statusBytes,
                           objectName(mappingFormat, gameProcess).data());
    if (mapping == nullptr) {
        return std::nullopt;
    }
    auto *const published =
        CreateEventW(nullptr, TRUE, FALSE, objectName(eventFormat, gameProcess).data());
    auto *const view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, statusBytes);
    if (published == nullptr || view == nullptr) {
        if (published != nullptr) {
            CloseHandle(published);
        }
        CloseHandle(mapping);
        return std::nullopt;
    }
    return AttachStatusReader{Owned{.mapping = mapping, .published = published, .view = view}};
}

AttachStatusReader::AttachStatusReader(AttachStatusReader &&other) noexcept
    : owned{std::exchange(other.owned, Owned{})} {}

AttachStatusReader::~AttachStatusReader() {
    if (owned.view != nullptr) {
        UnmapViewOfFile(owned.view);
    }
    if (owned.published != nullptr) {
        CloseHandle(owned.published);
    }
    if (owned.mapping != nullptr) {
        CloseHandle(owned.mapping);
    }
}

auto AttachStatusReader::result() const noexcept -> AttachResult {
    return *static_cast<const AttachResult *>(owned.view);
}

} // namespace extendedPrecache::platform::windows::client
