#include "binary.hpp"
#include <extendedPrecache/data/demo.hpp>

namespace extendedPrecache::data {
namespace {
using namespace detail;
auto localSchema(const DemoLocalRecord &record, DemoLocalContext context) -> const Schema & {
    const auto *module = context.application.module(record.module);
    require(module && module->client, ErrorCode::unsupportedModule);
    const auto found =
        std::ranges::find_if(module->localReplay, [&](const LocalReplayBinding &binding) {
            return binding.recordKind == record.kind && binding.schema == record.schema;
        });
    require(found != module->localReplay.end(), ErrorCode::unsupportedModule);
    const auto *schema = findSchema(context.application.definition().schemas, record.schema);
    require(schema && schema->role ==
                          (record.kind == 6 ? SchemaRole::moduleState : SchemaRole::localReplay),
            ErrorCode::invalidSchema);
    return *schema;
}
} // namespace
auto encodeDemoLocal(const DemoLocalRecord &record, DemoLocalContext context) noexcept
    -> Result<Bytes> {
    return detail::protect([&] {
        const auto &schema = localSchema(record, context);
        const auto values =
            get(encodeValues(schema, record.values, context.values, context.budget));
        within(add(demoLocalHeaderBytes, values.size()), context.budget.messageBytes.value);
        Writer writer{context.budget.ownedBytes.value};
        writer.integer(record.kind);
        writer.integer(record.module.value);
        writer.integer(record.schema.value);
        writer.zero(2);
        writer.integer(narrow(values.size()));
        writer.bytes(values);
        return std::move(writer).finish();
    });
}
auto decodeDemoLocal(std::span<const std::byte> bytes, DemoLocalContext context) noexcept
    -> Result<DemoLocalRecord> {
    return detail::protect([&] {
        inputBudget(bytes, context.budget);
        Reader reader{bytes};
        DemoLocalRecord record{};
        record.kind = reader.integer<std::uint16_t>();
        record.module = {reader.integer<std::uint16_t>()};
        record.schema = {reader.integer<std::uint16_t>()};
        reader.zero(2);
        const auto length = reader.integer<std::uint32_t>();
        const auto values = reader.take(length);
        reader.end();
        const auto &schema = localSchema(record, context);
        record.values = get(decodeValues(schema, values, context.values, context.budget));
        return record;
    });
}
auto ContractDemoValidator::metadata(DemoKind kind, std::span<const std::byte> bytes) noexcept
    -> Result<void> {
    return detail::protect([&] {
        const auto &app = applicationValue.definition();
        require(headerValue.applicationHash == applicationValue.digest() &&
                    headerValue.dataHash == app.dataContractHash &&
                    headerValue.schemaHash == app.schemaSetHash,
                ErrorCode::unsupportedContract);
        require(headerValue.selected == catalogContextValue.selected.vector(), ErrorCode::capacity);
        switch (kind) {
        case DemoKind::dataArtifact:
            require(hash(bytes, providerValue) == app.dataContractHash, ErrorCode::digestMismatch);
            break;
        case DemoKind::applicationArtifact: {
            const auto text =
                std::string_view{reinterpret_cast<const char *>(bytes.data()), bytes.size()};
            const auto parsed = get(
                ApplicationContract::read(text, providerValue, app.limits.contractArtifactBytes));
            require(parsed.digest() == applicationValue.digest(), ErrorCode::digestMismatch);
            break;
        }
        case DemoKind::schemaSet:
            require(get(decodeSchemaSet(bytes, app.limits.schemaSetBytes)) == app.schemas,
                    ErrorCode::invalidSchema);
            break;
        case DemoKind::catalog: {
            auto parsed = get(Catalog::decode(bytes, catalogContextValue, headerValue.catalogHash,
                                              providerValue));
            require(parsed.digest() == headerValue.catalogHash, ErrorCode::digestMismatch);
            catalogValue = std::move(parsed);
            break;
        }
        default:
            fail(ErrorCode::wrongPhase);
        }
    });
}
auto ContractDemoValidator::bootstrap(DemoBootstrap record) noexcept -> Result<void> {
    return detail::protect([&] {
        require(catalogValue.has_value(), ErrorCode::wrongPhase);
        get(recordsValue.bootstrap(record, *catalogValue));
    });
}
auto ContractDemoValidator::packet(DemoPacket record) noexcept -> Result<void> {
    return detail::protect([&] {
        require(catalogValue.has_value(), ErrorCode::wrongPhase);
        get(recordsValue.packet(record, *catalogValue));
    });
}
auto ContractDemoValidator::local(std::span<const std::byte> record) noexcept -> Result<void> {
    return detail::protect([&] {
        require(catalogValue.has_value(), ErrorCode::wrongPhase);
        const auto &limits = applicationValue.definition().limits;
        require(limits.demoBlockBytes.value >= demoBlockHeaderBytes, ErrorCode::byteBudget);
        const DemoLocalContext context{
            applicationValue,
            {*catalogValue, applicationValue.definition().objects.entityEntries},
            {{limits.demoBlockBytes.value - demoBlockHeaderBytes}, catalogContextValue.ownedBytes}};
        get(recordsValue.local(get(decodeDemoLocal(record, context))));
    });
}
auto ContractDemoValidator::complete() noexcept -> Result<void> {
    return detail::protect([&] {
        require(catalogValue.has_value(), ErrorCode::wrongPhase);
        get(recordsValue.complete());
    });
}
} // namespace extendedPrecache::data
