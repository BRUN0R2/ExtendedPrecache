#include "binary.hpp"
#include <extendedPrecache/data/media.hpp>

namespace extendedPrecache::data {
namespace {
using namespace detail;
void soundValid(const Sound &sound, Route route, Phase phase, SoundContext context) {
    require(validRoute(route) && route.direction == Direction::serverToClient && route.kind == 1 &&
            (route.opcode == static_cast<std::uint8_t>(Service::sound) ||
             route.opcode == static_cast<std::uint8_t>(Service::staticSound)));
    require(phase == Phase::gameplay, ErrorCode::wrongPhase);
    require(context.entityEntries > 0 && context.entityEntries <= maxEntityEntries &&
                sound.entity.value < context.entityEntries && sound.channel <= 7,
            ErrorCode::capacity);
    require(sound.action >= SoundAction::start && sound.action <= SoundAction::update);
    if (sound.action == SoundAction::stop) {
        require(std::holds_alternative<std::monostate>(sound.source) && sound.volume == 0 &&
                sound.attenuationQ6 == 0 && sound.pitch == 0 && !sound.spawning &&
                std::ranges::all_of(sound.origin, [](Coord value) { return value.value == 0; }));
    } else if (const auto *reference = std::get_if<ResourceReference>(&sound.source)) {
        require(reference->kind == ResourceKind::sound);
        get(context.resources.validate(*reference, false));
    } else {
        const auto *sentence = std::get_if<SentenceId>(&sound.source);
        require(sentence && sentence->value != 0 && context.bindings.sentenceExists(*sentence),
                ErrorCode::capacity);
    }
    if (sound.action == SoundAction::update) {
        require(context.bindings.canUpdate(sound.entity, sound.channel, sound.source),
                ErrorCode::capacity);
    }
    if (route.opcode == static_cast<std::uint8_t>(Service::staticSound)) {
        require(sound.action == SoundAction::start && sound.channel == 0 && sound.spawning &&
                route.delivery == Delivery::reliable);
    }
    within(envelopeBytes + soundBytes, context.budget.messageBytes.value);
}
auto eventSchema(const Event &event, Route route, Phase phase, EventContext context)
    -> const Schema & {
    require(validRoute(route) && route.direction == Direction::serverToClient && route.kind == 1 &&
            (route.opcode == static_cast<std::uint8_t>(Service::event) ||
             route.opcode == static_cast<std::uint8_t>(Service::reliableEvent)));
    require(phase == Phase::gameplay, ErrorCode::wrongPhase);
    require(context.entityEntries > 0 && context.entityEntries <= maxEntityEntries &&
                event.entity.value < context.entityEntries,
            ErrorCode::capacity);
    require((event.behaviorFlags & ~15U) == 0 && (event.behaviorFlags & 9U) != 9U);
    get(context.resources.validate({ResourceKind::event, event.id}, false));
    require(get(context.bindings.argumentSchema(event.id)) == event.argsSchema,
            ErrorCode::invalidSchema);
    const auto *schema = findSchema(context.application.definition().schemas, event.argsSchema);
    require(schema && schema->role == SchemaRole::eventArgs, ErrorCode::invalidSchema);
    return *schema;
}
} // namespace
auto encodeSound(const Sound &sound, Route route, Phase phase, SoundContext context) noexcept
    -> Result<Bytes> {
    return detail::protect([&] {
        soundValid(sound, route, phase, context);
        std::uint8_t kind{};
        std::uint32_t source{};
        if (const auto *reference = std::get_if<ResourceReference>(&sound.source)) {
            source = reference->id.value;
        }
        if (const auto *sentence = std::get_if<SentenceId>(&sound.source)) {
            kind = 1;
            source = sentence->value;
        }
        Writer writer{std::min<std::uint64_t>(soundBytes, context.budget.ownedBytes.value)};
        writer.integer(sound.entity.value);
        writer.integer(sound.channel);
        writer.enumeration(sound.action);
        writer.integer(kind);
        writer.integer(sound.volume);
        writer.integer(sound.attenuationQ6);
        writer.integer(sound.pitch);
        writer.integer(source);
        for (const auto coord : sound.origin) {
            writer.integer(std::bit_cast<std::uint16_t>(coord.value));
        }
        writer.integer(static_cast<std::uint16_t>(sound.spawning));
        return std::move(writer).finish();
    });
}
auto decodeSound(std::span<const std::byte> bytes, Route route, Phase phase,
                 SoundContext context) noexcept -> Result<Sound> {
    return detail::protect([&] {
        Reader reader{bytes};
        Sound sound{};
        sound.entity = {reader.integer<std::uint16_t>()};
        sound.channel = reader.integer<std::uint8_t>();
        sound.action = reader.enumeration<SoundAction>();
        const auto kind = reader.integer<std::uint8_t>();
        sound.volume = reader.integer<std::uint8_t>();
        sound.attenuationQ6 = reader.integer<std::uint8_t>();
        sound.pitch = reader.integer<std::uint8_t>();
        const auto source = reader.integer<std::uint32_t>();
        require(kind <= 1);
        if (sound.action == SoundAction::stop) {
            require(kind == 0 && source == 0);
        } else if (kind == 0) {
            sound.source = ResourceReference{ResourceKind::sound, {source}};
        } else {
            sound.source = SentenceId{source};
        }
        for (auto &coord : sound.origin) {
            coord = {std::bit_cast<std::int16_t>(reader.integer<std::uint16_t>())};
        }
        const auto flags = reader.integer<std::uint16_t>();
        require(flags <= 1);
        sound.spawning = flags == 1;
        reader.end();
        soundValid(sound, route, phase, context);
        return sound;
    });
}
auto encodeEvent(const Event &event, Route route, Phase phase, EventContext context) noexcept
    -> Result<Bytes> {
    return detail::protect([&] {
        const auto &schema = eventSchema(event, route, phase, context);
        const auto args = get(encodeValues(
            schema, event.args, {context.resources, context.entityEntries}, context.budget));
        within(add(envelopeBytes + eventHeaderBytes, args.size()),
               context.budget.messageBytes.value);
        if (route.delivery == Delivery::unreliable) {
            within(add(envelopeBytes + eventHeaderBytes, args.size()),
                   context.application.definition().limits.unreliableMessageBytes.value);
        }
        Writer writer{context.budget.ownedBytes.value};
        writer.integer(event.id.value);
        writer.integer(event.entity.value);
        writer.integer(event.behaviorFlags);
        writer.integer(event.delayTicks);
        writer.integer(event.argsSchema.value);
        writer.integer(narrow(args.size()));
        writer.bytes(args);
        return std::move(writer).finish();
    });
}
auto decodeEvent(std::span<const std::byte> bytes, Route route, Phase phase,
                 EventContext context) noexcept -> Result<Event> {
    return detail::protect([&] {
        within(add(envelopeBytes, bytes.size()), context.budget.messageBytes.value);
        if (route.delivery == Delivery::unreliable) {
            within(add(envelopeBytes, bytes.size()),
                   context.application.definition().limits.unreliableMessageBytes.value);
        }
        Reader reader{bytes};
        Event event{};
        event.id = {reader.integer<std::uint32_t>()};
        event.entity = {reader.integer<std::uint16_t>()};
        event.behaviorFlags = reader.integer<std::uint16_t>();
        event.delayTicks = reader.integer<std::uint16_t>();
        event.argsSchema = {reader.integer<std::uint16_t>()};
        const auto length = reader.integer<std::uint32_t>();
        const auto args = reader.take(length);
        reader.end();
        const auto &schema = eventSchema(event, route, phase, context);
        event.args = get(
            decodeValues(schema, args, {context.resources, context.entityEntries}, context.budget));
        return event;
    });
}
auto encodeDecalName(const DecalName &decal, const Catalog &catalog, RecordBudget budget) noexcept
    -> Result<Bytes> {
    return detail::protect([&] {
        const auto *resource = catalog.resolve({ResourceKind::decal, decal.id});
        require(resource && resource->name == decal.name, ErrorCode::capacity);
        within(decal.name.size(), maxDecalNameBytes);
        within(add(envelopeBytes + decalNameHeaderBytes, decal.name.size()),
               budget.messageBytes.value);
        Writer writer{budget.ownedBytes.value};
        writer.integer(decal.id.value);
        writer.integer(small(decal.name.size()));
        writer.text(decal.name);
        return std::move(writer).finish();
    });
}
auto decodeDecalName(std::span<const std::byte> bytes, const Catalog &catalog,
                     RecordBudget budget) noexcept -> Result<DecalName> {
    return detail::protect([&] {
        Reader reader{bytes};
        const auto id = DomainId{reader.integer<std::uint32_t>()};
        const auto count = reader.integer<std::uint16_t>();
        DecalName result{id, reader.text(count, maxDecalNameBytes)};
        reader.end();
        get(encodeDecalName(result, catalog, budget));
        return result;
    });
}
} // namespace extendedPrecache::data
