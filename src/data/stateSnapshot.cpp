#include "stateIo.hpp"

namespace extendedPrecache::data {
namespace {
using namespace detail;
template <typename Function>
void mergedRows(std::span<const FullObject> base, std::span<const FrameObject> changes,
                Function action) {
    std::size_t oldIndex{};
    std::size_t changeIndex{};
    while (oldIndex < base.size() || changeIndex < changes.size()) {
        const auto *old = oldIndex < base.size() ? &base[oldIndex] : nullptr;
        const auto *change = changeIndex < changes.size() ? &changes[changeIndex] : nullptr;
        if (!change || (old && old->key < change->key)) {
            action(old, nullptr);
            ++oldIndex;
        } else if (!old || change->key < old->key) {
            require(change->operation == Operation::replace);
            action(nullptr, change);
            ++changeIndex;
        } else {
            if (change->operation == Operation::remove) {
                require(change->schema == old->schema);
            } else {
                action(old, change);
            }
            ++oldIndex;
            ++changeIndex;
        }
    }
}
template <typename Function>
void selectedFields(const FullObject *old, const FrameObject *change, Function action) {
    if (!change) {
        require(old != nullptr);
        for (std::size_t index = 0; index < old->values.size(); ++index) {
            action(index, old->values[index]);
        }
        return;
    }
    const auto patch = change->operation == Operation::patch;
    require(!patch ||
            (old && old->schema == change->schema && old->values.size() == change->fields.size()));
    for (std::size_t index = 0; index < change->fields.size(); ++index) {
        const auto &field = change->fields[index];
        if (field) {
            action(index, *field);
        } else {
            require(patch);
            action(index, old->values[index]);
        }
    }
}
auto fieldWireBytes(const FieldDefinition &definition, const FieldValue &value) -> std::uint64_t {
    require(definition.has(FieldFlag::array) ? value.elements.size() <= definition.maxElements
                                             : value.elements.size() == 1);
    auto bytes = definition.has(FieldFlag::array) ? std::uint64_t{2} : std::uint64_t{};
    for (const auto &atom : value.elements) {
        if (definition.kind == ValueKind::bytes) {
            const auto *text = std::get_if<ByteString>(&atom);
            require(text != nullptr, ErrorCode::invalidSchema);
            within(text->bytes.size(), definition.maxAtomBytes);
            bytes = add(bytes, add(2, text->bytes.size()));
        } else {
            bytes = add(bytes, definition.maxAtomBytes);
        }
    }
    return bytes;
}
} // namespace
namespace detail {
auto canonicalObjects(std::span<const FullObject> objects, const StateContext &context) -> Bytes {
    Writer writer{context.frameBytes.value};
    std::optional<ObjectKey> previous;
    for (const auto &object : objects) {
        require(!previous || *previous < object.key);
        previous = object.key;
        const auto &schema = objectSchema(object.key, object.schema, context);
        const auto values =
            get(encodeValues(schema, object.values,
                             {context.catalog, context.objects.entityEntries}, context.records));
        writer.enumeration(object.key.kind);
        writer.zero(1);
        writer.integer(object.schema.value);
        writer.integer(object.key.id.value);
        writer.integer(narrow(values.size()));
        writer.bytes(values);
    }
    return std::move(writer).finish();
}
auto stateHash(FrameBegin begin, std::span<const FullObject> objects, const StateContext &context,
               const DigestProvider &provider) -> Digest {
    require(context.epoch.value != 0);
    const auto canonical = canonicalObjects(objects, context);
    Writer writer{context.records.ownedBytes.value};
    constexpr char label[]{"ExtendedPrecache/state/2"};
    writer.bytes(std::as_bytes(std::span{label}));
    writer.integer(context.epoch.value);
    writer.bytes(context.catalog.digest().bytes);
    writer.bytes(context.catalog.data().schemaSetHash.bytes);
    writer.bytes(get(encodeFrameBegin(begin)));
    writer.integer(narrow(objects.size()));
    writer.bytes(canonical);
    return hash(writer.view(), provider);
}
} // namespace detail
auto reconstructState(FrameBegin begin, std::span<const FrameObject> changes,
                      const StateSnapshot *base, const StateContext &context,
                      const DigestProvider &provider) noexcept -> Result<StateSnapshot> {
    return detail::protect([&] {
        using namespace detail;
        validateFrameBegin(begin);
        begin.serverTime = canonicalFloat(begin.serverTime);
        require((begin.base.value != 0) == (base != nullptr));
        require(changes.size() == begin.changeCount);
        within(begin.changeBytes.value, context.frameBytes.value);
        if (base) {
            require(base->begin.id == begin.base && base->epoch == context.epoch &&
                        base->catalogHash == context.catalog.digest() &&
                        base->schemaHash == context.catalog.data().schemaSetHash,
                    ErrorCode::staleContext);
            require(begin.serverTime >= base->begin.serverTime);
        }
        std::uint64_t byteCount{};
        std::optional<ObjectKey> previous;
        for (const auto &change : changes) {
            require(change.frame == begin.id && (!previous || *previous < change.key));
            previous = change.key;
            require(base || change.operation == Operation::replace);
            byteCount = add(byteCount, get(encodeFrameObject(change, context)).size());
        }
        require(byteCount == begin.changeBytes.value);
        const auto oldRows =
            base ? std::span<const FullObject>{base->objects} : std::span<const FullObject>{};
        std::uint64_t plannedOwned = sizeof(StateSnapshot);
        std::uint64_t plannedWire{};
        std::size_t objectCount{};
        mergedRows(oldRows, changes, [&](const FullObject *old, const FrameObject *change) {
            if (!old && !change) {
                fail(ErrorCode::invalidSchema);
            }
            const auto key = change ? change->key : old->key;
            const auto id = change ? change->schema : old->schema;
            const auto &schema = objectSchema(key, id, context);
            plannedOwned =
                add(plannedOwned,
                    add(sizeof(FullObject), multiply(schema.fields.size(), sizeof(FieldValue))));
            plannedWire = add(plannedWire, fullObjectHeaderBytes);
            std::size_t fields{};
            selectedFields(old, change, [&](std::size_t index, const FieldValue &value) {
                require(index < schema.fields.size(), ErrorCode::invalidSchema);
                plannedOwned = add(plannedOwned, ownedFieldBytes(value));
                plannedWire = add(plannedWire, fieldWireBytes(schema.fields[index], value));
                ++fields;
            });
            require(fields == schema.fields.size(), ErrorCode::invalidSchema);
            ++objectCount;
        });
        within(plannedOwned, context.records.ownedBytes.value);
        within(plannedWire, context.frameBytes.value);
        std::vector<FullObject> objects;
        objects.reserve(objectCount);
        mergedRows(oldRows, changes, [&](const FullObject *old, const FrameObject *change) {
            if (!old && !change) {
                fail(ErrorCode::invalidSchema);
            }
            FullObject object{
                change ? change->key : old->key, change ? change->schema : old->schema, {}};
            const auto *schema = findSchema(context.schemas, object.schema);
            require(schema != nullptr, ErrorCode::invalidSchema);
            object.values.reserve(schema->fields.size());
            selectedFields(old, change, [&](std::size_t, const FieldValue &value) {
                object.values.push_back(value);
            });
            canonicalizeValues(object.values);
            objects.push_back(std::move(object));
        });
        const auto has = [&](ObjectKey key) {
            const auto found = std::ranges::lower_bound(objects, key, {}, &FullObject::key);
            return found != objects.end() && found->key == key;
        };
        require(has({ObjectKind::entity, {0}}));
        for (const auto key : context.bindings.mandatoryObjects()) {
            require(has(key), ErrorCode::invalidSchema);
        }
        const auto identity = stateHash(begin, objects, context, provider);
        return StateSnapshot{begin,
                             context.epoch,
                             context.catalog.digest(),
                             context.catalog.data().schemaSetHash,
                             identity,
                             std::move(objects)};
    });
}
} // namespace extendedPrecache::data
