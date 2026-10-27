#include "binary.hpp"
#include <extendedPrecache/data/consistency.hpp>

namespace extendedPrecache::data {
namespace {
using namespace detail;
auto entryBytes(const ConsistencyEntry &entry) -> std::uint32_t {
    require(entry.mode >= ConsistencyMode::exactDigest &&
            entry.mode <= ConsistencyMode::withinBounds);
    require(entry.bounds.has_value() == (entry.mode != ConsistencyMode::exactDigest) &&
            !entry.actualAsset.isZero());
    if (entry.bounds) {
        for (std::size_t index = 0; index < 3; ++index) {
            validFloat(entry.bounds->mins[index]);
            validFloat(entry.bounds->maxs[index]);
            require(entry.bounds->mins[index] <= entry.bounds->maxs[index]);
        }
    }
    return entry.bounds ? consistencyBoundsEntryBytes : consistencyDigestEntryBytes;
}
void writeEntry(Writer &writer, const ConsistencyEntry &entry) {
    entryBytes(entry);
    writer.integer(entry.resource.value);
    writer.enumeration(entry.mode);
    writer.zero(3);
    writer.bytes(entry.actualAsset.bytes);
    if (entry.bounds) {
        for (const auto value : entry.bounds->mins) {
            writer.f32(value);
        }
        for (const auto value : entry.bounds->maxs) {
            writer.f32(value);
        }
    }
}
auto readEntry(Reader &reader) -> ConsistencyEntry {
    ConsistencyEntry entry{};
    entry.resource = {reader.integer<std::uint32_t>()};
    entry.mode = reader.enumeration<ConsistencyMode>();
    reader.zero(3);
    entry.actualAsset = reader.identity<Digest>();
    if (entry.mode == ConsistencyMode::exactBounds || entry.mode == ConsistencyMode::withinBounds) {
        Bounds value{};
        for (auto &item : value.mins) {
            item = reader.f32();
        }
        for (auto &item : value.maxs) {
            item = reader.f32();
        }
        entry.bounds = value;
    }
    entryBytes(entry);
    return entry;
}
} // namespace
auto fitConsistencyEntries(std::span<const ConsistencyEntry> entries, RecordBudget budget) noexcept
    -> Result<std::size_t> {
    return detail::protect([&] {
        std::uint64_t total = envelopeBytes + consistencyPageHeaderBytes;
        within(total, budget.messageBytes.value);
        std::size_t count{};
        for (const auto &entry : entries) {
            const auto next = add(total, entryBytes(entry));
            if (next > budget.messageBytes.value) {
                break;
            }
            total = next;
            ++count;
        }
        return count;
    });
}
auto encodeConsistencyPage(const ConsistencyPage &page, RecordBudget budget) noexcept
    -> Result<Bytes> {
    return detail::protect([&] {
        require(!page.entries.empty());
        require(get(fitConsistencyEntries(page.entries, budget)) == page.entries.size(),
                ErrorCode::byteBudget);
        Writer writer{std::min<std::uint64_t>(budget.messageBytes.value - envelopeBytes,
                                              budget.ownedBytes.value)};
        writer.integer(page.ruleOffset);
        writer.integer(narrow(page.entries.size()));
        std::optional<ResourceOrdinal> previous;
        for (const auto &entry : page.entries) {
            require(!previous || previous->value < entry.resource.value);
            previous = entry.resource;
            writeEntry(writer, entry);
        }
        return std::move(writer).finish();
    });
}
auto decodeConsistencyPage(std::span<const std::byte> bytes, RecordBudget budget) noexcept
    -> Result<ConsistencyPage> {
    return detail::protect([&] {
        within(add(envelopeBytes, bytes.size()), budget.messageBytes.value);
        Reader reader{bytes};
        ConsistencyPage page{};
        page.ruleOffset = reader.integer<std::uint32_t>();
        const auto count = reader.integer<std::uint32_t>();
        require(count > 0);
        reader.count(count, consistencyDigestEntryBytes);
        within(multiply(count, sizeof(ConsistencyEntry)), budget.ownedBytes.value);
        page.entries.reserve(count);
        std::optional<ResourceOrdinal> previous;
        for (std::uint32_t index = 0; index < count; ++index) {
            auto entry = readEntry(reader);
            require(!previous || previous->value < entry.resource.value);
            previous = entry.resource;
            page.entries.push_back(entry);
        }
        reader.end();
        return page;
    });
}
auto encodeConsistencyEnd(ConsistencyEnd end) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        require(!end.reportHash.isZero());
        Writer writer{consistencyEndBytes};
        writer.integer(end.ruleCount);
        writer.bytes(end.reportHash.bytes);
        return std::move(writer).finish();
    });
}
auto decodeConsistencyEnd(std::span<const std::byte> bytes) noexcept -> Result<ConsistencyEnd> {
    return detail::protect([&] {
        Reader reader{bytes};
        const ConsistencyEnd result{reader.integer<std::uint32_t>(), reader.identity<Digest>()};
        reader.end();
        require(!result.reportHash.isZero());
        return result;
    });
}
auto consistencyHash(std::span<const ConsistencyEntry> entries, Digest catalogHash,
                     const DigestProvider &provider, MetadataBytes memory) noexcept
    -> Result<Digest> {
    return detail::protect([&] {
        require(!catalogHash.isZero());
        Writer writer{memory.value};
        constexpr char label[]{"ExtendedPrecache/consistency/2"};
        writer.bytes(std::as_bytes(std::span{label}));
        writer.bytes(catalogHash.bytes);
        writer.integer(narrow(entries.size()));
        for (const auto &entry : entries) {
            writeEntry(writer, entry);
        }
        return hash(writer.view(), provider);
    });
}
auto ConsistencyReport::append(const ConsistencyPage &page) noexcept -> Result<void> {
    auto result = detail::protect([&] {
        require(!terminal, ErrorCode::wrongPhase);
        require(page.ruleOffset == entries.size() && !page.entries.empty(),
                ErrorCode::consistencyFailure);
        within(add(entries.size(), page.entries.size()), catalogValue.data().rules.size());
        within(multiply(add(entries.size(), page.entries.size()), sizeof(ConsistencyEntry)),
               memoryLimit.value);
        for (std::size_t index = 0; index < page.entries.size(); ++index) {
            const auto &entry = page.entries[index];
            const auto &rule = catalogValue.data().rules[entries.size() + index];
            entryBytes(entry);
            require(entry.resource == rule.resource && entry.mode == rule.mode,
                    ErrorCode::consistencyFailure);
            const auto &resource = catalogValue.data().resources[rule.resource.value];
            require(entry.actualAsset == catalogValue.data().assets[resource.asset.value].digest,
                    ErrorCode::consistencyFailure);
            if (rule.bounds) {
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    const auto &actual = *entry.bounds;
                    const auto &expected = *rule.bounds;
                    if (rule.mode == ConsistencyMode::exactBounds) {
                        require(actual.mins[axis] == expected.mins[axis] &&
                                    actual.maxs[axis] == expected.maxs[axis],
                                ErrorCode::consistencyFailure);
                    } else {
                        require(actual.mins[axis] >= expected.mins[axis] &&
                                    actual.maxs[axis] <= expected.maxs[axis],
                                ErrorCode::consistencyFailure);
                    }
                }
            }
        }
        entries.insert(entries.end(), page.entries.begin(), page.entries.end());
    });
    if (!result) {
        terminal = true;
    }
    return result;
}
auto ConsistencyReport::finish(ConsistencyEnd end, const DigestProvider &provider) noexcept
    -> Result<Digest> {
    auto result = detail::protect([&] {
        require(!terminal && entries.size() == catalogValue.data().rules.size() &&
                    end.ruleCount == entries.size(),
                ErrorCode::consistencyFailure);
        const auto identity =
            get(consistencyHash(entries, catalogValue.digest(), provider, memoryLimit));
        require(identity == end.reportHash, ErrorCode::digestMismatch);
        return identity;
    });
    terminal = true;
    return result;
}
} // namespace extendedPrecache::data
