#include "clientEngine.hpp"

#include <array>
#include <utility>

namespace extendedPrecache::platform::windows::client {
namespace {
struct ServiceRow final {
    std::int32_t opcode;
    const char *name;
    ServiceParser parser;
};
static_assert(sizeof(ServiceRow) == 12, "The engine's x86 layout is fixed");

struct NetStatus final {
    std::int32_t connected;
    std::array<std::byte, 40> addresses;
    std::int32_t packetLoss;
    double latency;
    double connectionTime;
    double rate;
};
static_assert(sizeof(NetStatus) == 72, "The engine's x86 layout is fixed");

constexpr std::ptrdiff_t serviceTableOffset = 0x31a238;
constexpr std::ptrdiff_t engineFunctionsOffset = 0x319b08;
constexpr std::ptrdiff_t frameEntryOffset = 0x145efc4;
constexpr std::ptrdiff_t messageDataOffset = 0x1245fe8;
constexpr std::ptrdiff_t messageSizeOffset = 0x1245ff0;
constexpr std::ptrdiff_t readCursorOffset = 0x124c984;
constexpr std::ptrdiff_t secureServerOffset = 0x140ea00;
constexpr std::ptrdiff_t variableRegistrationOffset = 0x195b30;

struct ServiceEntry final {
    std::int32_t opcode;
    std::ptrdiff_t parserOffset;
};
constexpr auto serverInfoEntry = ServiceEntry{.opcode = 11, .parserOffset = 0x1aa2d0};
constexpr auto registrationEntry = ServiceEntry{.opcode = 39, .parserOffset = 0x1aa090};

namespace engineEntry {
constexpr std::size_t registerVariable = 14;
constexpr std::size_t hookUserMessage = 18;
constexpr std::size_t serverCommand = 19;
constexpr std::size_t clientCommand = 20;
constexpr std::size_t consolePrint = 30;
constexpr std::size_t netApi = 86;
constexpr std::size_t localInfo = 116;
} // namespace engineEntry
constexpr std::size_t netStatusEntry = 1;

constexpr auto entryOf(Service service) noexcept -> ServiceEntry {
    return service == Service::serverInfo ? serverInfoEntry : registrationEntry;
}
} // namespace

template <typename Type> auto ClientEngine::at(std::ptrdiff_t offset) const noexcept -> Type * {
    return reinterpret_cast<Type *>(base + offset);
}

template <typename Function>
auto ClientEngine::engineFunction(std::size_t entry) const noexcept -> Function {
    return reinterpret_cast<Function>(at<void *>(engineFunctionsOffset)[entry]);
}

auto ClientEngine::holdsVerifiedTables() const noexcept -> bool {
    const auto holdsParser = [this](ServiceEntry entry) {
        const auto &row = at<ServiceRow>(serviceTableOffset)[entry.opcode];
        return row.opcode == entry.opcode &&
               row.parser == reinterpret_cast<ServiceParser>(base + entry.parserOffset);
    };
    return holdsParser(serverInfoEntry) && holdsParser(registrationEntry) &&
           at<VariableRegistration>(engineFunctionsOffset)[engineEntry::registerVariable] ==
               reinterpret_cast<VariableRegistration>(base + variableRegistrationOffset);
}

auto ClientEngine::replaceService(Service service, ServiceParser replacement) const noexcept
    -> ServiceParser {
    return std::exchange(at<ServiceRow>(serviceTableOffset)[entryOf(service).opcode].parser,
                         replacement);
}

auto ClientEngine::replaceVariableRegistration(VariableRegistration replacement) const noexcept
    -> VariableRegistration {
    return std::exchange(
        at<VariableRegistration>(engineFunctionsOffset)[engineEntry::registerVariable],
        replacement);
}

auto ClientEngine::replaceFrame(FrameFunction replacement) const noexcept -> FrameFunction {
    auto &entry = *at<FrameFunction>(frameEntryOffset);
    const auto previous = entry;
    if (previous != nullptr) {
        entry = replacement;
    }
    return previous;
}

void ClientEngine::hookUserMessage(const char *name, UserMessageHandler handler) const noexcept {
    using Hook = std::int32_t (*)(const char *, UserMessageHandler);
    engineFunction<Hook>(engineEntry::hookUserMessage)(name, handler);
}

void ClientEngine::serverCommand(const char *text) const noexcept {
    engineFunction<std::int32_t (*)(const char *)>(engineEntry::serverCommand)(text);
}

void ClientEngine::clientCommand(const char *text) const noexcept {
    engineFunction<std::int32_t (*)(const char *)>(engineEntry::clientCommand)(text);
}

void ClientEngine::consolePrint(const char *text) const noexcept {
    engineFunction<void (*)(const char *)>(engineEntry::consolePrint)(text);
}

auto ClientEngine::localInfo(const char *key) const noexcept -> const char * {
    using LocalInfo = const char *(*)(const char *);
    return engineFunction<LocalInfo>(engineEntry::localInfo)(key);
}

auto ClientEngine::serverIsSecure() const noexcept -> bool {
    return *at<std::int32_t>(secureServerOffset) != 0;
}

auto ClientEngine::status() const noexcept -> ConnectionStatus {
    using Status = void (*)(NetStatus *);
    const auto *const netApi =
        static_cast<void *const *>(at<void *>(engineFunctionsOffset)[engineEntry::netApi]);
    auto current = NetStatus{};
    reinterpret_cast<Status>(netApi[netStatusEntry])(&current);
    return {.connected = current.connected != 0, .connectionTime = current.connectionTime};
}

auto ClientEngine::unreadMessage() const noexcept -> std::span<const std::byte> {
    const auto *const data = *at<const std::byte *>(messageDataOffset);
    const auto size = *at<std::int32_t>(messageSizeOffset);
    const auto cursor = *at<std::int32_t>(readCursorOffset);
    if (data == nullptr || cursor < 0 || cursor > size) {
        return {};
    }
    return {data + cursor, static_cast<std::size_t>(size - cursor)};
}

void ClientEngine::skipMessage() const noexcept {
    *at<std::int32_t>(readCursorOffset) = *at<std::int32_t>(messageSizeOffset);
}

} // namespace extendedPrecache::platform::windows::client
