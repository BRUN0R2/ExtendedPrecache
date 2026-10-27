#pragma once
#include <extendedPrecache/data/state.hpp>

namespace extendedPrecache::data {
struct AssetsReady final {
    Digest catalogHash;
    Digest schemaHash;
    Digest reportHash;
    auto operator==(const AssetsReady &) const -> bool = default;
};
struct GameStart final {
    FrameAck initial;
    Digest catalogHash;
    auto operator==(const GameStart &) const -> bool = default;
};
[[nodiscard]] auto encodeAssetsReady(AssetsReady ready) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeAssetsReady(std::span<const std::byte> bytes) noexcept
    -> Result<AssetsReady>;
[[nodiscard]] auto encodeGameStart(GameStart start) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeGameStart(std::span<const std::byte> bytes) noexcept -> Result<GameStart>;
[[nodiscard]] auto encodeResyncRequest(StateFrameId lastApplied) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeResyncRequest(std::span<const std::byte> bytes) noexcept
    -> Result<StateFrameId>;
[[nodiscard]] auto encodeDigest(Digest digest) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeDigest(std::span<const std::byte> bytes) noexcept -> Result<Digest>;
} // namespace extendedPrecache::data
