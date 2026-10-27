#include "binary.hpp"
#include <extendedPrecache/data/demo.hpp>

namespace extendedPrecache::data {
namespace {
using namespace detail;
void validHeader(const DemoHeader &header) {
    require(header.epoch.value != 0 && !header.nonce.isZero());
    require(!header.applicationHash.isZero() && !header.dataHash.isZero() &&
            !header.catalogHash.isZero() && !header.schemaHash.isZero());
    require(session::CapabilityLimits::create(header.selected).has_value(), ErrorCode::capacity);
}
void validBootstrap(DemoBootstrap bootstrap, ByteCount maximum) {
    require(bootstrap.firstReliable.value <= maxSequence && bootstrap.firstUnreliable.value == 0);
    require(!bootstrap.serverInfo.empty() && bootstrap.serverInfo.front() == std::byte{11});
    within(add(demoBootstrapHeaderBytes, bootstrap.serverInfo.size()), maximum.value);
}
} // namespace
auto encodeDemoHeader(const DemoHeader &header, const DigestProvider &provider) noexcept
    -> Result<Bytes> {
    return detail::protect([&] {
        validHeader(header);
        Writer writer{demoHeaderBytes};
        writer.text("EPCDEMO2");
        writer.integer(std::uint16_t{2});
        writer.zero(2);
        writer.integer(demoHeaderBytes);
        writer.integer(std::uint32_t{2});
        writer.integer(header.spawn.value);
        writer.integer(header.epoch.value);
        writer.bytes(header.nonce.bytes);
        for (const auto value : header.selected.wireValues()) {
            writer.integer(value);
        }
        for (const auto digest :
             {header.applicationHash, header.dataHash, header.catalogHash, header.schemaHash}) {
            writer.bytes(digest.bytes);
        }
        const auto digest = hash(writer.view(), provider);
        writer.bytes(digest.bytes);
        return std::move(writer).finish();
    });
}
auto decodeDemoHeader(std::span<const std::byte> bytes, const DigestProvider &provider) noexcept
    -> Result<DemoHeader> {
    return detail::protect([&] {
        require(bytes.size() == demoHeaderBytes);
        Reader reader{bytes};
        require(reader.text(8, 8) == "EPCDEMO2");
        require(reader.integer<std::uint16_t>() == 2, ErrorCode::unsupportedContract);
        reader.zero(2);
        require(reader.integer<std::uint32_t>() == demoHeaderBytes &&
                    reader.integer<std::uint32_t>() == 2,
                ErrorCode::unsupportedContract);
        const auto spawn = session::SpawnCount{reader.integer<std::uint32_t>()};
        const auto epoch = MapEpoch{reader.integer<std::uint64_t>()};
        const auto nonce = reader.identity<SessionNonce>();
        std::array<std::uint32_t, session::capabilityCount> limits{};
        for (auto &value : limits) {
            value = reader.integer<std::uint32_t>();
        }
        DemoHeader header{spawn,
                          epoch,
                          nonce,
                          session::CapabilityVector{limits},
                          reader.identity<Digest>(),
                          reader.identity<Digest>(),
                          reader.identity<Digest>(),
                          reader.identity<Digest>()};
        const auto expected = reader.identity<Digest>();
        reader.end();
        require(hash(bytes.first(demoHeaderBytes - digestBytes), provider) == expected,
                ErrorCode::digestMismatch);
        validHeader(header);
        return header;
    });
}
auto decodeDemoBlockHeader(std::span<const std::byte> bytes,
                           const ApplicationLimits &limits) noexcept -> Result<DemoBlockHeader> {
    return detail::protect([&] {
        require(bytes.size() == demoBlockHeaderBytes);
        Reader reader{bytes};
        const auto rawKind = reader.integer<std::uint16_t>();
        require(rawKind >= static_cast<std::uint16_t>(DemoKind::dataArtifact) &&
                rawKind <= static_cast<std::uint16_t>(DemoKind::end));
        const auto kind = static_cast<DemoKind>(rawKind);
        reader.zero(2);
        const auto length = ByteCount{reader.integer<std::uint32_t>()};
        const auto sequence = reader.integer<std::uint64_t>();
        const auto time = TimeMicros{reader.integer<std::uint64_t>()};
        const auto digest = reader.identity<Digest>();
        reader.end();
        require(kind >= DemoKind::dataArtifact && kind <= DemoKind::end);
        within(add(demoBlockHeaderBytes, length.value), limits.demoBlockBytes.value);
        within(time.value, limits.demoDuration.value);
        return DemoBlockHeader{kind, length, sequence, time, digest};
    });
}
auto encodeDemoBlock(DemoKind kind, std::uint64_t sequence, TimeMicros time,
                     std::span<const std::byte> payload, const ApplicationLimits &limits,
                     const DigestProvider &provider) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        require(kind >= DemoKind::dataArtifact && kind <= DemoKind::end);
        within(add(demoBlockHeaderBytes, payload.size()), limits.demoBlockBytes.value);
        within(time.value, limits.demoDuration.value);
        Writer writer{limits.demoBlockBytes.value};
        writer.integer(static_cast<std::uint16_t>(kind));
        writer.zero(2);
        writer.integer(narrow(payload.size()));
        writer.integer(sequence);
        writer.integer(time.value);
        writer.bytes(hash(payload, provider).bytes);
        writer.bytes(payload);
        return std::move(writer).finish();
    });
}
auto decodeDemoBlock(std::span<const std::byte> bytes, const ApplicationLimits &limits,
                     const DigestProvider &provider) noexcept -> Result<DemoBlockView> {
    return detail::protect([&] {
        Reader reader{bytes};
        const auto header = get(decodeDemoBlockHeader(reader.take(demoBlockHeaderBytes), limits));
        const auto payload = reader.take(header.payloadBytes.value);
        reader.end();
        require(hash(payload, provider) == header.payloadHash, ErrorCode::digestMismatch);
        return DemoBlockView{header, payload};
    });
}
auto encodeDemoBootstrap(DemoBootstrap bootstrap, ByteCount maximum) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        validBootstrap(bootstrap, maximum);
        Writer writer{maximum.value};
        writer.integer(bootstrap.firstReliable.value);
        writer.integer(bootstrap.firstUnreliable.value);
        writer.integer(narrow(bootstrap.serverInfo.size()));
        writer.bytes(bootstrap.serverInfo);
        return std::move(writer).finish();
    });
}
auto decodeDemoBootstrap(std::span<const std::byte> bytes, ByteCount maximum) noexcept
    -> Result<DemoBootstrap> {
    return detail::protect([&] {
        within(bytes.size(), maximum.value);
        Reader reader{bytes};
        const auto reliable = session::FrameSequence{reader.integer<std::uint32_t>()};
        const auto unreliable = session::FrameSequence{reader.integer<std::uint32_t>()};
        const auto count = reader.integer<std::uint32_t>();
        const DemoBootstrap result{reliable, unreliable, reader.take(count)};
        reader.end();
        validBootstrap(result, maximum);
        return result;
    });
}
auto encodeDemoPacket(DemoPacket packet, ByteCount maximum) noexcept -> Result<Bytes> {
    return detail::protect([&] {
        require(packet.source == PacketSource::channel ||
                packet.source == PacketSource::reliableReassembly);
        require(!packet.payload.empty());
        within(packet.payload.size(), maximum.value);
        Writer writer{add(maximum.value, demoPacketHeaderBytes)};
        writer.enumeration(packet.source);
        writer.zero(3);
        writer.integer(packet.nativeSequence);
        writer.integer(narrow(packet.payload.size()));
        writer.bytes(packet.payload);
        return std::move(writer).finish();
    });
}
auto decodeDemoPacket(std::span<const std::byte> bytes, ByteCount maximum) noexcept
    -> Result<DemoPacket> {
    return detail::protect([&] {
        Reader reader{bytes};
        const auto source = reader.enumeration<PacketSource>();
        reader.zero(3);
        const auto sequence = reader.integer<std::uint32_t>();
        const auto count = reader.integer<std::uint32_t>();
        within(count, maximum.value);
        require(count > 0 &&
                (source == PacketSource::channel || source == PacketSource::reliableReassembly));
        const DemoPacket result{source, sequence, reader.take(count)};
        reader.end();
        return result;
    });
}
} // namespace extendedPrecache::data
