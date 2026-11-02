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

constexpr std::int32_t serverInfoOpcode = 11;
constexpr std::int32_t registerUserMessageOpcode = 39;

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
} // namespace

template <typename Type> auto ClientEngine::at(std::ptrdiff_t offset) const noexcept -> Type * {
    return reinterpret_cast<Type *>(base + offset);
}

template <typename Function>
auto ClientEngine::engineFunction(std::size_t entry) const noexcept -> Function {
    return reinterpret_cast<Function>(at<void *>(layout->engineFunctions)[entry]);
}

auto ClientEngine::holdsVerifiedTables() const noexcept -> bool {
    const auto holdsParser = [this](std::int32_t opcode, std::ptrdiff_t parser) {
        const auto &row = at<ServiceRow>(layout->serviceTable)[opcode];
        return row.opcode == opcode && row.parser == reinterpret_cast<ServiceParser>(base + parser);
    };
    return holdsParser(serverInfoOpcode, layout->serverInfoParser) &&
           holdsParser(registerUserMessageOpcode, layout->registerUserMessageParser) &&
           at<VariableRegistration>(layout->engineFunctions)[engineEntry::registerVariable] ==
               reinterpret_cast<VariableRegistration>(base + layout->variableRegistration);
}

auto ClientEngine::replaceService(Service service, ServiceParser replacement) const noexcept
    -> ServiceParser {
    const auto opcode =
        service == Service::serverInfo ? serverInfoOpcode : registerUserMessageOpcode;
    return std::exchange(at<ServiceRow>(layout->serviceTable)[opcode].parser, replacement);
}

auto ClientEngine::replaceVariableRegistration(VariableRegistration replacement) const noexcept
    -> VariableRegistration {
    return std::exchange(
        at<VariableRegistration>(layout->engineFunctions)[engineEntry::registerVariable],
        replacement);
}

auto ClientEngine::replaceFrame(FrameFunction replacement) const noexcept -> FrameFunction {
    auto &entry = *at<FrameFunction>(layout->frameEntry);
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
    return *at<std::int32_t>(layout->secureServer) != 0;
}

auto ClientEngine::status() const noexcept -> ConnectionStatus {
    using Status = void (*)(NetStatus *);
    const auto *const netApi =
        static_cast<void *const *>(at<void *>(layout->engineFunctions)[engineEntry::netApi]);
    auto current = NetStatus{};
    reinterpret_cast<Status>(netApi[netStatusEntry])(&current);
    return {.connected = current.connected != 0, .connectionTime = current.connectionTime};
}

auto ClientEngine::unreadMessage() const noexcept -> std::span<const std::byte> {
    const auto *const data = *at<const std::byte *>(layout->messageData);
    const auto size = *at<std::int32_t>(layout->messageSize);
    const auto cursor = *at<std::int32_t>(layout->readCursor);
    if (data == nullptr || cursor < 0 || cursor > size) {
        return {};
    }
    return {data + cursor, static_cast<std::size_t>(size - cursor)};
}

void ClientEngine::skipMessage() const noexcept {
    *at<std::int32_t>(layout->readCursor) = *at<std::int32_t>(layout->messageSize);
}

} // namespace extendedPrecache::platform::windows::client
