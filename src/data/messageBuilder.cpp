#include "binary.hpp"
#include <extendedPrecache/data/codec.hpp>

namespace extendedPrecache::data {
namespace {
auto logicalBudget(Delivery delivery, const CodecContext &context) -> ByteCount {
    auto maximum = std::min(
        {maxMessageBytes, context.budget.messageBytes.value,
         context.selected.vector().get<session::CapabilityField::reliableMessageBytes>().value});
    if (delivery == Delivery::unreliable) {
        maximum =
            std::min(maximum, context.application.definition().limits.unreliableMessageBytes.value);
    }
    return {static_cast<std::uint32_t>(
        std::min<std::uint64_t>(maximum, context.budget.ownedBytes.value))};
}
} // namespace
auto DataMessageBuilder::create(Direction direction, Delivery delivery, SessionNonce nonce,
                                const CodecContext &context) noexcept
    -> Result<DataMessageBuilder> {
    return detail::protect([&] {
        detail::require(
            (direction == Direction::serverToClient || direction == Direction::clientToServer) &&
            (delivery == Delivery::reliable || delivery == Delivery::unreliable) &&
            (direction != Direction::clientToServer || delivery == Delivery::reliable));
        detail::require(!nonce.isZero() && context.epoch.value != 0);
        const auto maximum = logicalBudget(delivery, context);
        detail::require(maximum.value >= envelopeBytes, ErrorCode::byteBudget);
        return DataMessageBuilder{direction, delivery, nonce, context.epoch, maximum};
    });
}
auto DataMessageBuilder::append(const DataPayload &payload, Envelope header,
                                const CodecContext &context) noexcept -> Result<void> {
    return detail::protect([&] {
        detail::require(header.route.direction == directionValue &&
                        header.route.delivery == deliveryValue);
        detail::require(header.nonce == nonceValue && header.epoch == epochValue &&
                            context.epoch == epochValue,
                        ErrorCode::staleContext);
        const auto encoded = detail::get(encodeMessage(payload, header, context));
        const auto maximum =
            std::min(maximumBytes.value, logicalBudget(deliveryValue, context).value);
        const auto total = detail::add(bytes.size(), encoded.size());
        detail::within(total, maximum);
        detail::within(total, context.budget.ownedBytes.value);
        // Allocate once for this bounded message; failed appends leave its prefix intact.
        if (bytes.capacity() < maximum) {
            bytes.reserve(maximum);
        }
        bytes.insert(bytes.end(), encoded.begin(), encoded.end());
    });
}
} // namespace extendedPrecache::data
