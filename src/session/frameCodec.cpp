#include <extendedPrecache/session/frameCodec.hpp>

#include <concepts>
#include <type_traits>

namespace extendedPrecache::session {
namespace {
constexpr auto wireMagic =
    std::array{std::byte{'E'}, std::byte{'P'}, std::byte{'C'}, std::byte{'N'}};
constexpr auto wireMajor = std::uint8_t{1};
constexpr auto wireMinor = std::uint16_t{0};
constexpr auto bitsPerByte = 8U;
constexpr auto commandPrefix = std::string_view{"epc1 "};
constexpr auto offerBytes = 24U;
constexpr auto rejectionBytes = 12U;

class Writer final {
  public:
    explicit Writer(std::span<std::byte> output) : target{output} {}
    template <std::unsigned_integral T> void integer(T value) noexcept {
        for (std::size_t index = 0; index < sizeof(T); ++index) {
            target[position++] = static_cast<std::byte>(
                (static_cast<std::uint64_t>(value) >> (index * bitsPerByte)) & 0xffU);
        }
    }
    void bytes(std::span<const std::byte> input) noexcept {
        std::ranges::copy(input, target.begin() + static_cast<std::ptrdiff_t>(position));
        position += input.size();
    }

  private:
    std::span<std::byte> target;
    std::size_t position{};
};
class Reader final {
  public:
    explicit Reader(std::span<const std::byte> input) : source{input} {}
    template <std::unsigned_integral T> auto integer() noexcept -> T {
        std::uint64_t result{};
        for (std::size_t index = 0; index < sizeof(T); ++index) {
            result |= std::to_integer<std::uint64_t>(source[position++]) << (index * bitsPerByte);
        }
        return static_cast<T>(result);
    }
    template <typename T> auto identity() noexcept -> T {
        T result;
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(position), result.bytes.size(),
                    result.bytes.begin());
        position += result.bytes.size();
        return result;
    }

  private:
    std::span<const std::byte> source;
    std::size_t position{};
};

auto validPolicy(ConnectionPolicy policy) noexcept -> bool {
    return policy == ConnectionPolicy::preferExtended ||
           policy == ConnectionPolicy::requireExtended;
}
auto validTimeout(DurationMs timeout) noexcept -> bool {
    return timeout.value >= minTimeoutMs && timeout.value <= maxTimeoutMs;
}
auto expectedBodyBytes(MessageKind kind) noexcept -> std::size_t {
    if (kind == MessageKind::serverOffer || kind == MessageKind::clientHello) {
        return offerBytes;
    }
    return kind == MessageKind::reject ? rejectionBytes : maxBodyBytes;
}
template <MessageKind kind>
auto readPage(Reader &reader) noexcept -> std::expected<FramePayload, Rejection> {
    LimitPageBody<kind> result{};
    const auto first = reader.integer<std::uint8_t>();
    if (first < 1 || first > capabilityCount || (first - 1) % limitsPerPage != 0) {
        return std::unexpected{Rejection{RejectCode::malformedFrame}};
    }
    result.ordinal = {static_cast<std::uint8_t>((first - 1) / limitsPerPage)};
    for (std::size_t index = 0; index < limitsPerPage; ++index) {
        if (index != 0 && reader.integer<std::uint8_t>() != first + index) {
            return std::unexpected{Rejection{RejectCode::malformedFrame}};
        }
        if (reader.integer<std::uint8_t>() != 0 || reader.integer<std::uint16_t>() != 0) {
            return std::unexpected{Rejection{RejectCode::malformedFrame}};
        }
        result.wireValues[index] = reader.integer<std::uint32_t>();
    }
    return result;
}

auto readPayload(MessageKind kind, Reader &reader) noexcept
    -> std::expected<FramePayload, Rejection> {
    if (kind == MessageKind::serverOffer || kind == MessageKind::clientHello) {
        if (reader.integer<std::uint16_t>() != wireMinor) {
            return std::unexpected{Rejection{RejectCode::unsupportedVersion}};
        }
        const auto policy = static_cast<ConnectionPolicy>(reader.integer<std::uint8_t>());
        if (!validPolicy(policy) || reader.integer<std::uint8_t>() != 0) {
            return std::unexpected{Rejection{RejectCode::malformedFrame}};
        }
        if (kind == MessageKind::serverOffer) {
            ServerOfferBody result{policy,
                                   {reader.integer<std::uint32_t>()},
                                   {reader.integer<std::uint64_t>()},
                                   {reader.integer<std::uint32_t>()},
                                   {reader.integer<std::uint32_t>()}};
            if (result.epoch.value == 0 || result.dataContract.value == 0 ||
                !validTimeout(result.timeout)) {
                return std::unexpected{Rejection{RejectCode::malformedFrame}};
            }
            return result;
        }
        ClientHelloBody result{
            policy, reader.identity<ClientNonce>(), {reader.integer<std::uint32_t>()}};
        if (result.nonce.isZero() || !validTimeout(result.timeout)) {
            return std::unexpected{Rejection{RejectCode::malformedFrame}};
        }
        return result;
    }
    switch (kind) {
    case MessageKind::requirementsPage:
        return readPage<MessageKind::requirementsPage>(reader);
    case MessageKind::serverLimitsPage:
        return readPage<MessageKind::serverLimitsPage>(reader);
    case MessageKind::clientLimitsPage:
        return readPage<MessageKind::clientLimitsPage>(reader);
    case MessageKind::selectionPage:
        return readPage<MessageKind::selectionPage>(reader);
    case MessageKind::reject: {
        const auto code = reader.integer<std::uint16_t>();
        const auto field = reader.integer<std::uint8_t>();
        const auto reserved = reader.integer<std::uint8_t>();
        Rejection result{static_cast<RejectCode>(code), field, reader.integer<std::uint32_t>(),
                         reader.integer<std::uint32_t>()};
        if (code < static_cast<std::uint16_t>(RejectCode::malformedFrame) ||
            code > static_cast<std::uint16_t>(RejectCode::budgetExceeded) ||
            field > capabilityCount || reserved != 0) {
            return std::unexpected{Rejection{RejectCode::malformedFrame}};
        }
        return RejectBody{result};
    }
    case MessageKind::applicationContract:
        return DigestBody<MessageKind::applicationContract>{reader.identity<Digest>()};
    case MessageKind::dataContract:
        return DigestBody<MessageKind::dataContract>{reader.identity<Digest>()};
    case MessageKind::catalogIdentity:
        return DigestBody<MessageKind::catalogIdentity>{reader.identity<Digest>()};
    case MessageKind::offerEnd:
        return DigestBody<MessageKind::offerEnd>{reader.identity<Digest>()};
    case MessageKind::offerReference:
        return DigestBody<MessageKind::offerReference>{reader.identity<Digest>()};
    case MessageKind::clientCapabilitiesEnd:
        return DigestBody<MessageKind::clientCapabilitiesEnd>{reader.identity<Digest>()};
    case MessageKind::selectionEnd:
        return DigestBody<MessageKind::selectionEnd>{reader.identity<Digest>()};
    case MessageKind::clientReady:
        return DigestBody<MessageKind::clientReady>{reader.identity<Digest>()};
    case MessageKind::serverCommit:
        return DigestBody<MessageKind::serverCommit>{reader.identity<Digest>()};
    case MessageKind::clientCommitAck:
        return DigestBody<MessageKind::clientCommitAck>{reader.identity<Digest>()};
    default:
        return std::unexpected{Rejection{RejectCode::unexpectedMessage}};
    }
}
} // namespace

auto messageKind(const FramePayload &payload) -> MessageKind {
    return std::visit([](const auto &body) { return std::remove_cvref_t<decltype(body)>::kind; },
                      payload);
}

auto encodeFrame(const Frame &frame) -> std::expected<EncodedFrame, Rejection> {
    if (frame.nonce.isZero()) {
        return std::unexpected{Rejection{RejectCode::malformedFrame}};
    }
    if (frame.sequence.value >= maxFramesPerDirection) {
        return std::unexpected{Rejection{RejectCode::unexpectedSequence}};
    }
    EncodedFrame result;
    const auto kind = messageKind(frame.payload);
    const auto bodySize = expectedBodyBytes(kind);
    result.length = frameHeaderBytes + bodySize;
    Writer writer{result.storage};
    writer.bytes(wireMagic);
    writer.integer(wireMajor);
    writer.integer(static_cast<std::uint8_t>(kind));
    writer.integer(static_cast<std::uint16_t>(bodySize));
    writer.bytes(frame.nonce.bytes);
    writer.integer(frame.sequence.value);
    const auto valid = std::visit(
        [&writer](const auto &body) -> bool {
            using T = std::remove_cvref_t<decltype(body)>;
            if constexpr (std::same_as<T, ServerOfferBody> || std::same_as<T, ClientHelloBody>) {
                if (!validPolicy(body.policy) || !validTimeout(body.timeout)) {
                    return false;
                }
                writer.integer(wireMinor);
                writer.integer(static_cast<std::uint8_t>(body.policy));
                writer.integer(std::uint8_t{});
                if constexpr (std::same_as<T, ServerOfferBody>) {
                    if (body.epoch.value == 0 || body.dataContract.value == 0) {
                        return false;
                    }
                    writer.integer(body.spawn.value);
                    writer.integer(body.epoch.value);
                    writer.integer(body.timeout.value);
                    writer.integer(body.dataContract.value);
                } else {
                    if (body.nonce.isZero()) {
                        return false;
                    }
                    writer.bytes(body.nonce.bytes);
                    writer.integer(body.timeout.value);
                }
            } else if constexpr (requires { body.ordinal; }) {
                if (body.ordinal.value >= pageCount) {
                    return false;
                }
                for (std::size_t index = 0; index < limitsPerPage; ++index) {
                    writer.integer(
                        static_cast<std::uint8_t>(body.ordinal.value * limitsPerPage + index + 1));
                    writer.integer(std::uint8_t{});
                    writer.integer(std::uint16_t{});
                    writer.integer(body.wireValues[index]);
                }
            } else if constexpr (std::same_as<T, RejectBody>) {
                if (body.value.code < RejectCode::malformedFrame ||
                    body.value.code > RejectCode::budgetExceeded ||
                    body.value.fieldId > capabilityCount) {
                    return false;
                }
                writer.integer(static_cast<std::uint16_t>(body.value.code));
                writer.integer(body.value.fieldId);
                writer.integer(std::uint8_t{});
                writer.integer(body.value.offered);
                writer.integer(body.value.required);
            } else {
                writer.bytes(body.value.bytes);
            }
            return true;
        },
        frame.payload);
    if (!valid) {
        return std::unexpected{Rejection{RejectCode::malformedFrame}};
    }
    return result;
}

auto decodeFrame(std::span<const std::byte> bytes) noexcept -> std::expected<Frame, Rejection> {
    if (bytes.size() < frameHeaderBytes || bytes.size() > maxFrameBytes ||
        !std::ranges::equal(bytes.first(wireMagic.size()), wireMagic)) {
        return std::unexpected{Rejection{RejectCode::malformedFrame}};
    }
    Reader header{bytes.subspan(wireMagic.size())};
    if (header.integer<std::uint8_t>() != wireMajor) {
        return std::unexpected{Rejection{RejectCode::unsupportedVersion}};
    }
    const auto kindValue = header.integer<std::uint8_t>();
    if (kindValue < static_cast<std::uint8_t>(MessageKind::serverOffer) ||
        kindValue > static_cast<std::uint8_t>(MessageKind::catalogIdentity)) {
        return std::unexpected{Rejection{RejectCode::unexpectedMessage}};
    }
    const auto kind = static_cast<MessageKind>(kindValue);
    const auto bodySize = header.integer<std::uint16_t>();
    if (bodySize != expectedBodyBytes(kind) || bytes.size() != frameHeaderBytes + bodySize) {
        return std::unexpected{Rejection{RejectCode::malformedFrame}};
    }
    const auto nonce = header.identity<SessionNonce>();
    const FrameSequence sequence{header.integer<std::uint32_t>()};
    if (nonce.isZero()) {
        return std::unexpected{Rejection{RejectCode::malformedFrame}};
    }
    if (sequence.value >= maxFramesPerDirection) {
        return std::unexpected{Rejection{RejectCode::unexpectedSequence}};
    }
    Reader body{bytes.subspan(frameHeaderBytes)};
    auto payload = readPayload(kind, body);
    if (!payload) {
        return std::unexpected{payload.error()};
    }
    return Frame{nonce, sequence, *payload};
}

auto encodeClientCommand(const EncodedFrame &frame) -> std::string {
    return std::string{commandPrefix} + encodeHex(frame.bytes());
}
auto decodeClientCommand(std::string_view command) -> std::expected<Frame, Rejection> {
    constexpr auto maxCommandBytes = commandPrefix.size() + maxFrameBytes * 2;
    if (command.size() > maxCommandBytes || !command.starts_with(commandPrefix)) {
        return std::unexpected{Rejection{RejectCode::malformedFrame}};
    }
    const auto decoded = decodeHex(command.substr(commandPrefix.size()));
    if (!decoded) {
        return std::unexpected{decoded.error()};
    }
    return decodeFrame(*decoded);
}
} // namespace extendedPrecache::session
