#pragma once
#include "valueIo.hpp"
#include <extendedPrecache/data/state.hpp>

namespace extendedPrecache::data::detail {
auto objectSchema(ObjectKey key, SchemaId id, const StateContext &context) -> const Schema &;
void validateFrameBegin(FrameBegin begin);
auto canonicalObjects(std::span<const FullObject> objects, const StateContext &context) -> Bytes;
auto stateHash(FrameBegin begin, std::span<const FullObject> objects, const StateContext &context,
               const DigestProvider &provider) -> Digest;
} // namespace extendedPrecache::data::detail
