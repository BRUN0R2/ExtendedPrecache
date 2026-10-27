#include "binary.hpp"
#include <extendedPrecache/data/modMessages.hpp>

namespace extendedPrecache::data {
namespace {
using namespace detail;
auto definition(ModuleId owner, ModMessageId id, ModContext context, WireMode mode)
    -> const MessageDefinition & {
    require(context.phase == Phase::gameplay, ErrorCode::wrongPhase);
    const auto *message = context.application.message(owner, id);
    require(message && message->direction == context.direction &&
                message->deliveries.allows(context.delivery) && message->wireMode == mode,
            ErrorCode::unsupportedModule);
    return *message;
}
auto typedSchema(const ModMessage &message, ModContext context) -> const Schema & {
    const auto &declared = definition(message.owner, message.id, context, WireMode::typed2);
    require(declared.schema == message.schema, ErrorCode::invalidSchema);
    const auto *schema = findSchema(context.application.definition().schemas, message.schema);
    require(schema && schema->role == SchemaRole::modMessage, ErrorCode::invalidSchema);
    return *schema;
}
void byteLimit(std::size_t bytes, ModContext context) {
    within(add(envelopeBytes, bytes), context.budget.messageBytes.value);
    if (context.delivery == Delivery::unreliable) {
        within(add(envelopeBytes, bytes),
               context.application.definition().limits.unreliableMessageBytes.value);
    }
}
} // namespace
auto encodeModMessage(const ModMessage &message, ModContext context) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        const auto &schema = typedSchema(message, context);
        const auto values =
            get(encodeValues(schema, message.values, context.values, context.budget));
        byteLimit(add(modMessageHeaderBytes, values.size()), context);
        Writer writer{context.budget.ownedBytes.value};
        writer.integer(message.owner.value);
        writer.integer(message.id.value);
        writer.integer(message.schema.value);
        writer.zero(2);
        writer.bytes(values);
        return std::move(writer).finish();
    });
}
auto decodeModMessage(std::span<const std::byte> bytes, ModContext context) noexcept
    -> Result<ModMessage> {
    return detail::protect([&] {
        byteLimit(bytes.size(), context);
        Reader reader{bytes};
        ModMessage message{};
        message.owner = {reader.integer<std::uint16_t>()};
        message.id = {reader.integer<std::uint16_t>()};
        message.schema = {reader.integer<std::uint16_t>()};
        reader.zero(2);
        const auto &schema = typedSchema(message, context);
        message.values = get(
            decodeValues(schema, reader.take(reader.remaining()), context.values, context.budget));
        reader.end();
        return message;
    });
}
auto validateNativeMessage(ModuleId module, ModMessageId message,
                           std::span<const std::byte> payload, ModContext context,
                           const NativeMessageValidator &validator) noexcept -> Result<void> {
    return detail::protect([&] {
        const auto &declared = definition(module, message, context, WireMode::nativeExact);
        within(payload.size(), nativeUserMessageBytes);
        within(payload.size(), context.budget.messageBytes.value);
        require(declared.nativeSize < 0 ||
                payload.size() == static_cast<std::size_t>(declared.nativeSize));
        get(validator.validate(module, declared, payload, context.values.resources));
    });
}
} // namespace extendedPrecache::data
