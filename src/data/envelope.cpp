#include "binary.hpp"
#include <extendedPrecache/data/envelope.hpp>

namespace extendedPrecache::data {
auto validRoute(Route route) noexcept -> bool {
    if (route.delivery != Delivery::reliable && route.delivery != Delivery::unreliable) {
        return false;
    }
    const auto reliable = route.delivery == Delivery::reliable;
    if (route.direction == Direction::clientToServer) {
        return route.opcode == 7 && reliable &&
               ((route.kind >= 1 && route.kind <= 7) || route.kind == 255);
    }
    if (route.direction != Direction::serverToClient) {
        return false;
    }
    switch (static_cast<Service>(route.opcode)) {
    case Service::schema:
        return reliable && (route.kind == 1 || route.kind == 2);
    case Service::resource:
        return ((route.kind >= 1 && route.kind <= 4) || route.kind == 255)
                   ? reliable
                   : (route.kind == 5 || route.kind == 6);
    case Service::initialFrame:
        return reliable && route.kind >= 1 && route.kind <= 3;
    case Service::fullFrame:
    case Service::deltaFrame:
        return route.kind >= 1 && route.kind <= 3;
    case Service::clientData:
        return route.kind == 2;
    case Service::event:
        return !reliable && route.kind == 1;
    case Service::reliableEvent:
    case Service::staticSound:
    case Service::decalName:
        return reliable && route.kind == 1;
    case Service::sound:
    case Service::effect:
        return route.kind == 1;
    }
    return false;
}
auto encodeEnvelope(const Envelope &value, RecordBudget budget) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        detail::require(validRoute(value.route) && !value.nonce.isZero() &&
                        value.epoch.value != 0 && value.sequence.value <= maxSequence);
        detail::within(detail::add(envelopeBytes, value.body.size()), budget.messageBytes.value);
        detail::within(detail::add(envelopeBytes, value.body.size()), maxMessageBytes);
        detail::Writer writer{
            std::min<std::uint64_t>(budget.messageBytes.value, budget.ownedBytes.value)};
        writer.integer(value.route.opcode);
        writer.integer(std::uint8_t{2});
        writer.integer(value.route.kind);
        writer.enumeration(value.route.delivery);
        writer.integer(detail::narrow(value.body.size()));
        writer.bytes(value.nonce.bytes);
        writer.integer(value.epoch.value);
        writer.integer(value.sequence.value);
        writer.zero(4);
        writer.bytes(value.body);
        return std::move(writer).finish();
    });
}
auto decodeEnvelope(std::span<const std::byte> bytes, Direction direction,
                    RecordBudget budget) noexcept -> Result<Envelope> {
    return detail::protect([&] {
        detail::inputBudget(bytes, budget);
        detail::within(bytes.size(), maxMessageBytes);
        detail::Reader reader{bytes};
        const auto opcode = reader.integer<std::uint8_t>();
        detail::require(reader.integer<std::uint8_t>() == 2, ErrorCode::unsupportedContract);
        const auto kind = reader.integer<std::uint8_t>();
        const auto delivery = reader.enumeration<Delivery>();
        const auto count = reader.integer<std::uint32_t>();
        const auto nonce = reader.identity<SessionNonce>();
        const auto epoch = MapEpoch{reader.integer<std::uint64_t>()};
        const auto sequence = session::FrameSequence{reader.integer<std::uint32_t>()};
        reader.zero(4);
        const Route route{direction, opcode, kind, delivery};
        detail::require(validRoute(route) && !nonce.isZero() && epoch.value != 0 &&
                        sequence.value <= maxSequence);
        const auto body = reader.take(count);
        reader.end();
        return Envelope{route, nonce, epoch, sequence, body};
    });
}
auto SequenceTracker::accept(const Envelope &value) noexcept -> Result<ReceiveDisposition> {
    return detail::protect([&] {
        detail::require(!nonceValue.isZero() && epochValue.value != 0 && validRoute(value.route) &&
                        !value.nonce.isZero() && value.sequence.value <= maxSequence);
        if (value.nonce != nonceValue) {
            return ReceiveDisposition::staleNonce;
        }
        detail::require(value.epoch == epochValue, ErrorCode::staleContext);
        const auto index =
            value.route.direction == Direction::clientToServer
                ? std::size_t{2}
                : (value.route.delivery == Delivery::reliable ? std::size_t{0} : std::size_t{1});
        auto &next = nextValues[index];
        if (index == 1 && value.sequence.value < next) {
            return ReceiveDisposition::oldUnreliable;
        }
        detail::require(next <= maxSequence && (index == 1 || next == value.sequence.value),
                        ErrorCode::sequence);
        next = value.sequence.value + 1;
        return ReceiveDisposition::accept;
    });
}
auto encodeFatal(Failure error) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        detail::require(error.code >= ErrorCode::malformed &&
                        error.code <= ErrorCode::storageUnavailable);
        detail::Writer writer{fatalBytes};
        writer.integer(static_cast<std::uint16_t>(error.code));
        writer.zero(2);
        writer.integer(error.subjectSequence);
        writer.integer(error.fieldId);
        writer.zero(4);
        return std::move(writer).finish();
    });
}
auto decodeFatal(std::span<const std::byte> bytes) noexcept -> Result<Failure> {
    return detail::protect([&] {
        detail::Reader reader{bytes};
        Failure result{};
        const auto code = reader.integer<std::uint16_t>();
        detail::require(code >= static_cast<std::uint16_t>(ErrorCode::malformed) &&
                        code <= static_cast<std::uint16_t>(ErrorCode::storageUnavailable));
        result.code = static_cast<ErrorCode>(code);
        reader.zero(2);
        result.subjectSequence = reader.integer<std::uint32_t>();
        result.fieldId = reader.integer<std::uint32_t>();
        reader.zero(4);
        reader.end();
        detail::require(result.code >= ErrorCode::malformed &&
                        result.code <= ErrorCode::storageUnavailable);
        return result;
    });
}
} // namespace extendedPrecache::data
