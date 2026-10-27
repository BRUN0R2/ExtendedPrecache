#include "binary.hpp"
#include <extendedPrecache/data/demo.hpp>

namespace extendedPrecache::data::detail {
struct DemoState final {
    DemoHeader header;
    ApplicationLimits limits;
    const DigestProvider &provider;
    DemoValidator &validator;
    Bytes headerBytes;
    std::unique_ptr<session::DigestStream> stream;
    std::optional<Digest> finishedHash{};
    Bytes metadataBytes{};
    std::uint32_t metadataTotal{};
    DemoKind nextKind{DemoKind::dataArtifact};
    std::uint64_t nextSequence{};
    std::uint64_t fileBytes{demoHeaderBytes};
    TimeMicros lastTime{};
    bool ended{};
    bool failed{};

    auto finishHash() -> Digest {
        if (!finishedHash) {
            const auto result = stream->finish();
            require(result.has_value(), ErrorCode::storageUnavailable);
            finishedHash = *result;
        }
        return *finishedHash;
    }
    auto artifactLimit(DemoKind kind) const -> std::uint32_t {
        if (kind == DemoKind::schemaSet) {
            return limits.schemaSetBytes.value;
        }
        if (kind == DemoKind::catalog) {
            return header.selected.get<session::CapabilityField::manifestBytes>().value;
        }
        return limits.contractArtifactBytes.value;
    }
    auto artifactHash(DemoKind kind) const -> Digest {
        switch (kind) {
        case DemoKind::dataArtifact:
            return header.dataHash;
        case DemoKind::applicationArtifact:
            return header.applicationHash;
        case DemoKind::schemaSet:
            return header.schemaHash;
        case DemoKind::catalog:
            return header.catalogHash;
        default:
            fail(ErrorCode::wrongPhase);
        }
    }
    void metadata(const DemoBlockView &block) {
        require(block.header.kind == nextKind && block.header.time.value == 0,
                ErrorCode::wrongPhase);
        Reader reader{block.payload};
        const auto total = reader.integer<std::uint32_t>();
        const auto offset = reader.integer<std::uint32_t>();
        const auto bytes = reader.take(reader.remaining());
        require(total > 0 && !bytes.empty());
        within(total, artifactLimit(nextKind));
        require(offset == metadataBytes.size() && (metadataTotal == 0 || metadataTotal == total));
        within(add(offset, bytes.size()), total);
        if (metadataTotal == 0) {
            metadataTotal = total;
            metadataBytes.reserve(total);
        }
        metadataBytes.insert(metadataBytes.end(), bytes.begin(), bytes.end());
        if (metadataBytes.size() == total) {
            require(hash(metadataBytes, provider) == artifactHash(nextKind),
                    ErrorCode::digestMismatch);
            get(validator.metadata(nextKind, metadataBytes));
            Bytes{}.swap(metadataBytes);
            metadataTotal = 0;
            nextKind = static_cast<DemoKind>(static_cast<std::uint16_t>(nextKind) + 1);
        }
    }
    auto expectedEnd() -> Bytes {
        require(!failed && !ended && nextKind == DemoKind::packet, ErrorCode::wrongPhase);
        Writer writer{demoEndBytes};
        writer.integer(nextSequence);
        writer.integer(lastTime.value);
        writer.bytes(finishHash().bytes);
        return std::move(writer).finish();
    }
    void consume(const DemoBlockView &block, std::span<const std::byte> encoded) {
        require(!failed && !ended && block.header.sequence == nextSequence, ErrorCode::sequence);
        require(block.header.time.value >= lastTime.value);
        const auto nextOffset = add(fileBytes, encoded.size());
        within(nextOffset, limits.demoFileBytes.value);
        if (block.header.kind != DemoKind::end) {
            within(add(nextOffset, demoBlockHeaderBytes + demoEndBytes),
                   limits.demoFileBytes.value);
        }
        if (nextKind <= DemoKind::catalog) {
            metadata(block);
        } else if (nextKind == DemoKind::bootstrap) {
            require(block.header.kind == DemoKind::bootstrap && block.header.time.value == 0,
                    ErrorCode::wrongPhase);
            get(validator.bootstrap(
                get(decodeDemoBootstrap(block.payload, limits.demoBootstrapBytes))));
            nextKind = DemoKind::packet;
        } else if (block.header.kind == DemoKind::packet) {
            const auto maximum =
                header.selected.get<session::CapabilityField::reliableMessageBytes>();
            get(validator.packet(get(decodeDemoPacket(block.payload, {maximum.value}))));
        } else if (block.header.kind == DemoKind::localRecord) {
            get(validator.local(block.payload));
        } else {
            require(block.header.kind == DemoKind::end && block.header.time == lastTime,
                    ErrorCode::wrongPhase);
            Reader reader{block.payload};
            require(reader.integer<std::uint64_t>() == nextSequence &&
                    reader.integer<std::uint64_t>() == lastTime.value);
            const auto expected = reader.identity<Digest>();
            reader.end();
            require(expected == finishHash(), ErrorCode::digestMismatch);
            get(validator.complete());
            ended = true;
        }
        if (!ended) {
            require(!finishedHash && nextSequence != std::numeric_limits<std::uint64_t>::max(),
                    ErrorCode::sequence);
            require(stream->update(encoded).has_value(), ErrorCode::storageUnavailable);
            ++nextSequence;
        }
        lastTime = block.header.time;
        fileBytes = nextOffset;
    }
};
namespace {
auto newState(DemoHeader header, ApplicationLimits limits, Bytes bytes,
              const DigestProvider &provider, const session::StreamingDigestProvider &streaming,
              DemoValidator &validator) -> std::unique_ptr<DemoState> {
    require(
        limits.demoBlockBytes.value >=
            add(header.selected.get<session::CapabilityField::reliableMessageBytes>().value, 68),
        ErrorCode::byteBudget);
    within(limits.demoBlockBytes.value, maxMetadataBytes);
    within(limits.contractArtifactBytes.value, maxMetadataBytes);
    within(limits.schemaSetBytes.value, maxMetadataBytes);
    require(limits.demoFileBytes.value >= demoHeaderBytes + demoBlockHeaderBytes + demoEndBytes &&
                limits.demoFileBytes.value <=
                    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()),
            ErrorCode::byteBudget);
    auto stream = streaming.beginSha256();
    require(stream.has_value() && *stream != nullptr, ErrorCode::storageUnavailable);
    require((*stream)->update(bytes).has_value(), ErrorCode::storageUnavailable);
    return std::make_unique<DemoState>(
        DemoState{header, limits, provider, validator, std::move(bytes), std::move(*stream)});
}
} // namespace
} // namespace extendedPrecache::data::detail

namespace extendedPrecache::data {
DemoReader::DemoReader(std::unique_ptr<detail::DemoState> input) : state{std::move(input)} {}
DemoReader::DemoReader(DemoReader &&) noexcept = default;
auto DemoReader::operator=(DemoReader &&) noexcept -> DemoReader & = default;
DemoReader::~DemoReader() = default;
DemoWriter::DemoWriter(std::unique_ptr<detail::DemoState> input) : state{std::move(input)} {}
DemoWriter::DemoWriter(DemoWriter &&) noexcept = default;
auto DemoWriter::operator=(DemoWriter &&) noexcept -> DemoWriter & = default;
DemoWriter::~DemoWriter() = default;
auto DemoReader::open(std::span<const std::byte> bytes, ApplicationLimits limits,
                      const DigestProvider &provider,
                      const session::StreamingDigestProvider &streaming,
                      DemoValidator &validator) noexcept -> Result<DemoReader> {
    return detail::protect([&] {
        const auto header = detail::get(decodeDemoHeader(bytes, provider));
        return DemoReader{detail::newState(header, limits, Bytes{bytes.begin(), bytes.end()},
                                           provider, streaming, validator)};
    });
}
auto DemoReader::accept(std::span<const std::byte> bytes) noexcept -> Result<DemoBlockView> {
    auto result = detail::protect([&] {
        detail::require(state != nullptr, ErrorCode::wrongPhase);
        const auto block = detail::get(decodeDemoBlock(bytes, state->limits, state->provider));
        state->consume(block, bytes);
        return block;
    });
    if (!result && state) {
        state->failed = true;
    }
    return result;
}
auto DemoReader::finish(FileOffset actualFileBytes) noexcept -> Result<void> {
    auto result = detail::protect([&] {
        detail::require(state && state->ended && !state->failed &&
                            actualFileBytes.value == state->fileBytes,
                        ErrorCode::malformed);
    });
    if (!result && state) {
        state->failed = true;
    }
    return result;
}
auto DemoReader::offset() const noexcept -> FileOffset {
    return {state ? state->fileBytes : 0};
}
auto DemoWriter::create(DemoHeader header, ApplicationLimits limits, const DigestProvider &provider,
                        const session::StreamingDigestProvider &streaming,
                        DemoValidator &validator) noexcept -> Result<DemoWriter> {
    return detail::protect([&] {
        auto bytes = detail::get(encodeDemoHeader(header, provider));
        return DemoWriter{
            detail::newState(header, limits, std::move(bytes), provider, streaming, validator)};
    });
}
auto DemoWriter::header() const noexcept -> std::span<const std::byte> {
    return state ? std::span<const std::byte>{state->headerBytes} : std::span<const std::byte>{};
}
auto DemoWriter::append(DemoKind kind, TimeMicros time, std::span<const std::byte> payload) noexcept
    -> Result<Bytes> {
    auto result = detail::protect([&] {
        detail::require(state && kind != DemoKind::end, ErrorCode::wrongPhase);
        auto bytes = detail::get(encodeDemoBlock(kind, state->nextSequence, time, payload,
                                                 state->limits, state->provider));
        const auto block = detail::get(decodeDemoBlock(bytes, state->limits, state->provider));
        state->consume(block, bytes);
        return bytes;
    });
    if (!result && state) {
        state->failed = true;
    }
    return result;
}
auto DemoWriter::finish() noexcept -> Result<Bytes> {
    auto result = detail::protect([&] {
        detail::require(state != nullptr, ErrorCode::wrongPhase);
        const auto payload = state->expectedEnd();
        auto bytes =
            detail::get(encodeDemoBlock(DemoKind::end, state->nextSequence, state->lastTime,
                                        payload, state->limits, state->provider));
        const auto block = detail::get(decodeDemoBlock(bytes, state->limits, state->provider));
        state->consume(block, bytes);
        return bytes;
    });
    if (!result && state) {
        state->failed = true;
    }
    return result;
}
auto DemoWriter::offset() const noexcept -> FileOffset {
    return {state ? state->fileBytes : 0};
}
} // namespace extendedPrecache::data
