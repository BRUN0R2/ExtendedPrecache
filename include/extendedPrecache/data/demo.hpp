#pragma once
#include <extendedPrecache/data/application.hpp>

namespace extendedPrecache::data {
enum class DemoKind : std::uint8_t {
    dataArtifact = 1,
    applicationArtifact,
    schemaSet,
    catalog,
    bootstrap,
    packet,
    localRecord,
    end
};
enum class PacketSource : std::uint8_t { channel, reliableReassembly };
struct DemoHeader final {
    session::SpawnCount spawn;
    MapEpoch epoch;
    SessionNonce nonce;
    session::CapabilityVector selected;
    Digest applicationHash;
    Digest dataHash;
    Digest catalogHash;
    Digest schemaHash;
};
struct DemoBlockHeader final {
    DemoKind kind;
    ByteCount payloadBytes;
    std::uint64_t sequence{};
    TimeMicros time;
    Digest payloadHash;
};
struct DemoBlockView final {
    DemoBlockHeader header;
    std::span<const std::byte> payload;
};
struct DemoBootstrap final {
    session::FrameSequence firstReliable;
    session::FrameSequence firstUnreliable;
    std::span<const std::byte> serverInfo;
};
struct DemoPacket final {
    PacketSource source;
    std::uint32_t nativeSequence{};
    std::span<const std::byte> payload;
};
struct DemoLocalRecord final {
    std::uint16_t kind{};
    ModuleId module;
    SchemaId schema;
    ValueSet values;
    auto operator==(const DemoLocalRecord &) const -> bool = default;
};
struct DemoLocalContext final {
    const ApplicationContract &application;
    ValueContext values;
    RecordBudget budget;
};
[[nodiscard]] auto encodeDemoHeader(const DemoHeader &header,
                                    const DigestProvider &provider) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeDemoHeader(std::span<const std::byte> bytes,
                                    const DigestProvider &provider) noexcept -> Result<DemoHeader>;
[[nodiscard]] auto decodeDemoBlockHeader(std::span<const std::byte> bytes,
                                         const ApplicationLimits &limits) noexcept
    -> Result<DemoBlockHeader>;
[[nodiscard]] auto encodeDemoBlock(DemoKind kind, std::uint64_t sequence, TimeMicros time,
                                   std::span<const std::byte> payload,
                                   const ApplicationLimits &limits,
                                   const DigestProvider &provider) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeDemoBlock(std::span<const std::byte> bytes,
                                   const ApplicationLimits &limits,
                                   const DigestProvider &provider) noexcept
    -> Result<DemoBlockView>;
[[nodiscard]] auto encodeDemoBootstrap(DemoBootstrap bootstrap, ByteCount maximum) noexcept
    -> Result<Bytes>;
[[nodiscard]] auto decodeDemoBootstrap(std::span<const std::byte> bytes, ByteCount maximum) noexcept
    -> Result<DemoBootstrap>;
[[nodiscard]] auto encodeDemoPacket(DemoPacket packet, ByteCount maximum) noexcept -> Result<Bytes>;
[[nodiscard]] auto decodeDemoPacket(std::span<const std::byte> bytes, ByteCount maximum) noexcept
    -> Result<DemoPacket>;
[[nodiscard]] auto encodeDemoLocal(const DemoLocalRecord &record, DemoLocalContext context) noexcept
    -> Result<Bytes>;
[[nodiscard]] auto decodeDemoLocal(std::span<const std::byte> bytes,
                                   DemoLocalContext context) noexcept -> Result<DemoLocalRecord>;

// Validation callbacks are local, side-effect-free parsers, never playback callbacks.
class DemoValidator {
  public:
    virtual ~DemoValidator() = default;
    [[nodiscard]] virtual auto metadata(DemoKind kind, std::span<const std::byte> bytes) noexcept
        -> Result<void> = 0;
    [[nodiscard]] virtual auto bootstrap(DemoBootstrap record) noexcept -> Result<void> = 0;
    [[nodiscard]] virtual auto packet(DemoPacket record) noexcept -> Result<void> = 0;
    [[nodiscard]] virtual auto local(std::span<const std::byte> record) noexcept
        -> Result<void> = 0;
    [[nodiscard]] virtual auto complete() noexcept -> Result<void> = 0;
};
class DemoRecordValidator {
  public:
    virtual ~DemoRecordValidator() = default;
    [[nodiscard]] virtual auto bootstrap(DemoBootstrap record, const Catalog &catalog) noexcept
        -> Result<void> = 0;
    [[nodiscard]] virtual auto packet(DemoPacket record, const Catalog &catalog) noexcept
        -> Result<void> = 0;
    [[nodiscard]] virtual auto local(const DemoLocalRecord &record) noexcept -> Result<void> = 0;
    [[nodiscard]] virtual auto complete() noexcept -> Result<void> = 0;
};
class ContractDemoValidator final : public DemoValidator {
  public:
    ContractDemoValidator(DemoHeader header, ApplicationContract application,
                          CatalogContext catalogContext, const DigestProvider &provider,
                          DemoRecordValidator &records)
        : headerValue{header}, applicationValue{std::move(application)},
          catalogContextValue{catalogContext}, providerValue{provider}, recordsValue{records} {}
    auto metadata(DemoKind kind, std::span<const std::byte> bytes) noexcept
        -> Result<void> override;
    auto bootstrap(DemoBootstrap record) noexcept -> Result<void> override;
    auto packet(DemoPacket record) noexcept -> Result<void> override;
    auto local(std::span<const std::byte> record) noexcept -> Result<void> override;
    auto complete() noexcept -> Result<void> override;
    [[nodiscard]] auto catalog() const noexcept -> const std::optional<Catalog> & {
        return catalogValue;
    }

  private:
    DemoHeader headerValue;
    ApplicationContract applicationValue;
    CatalogContext catalogContextValue;
    const DigestProvider &providerValue;
    DemoRecordValidator &recordsValue;
    std::optional<Catalog> catalogValue;
};
namespace detail {
struct DemoState;
}
class DemoReader final {
  public:
    DemoReader(DemoReader &&) noexcept;
    auto operator=(DemoReader &&) noexcept -> DemoReader &;
    ~DemoReader();
    [[nodiscard]] static auto open(std::span<const std::byte> header, ApplicationLimits limits,
                                   const DigestProvider &provider,
                                   const session::StreamingDigestProvider &streaming,
                                   DemoValidator &validator) noexcept -> Result<DemoReader>;
    [[nodiscard]] auto accept(std::span<const std::byte> block) noexcept -> Result<DemoBlockView>;
    [[nodiscard]] auto finish(FileOffset actualFileBytes) noexcept -> Result<void>;
    [[nodiscard]] auto offset() const noexcept -> FileOffset;

  private:
    explicit DemoReader(std::unique_ptr<detail::DemoState> state);
    std::unique_ptr<detail::DemoState> state;
};
class DemoWriter final {
  public:
    DemoWriter(DemoWriter &&) noexcept;
    auto operator=(DemoWriter &&) noexcept -> DemoWriter &;
    ~DemoWriter();
    [[nodiscard]] static auto create(DemoHeader header, ApplicationLimits limits,
                                     const DigestProvider &provider,
                                     const session::StreamingDigestProvider &streaming,
                                     DemoValidator &validator) noexcept -> Result<DemoWriter>;
    [[nodiscard]] auto header() const noexcept -> std::span<const std::byte>;
    [[nodiscard]] auto append(DemoKind kind, TimeMicros time,
                              std::span<const std::byte> payload) noexcept -> Result<Bytes>;
    [[nodiscard]] auto finish() noexcept -> Result<Bytes>;
    [[nodiscard]] auto offset() const noexcept -> FileOffset;

  private:
    explicit DemoWriter(std::unique_ptr<detail::DemoState> state);
    std::unique_ptr<detail::DemoState> state;
};
} // namespace extendedPrecache::data
