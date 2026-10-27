#include "binary.hpp"
#include <extendedPrecache/data/catalog.hpp>

namespace extendedPrecache::data {
auto encodeCatalogBegin(CatalogBegin begin) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        detail::require(begin.totalBytes.value >= catalogHeaderBytes && begin.resourceCount > 0 &&
                        begin.assetCount <= begin.resourceCount &&
                        begin.consistencyCount <= begin.resourceCount);
        detail::Writer writer{catalogBeginBytes};
        writer.integer(begin.totalBytes.value);
        writer.integer(begin.resourceCount);
        writer.integer(begin.assetCount);
        writer.integer(begin.consistencyCount);
        return std::move(writer).finish();
    });
}
auto decodeCatalogBegin(std::span<const std::byte> bytes) noexcept -> Result<CatalogBegin> {
    return detail::protect([&] {
        detail::Reader reader{bytes};
        const CatalogBegin result{{reader.integer<std::uint32_t>()},
                                  reader.integer<std::uint32_t>(),
                                  reader.integer<std::uint32_t>(),
                                  reader.integer<std::uint32_t>()};
        reader.end();
        detail::get(encodeCatalogBegin(result));
        return result;
    });
}
auto encodeChunk(ChunkView chunk, ByteCount maxPayload, RecordBudget budget) noexcept
    -> Result<Bytes> {
    return detail::protect([&] {
        detail::require(!chunk.bytes.empty());
        detail::within(chunk.bytes.size(), maxPayload.value);
        detail::within(detail::add(chunkOverheadBytes, chunk.bytes.size()),
                       budget.messageBytes.value);
        detail::within(detail::add(chunk.offset.value, chunk.bytes.size()), maxMetadataBytes);
        detail::Writer writer{budget.ownedBytes.value};
        writer.integer(chunk.offset.value);
        writer.integer(detail::narrow(chunk.bytes.size()));
        writer.bytes(chunk.bytes);
        return std::move(writer).finish();
    });
}
auto decodeChunk(std::span<const std::byte> bytes, ByteCount maxPayload,
                 RecordBudget budget) noexcept -> Result<ChunkView> {
    return detail::protect([&] {
        detail::Reader reader{bytes};
        const auto offset = ByteCount{reader.integer<std::uint32_t>()};
        const auto count = reader.integer<std::uint32_t>();
        const auto part = reader.take(count);
        reader.end();
        const ChunkView result{offset, part};
        detail::get(encodeChunk(result, maxPayload, budget));
        return result;
    });
}
auto ChunkAssembler::create(ByteCount total, ByteCount maximum, MetadataBytes memory) noexcept
    -> Result<ChunkAssembler> {
    return detail::protect([&] {
        detail::require(total.value > 0);
        detail::within(total.value, maximum.value);
        detail::within(total.value, memory.value);
        ChunkAssembler result{total};
        result.buffer.reserve(total.value);
        return result;
    });
}
auto ChunkAssembler::append(ChunkView chunk) noexcept -> Result<void> {
    auto result = detail::protect([&] {
        detail::require(!failed, ErrorCode::wrongPhase);
        detail::require(chunk.offset.value == buffer.size() && !chunk.bytes.empty());
        detail::within(detail::add(buffer.size(), chunk.bytes.size()), totalBytes.value);
        buffer.insert(buffer.end(), chunk.bytes.begin(), chunk.bytes.end());
    });
    if (!result) {
        failed = true;
    }
    return result;
}
auto ChunkAssembler::finish(Digest expected, const DigestProvider &provider) const noexcept
    -> Result<std::span<const std::byte>> {
    return detail::protect([&] {
        detail::require(!failed && buffer.size() == totalBytes.value, ErrorCode::wrongPhase);
        detail::require(!expected.isZero() && detail::hash(buffer, provider) == expected,
                        ErrorCode::digestMismatch);
        return std::span<const std::byte>{buffer};
    });
}
} // namespace extendedPrecache::data
