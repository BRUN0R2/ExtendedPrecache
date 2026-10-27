#include "binary.hpp"
#include <extendedPrecache/data/readiness.hpp>

namespace extendedPrecache::data {
auto encodeAssetsReady(AssetsReady ready) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        detail::require(!ready.catalogHash.isZero() && !ready.schemaHash.isZero() &&
                        !ready.reportHash.isZero());
        detail::Writer writer{assetsReadyBytes};
        writer.bytes(ready.catalogHash.bytes);
        writer.bytes(ready.schemaHash.bytes);
        writer.bytes(ready.reportHash.bytes);
        return std::move(writer).finish();
    });
}
auto decodeAssetsReady(std::span<const std::byte> bytes) noexcept -> Result<AssetsReady> {
    return detail::protect([&] {
        detail::Reader reader{bytes};
        const AssetsReady ready{reader.identity<Digest>(), reader.identity<Digest>(),
                                reader.identity<Digest>()};
        reader.end();
        detail::get(encodeAssetsReady(ready));
        return ready;
    });
}
auto encodeGameStart(GameStart start) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        detail::require(!start.catalogHash.isZero());
        detail::Writer writer{gameStartBytes};
        writer.bytes(detail::get(encodeFrameAck(start.initial)));
        writer.bytes(start.catalogHash.bytes);
        return std::move(writer).finish();
    });
}
auto decodeGameStart(std::span<const std::byte> bytes) noexcept -> Result<GameStart> {
    return detail::protect([&] {
        detail::Reader reader{bytes};
        const auto ack = detail::get(decodeFrameAck(reader.take(frameAckBytes)));
        const auto catalog = reader.identity<Digest>();
        reader.end();
        detail::require(!catalog.isZero());
        return GameStart{ack, catalog};
    });
}
auto encodeResyncRequest(StateFrameId id) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        detail::require(id.value <= maxSequence);
        detail::Writer writer{4};
        writer.integer(id.value);
        return std::move(writer).finish();
    });
}
auto decodeResyncRequest(std::span<const std::byte> bytes) noexcept -> Result<StateFrameId> {
    return detail::protect([&] {
        detail::Reader reader{bytes};
        const auto id = StateFrameId{reader.integer<std::uint32_t>()};
        reader.end();
        detail::require(id.value <= maxSequence);
        return id;
    });
}
auto encodeDigest(Digest digest) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        detail::require(!digest.isZero());
        return Bytes{digest.bytes.begin(), digest.bytes.end()};
    });
}
auto decodeDigest(std::span<const std::byte> bytes) noexcept -> Result<Digest> {
    return detail::protect([&] {
        detail::Reader reader{bytes};
        const auto digest = reader.identity<Digest>();
        reader.end();
        detail::require(!digest.isZero());
        return digest;
    });
}
} // namespace extendedPrecache::data
