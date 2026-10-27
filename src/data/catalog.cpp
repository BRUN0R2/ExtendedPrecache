#include "binary.hpp"
#include <extendedPrecache/data/catalog.hpp>
#include <extendedPrecache/domain/resourcePath.hpp>

#include <charconv>
#include <set>

namespace extendedPrecache::data {
namespace {
using namespace detail;
void path(std::string_view name, std::uint32_t maximum) {
    const auto limits = ::extendedPrecache::CatalogLimits::create({1}, {maximum});
    require(limits && validateResourcePath(name, *limits).has_value());
}
void bounds(const Bounds &value) {
    for (std::size_t index = 0; index < 3; ++index) {
        validFloat(value.mins[index]);
        validFloat(value.maxs[index]);
        require(value.mins[index] <= value.maxs[index]);
    }
}
auto writeCatalog(const CatalogData &data, std::uint32_t maximum) -> Bytes {
    Writer writer{maximum};
    writer.text("EPCM");
    writer.integer(std::uint16_t{2});
    writer.zero(2);
    writer.integer(narrow(data.resources.size()));
    writer.integer(narrow(data.assets.size()));
    writer.integer(narrow(data.rules.size()));
    for (const auto count : data.counts) {
        writer.integer(count);
    }
    writer.bytes(data.schemaSetHash.bytes);
    for (const auto &row : data.resources) {
        writer.integer(row.ordinal.value);
        writer.enumeration(row.reference.kind);
        writer.enumeration(row.type);
        writer.integer(std::uint16_t{1});
        writer.integer(row.reference.id.value);
        writer.integer(row.asset.value);
        writer.integer(row.parent ? static_cast<std::uint8_t>(row.parent->kind) : std::uint8_t{});
        writer.zero(3);
        writer.integer(row.parent ? row.parent->id.value : 0U);
        writer.integer(small(row.name.size()));
        writer.zero(2);
        writer.text(row.name);
    }
    for (const auto &asset : data.assets) {
        writer.integer(asset.ordinal.value);
        writer.integer(asset.fileBytes.value);
        writer.bytes(asset.digest.bytes);
        writer.integer(small(asset.path.size()));
        writer.integer(static_cast<std::uint16_t>(asset.downloadAllowed));
        writer.text(asset.path);
    }
    for (const auto &rule : data.rules) {
        writer.integer(rule.resource.value);
        writer.enumeration(rule.mode);
        writer.zero(3);
        if (rule.bounds) {
            const auto &extent = *rule.bounds;
            for (const auto value : extent.mins) {
                writer.f32(value);
            }
            for (const auto value : extent.maxs) {
                writer.f32(value);
            }
        }
    }
    return std::move(writer).finish();
}
void validateData(const CatalogData &data, CatalogContext context) {
    require(session::admitRequirements(context.requirements, context.selected).has_value(),
            ErrorCode::capacity);
    require(!data.schemaSetHash.isZero() && data.schemaSetHash == context.schemaSetHash,
            ErrorCode::digestMismatch);
    const auto &required = context.requirements.vector();
    std::uint64_t count{};
    for (std::size_t index = 0; index < data.counts.size(); ++index) {
        require(data.counts[index] == required.wireValues()[index], ErrorCode::capacity);
        count = add(count, data.counts[index]);
    }
    require(count == data.resources.size() &&
                count == required.get<session::CapabilityField::resourceEntries>().value &&
                data.rules.size() ==
                    required.get<session::CapabilityField::consistencyEntries>().value &&
                data.assets.size() <= count && data.rules.size() <= count &&
                !data.resources.empty(),
            ErrorCode::capacity);
    AllocationBudget allocation{context.ownedBytes};
    allocation.charge(
        add(sizeof(CatalogData),
            context.requirements.vector().get<session::CapabilityField::manifestBytes>().value));
    allocation.charge(multiply(data.resources.size(), sizeof(Resource)));
    allocation.charge(multiply(data.assets.size(), sizeof(Asset)));
    allocation.charge(multiply(data.rules.size(), sizeof(ConsistencyRule)));
    std::set<std::pair<ResourceKind, std::string_view>> names;
    std::set<std::string_view> assetPaths;
    for (std::size_t index = 0; index < data.assets.size(); ++index) {
        const auto &asset = data.assets[index];
        require(asset.ordinal.value == index && !asset.digest.isZero() &&
                    asset.fileBytes.value <=
                        required.get<session::CapabilityField::fileBytes>().value &&
                    asset.fileBytes.value <= maxFileBytes,
                ErrorCode::assetFailure);
        path(asset.path, maxNameBytes);
        require(assetPaths.insert(asset.path).second, ErrorCode::assetFailure);
        allocation.charge(asset.path.size());
    }
    std::vector<bool> used(data.assets.size());
    std::uint32_t nextAsset{};
    std::size_t ordinal{};
    for (std::size_t category = 0; category < data.counts.size(); ++category) {
        const auto kind = static_cast<ResourceKind>(category + 1);
        for (std::uint32_t id = 1; id <= data.counts[category]; ++id, ++ordinal) {
            const auto &row = data.resources[ordinal];
            require(row.ordinal.value == ordinal &&
                    row.reference == ResourceReference{kind, {id}} &&
                    row.asset.value < data.assets.size());
            path(row.name, kind == ResourceKind::decal ? maxDecalNameBytes : maxNameBytes);
            require(names.emplace(kind, row.name).second);
            allocation.charge(row.name.size());
            const auto type = static_cast<std::uint8_t>(row.type);
            require(kind == ResourceKind::model ? type >= 1 && type <= 4 : type == category + 4);
            if (!used[row.asset.value]) {
                require(row.asset.value == nextAsset);
                used[row.asset.value] = true;
                ++nextAsset;
            }
            if (row.type == ResourceType::world) {
                require(id == 1);
            }
            if (kind == ResourceKind::model && id == 1) {
                require(row.type == ResourceType::world);
            }
            if (row.type == ResourceType::inlineModel) {
                require(row.parent == ResourceReference{ResourceKind::model, {1}} &&
                        row.asset == data.resources.front().asset);
                require(row.name.size() > 1 && row.name.front() == '*');
                std::uint32_t submodel{};
                const auto parsed = std::from_chars(row.name.data() + 1,
                                                    row.name.data() + row.name.size(), submodel);
                require(parsed.ec == std::errc{} &&
                        parsed.ptr == row.name.data() + row.name.size() && submodel > 0);
            } else {
                require(!row.parent);
            }
        }
    }
    require(nextAsset == data.assets.size());
    std::optional<ResourceOrdinal> previous;
    for (const auto &rule : data.rules) {
        require(rule.resource.value < count &&
                (!previous || rule.resource.value > previous->value));
        previous = rule.resource;
        require(rule.mode >= ConsistencyMode::exactDigest &&
                rule.mode <= ConsistencyMode::withinBounds);
        require(rule.bounds.has_value() == (rule.mode != ConsistencyMode::exactDigest));
        if (rule.bounds) {
            require(data.resources[rule.resource.value].reference.kind == ResourceKind::model);
            bounds(*rule.bounds);
        }
    }
}
} // namespace
struct Catalog::Storage final {
    CatalogData data;
    Bytes bytes;
    Digest digest;
};
auto Catalog::create(CatalogData data, CatalogContext context,
                     const DigestProvider &provider) noexcept -> Result<Catalog> {
    return detail::protect([&] {
        validateData(data, context);
        for (auto &rule : data.rules) {
            if (rule.bounds) {
                for (auto &component : rule.bounds->mins) {
                    component = canonicalFloat(component);
                }
                for (auto &component : rule.bounds->maxs) {
                    component = canonicalFloat(component);
                }
            }
        }
        auto canonical = writeCatalog(
            data, context.selected.vector().get<session::CapabilityField::manifestBytes>().value);
        require(
            canonical.size() ==
                context.requirements.vector().get<session::CapabilityField::manifestBytes>().value,
            ErrorCode::byteBudget);
        within(canonical.size(), context.ownedBytes.value);
        const auto identity = hash(canonical, provider);
        return Catalog{std::make_shared<const Storage>(
            Storage{std::move(data), std::move(canonical), identity})};
    });
}
auto Catalog::decode(std::span<const std::byte> bytes, CatalogContext context,
                     Digest expectedCatalogHash, const DigestProvider &provider) noexcept
    -> Result<Catalog> {
    return detail::protect([&] {
        within(bytes.size(), context.ownedBytes.value);
        require(
            bytes.size() ==
                context.requirements.vector().get<session::CapabilityField::manifestBytes>().value,
            ErrorCode::byteBudget);
        within(bytes.size(),
               context.selected.vector().get<session::CapabilityField::manifestBytes>().value);
        require(!expectedCatalogHash.isZero() && hash(bytes, provider) == expectedCatalogHash,
                ErrorCode::digestMismatch);
        Reader reader{bytes};
        require(reader.text(4, 4) == "EPCM");
        require(reader.integer<std::uint16_t>() == 2, ErrorCode::unsupportedContract);
        reader.zero(2);
        const auto count = reader.integer<std::uint32_t>();
        const auto assets = reader.integer<std::uint32_t>();
        const auto rules = reader.integer<std::uint32_t>();
        require(count == context.requirements.vector()
                             .get<session::CapabilityField::resourceEntries>()
                             .value &&
                    rules == context.requirements.vector()
                                 .get<session::CapabilityField::consistencyEntries>()
                                 .value &&
                    assets <= count && rules <= count,
                ErrorCode::capacity);
        CatalogData data{};
        for (auto &value : data.counts) {
            value = reader.integer<std::uint32_t>();
        }
        data.schemaSetHash = reader.identity<Digest>();
        AllocationBudget allocation{context.ownedBytes};
        allocation.charge(add(sizeof(CatalogData), bytes.size()));
        allocation.charge(
            add(add(multiply(count, sizeof(Resource)), multiply(assets, sizeof(Asset))),
                multiply(rules, sizeof(ConsistencyRule))));
        reader.count(count, 29);
        data.resources.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            Resource row{};
            row.ordinal = {reader.integer<std::uint32_t>()};
            row.reference.kind = reader.enumeration<ResourceKind>();
            row.type = reader.enumeration<ResourceType>();
            require(reader.integer<std::uint16_t>() == 1);
            row.reference.id = {reader.integer<std::uint32_t>()};
            row.asset = {reader.integer<std::uint32_t>()};
            const auto parentKind = reader.integer<std::uint8_t>();
            reader.zero(3);
            const auto parentId = reader.integer<std::uint32_t>();
            if (parentKind != 0 || parentId != 0) {
                require(parentKind != 0 && parentId != 0);
                row.parent = ResourceReference{static_cast<ResourceKind>(parentKind), {parentId}};
            }
            const auto length = reader.integer<std::uint16_t>();
            reader.zero(2);
            allocation.charge(length);
            row.name = reader.text(length, maxNameBytes);
            data.resources.push_back(std::move(row));
        }
        reader.count(assets, 45);
        data.assets.reserve(assets);
        for (std::uint32_t index = 0; index < assets; ++index) {
            Asset asset{};
            asset.ordinal = {reader.integer<std::uint32_t>()};
            asset.fileBytes = {reader.integer<std::uint32_t>()};
            asset.digest = reader.identity<Digest>();
            const auto length = reader.integer<std::uint16_t>();
            const auto flags = reader.integer<std::uint16_t>();
            require(flags <= 1);
            asset.downloadAllowed = flags == 1;
            allocation.charge(length);
            asset.path = reader.text(length, maxNameBytes);
            data.assets.push_back(std::move(asset));
        }
        reader.count(rules, 8);
        data.rules.reserve(rules);
        for (std::uint32_t index = 0; index < rules; ++index) {
            ConsistencyRule rule{};
            rule.resource = {reader.integer<std::uint32_t>()};
            rule.mode = reader.enumeration<ConsistencyMode>();
            reader.zero(3);
            if (rule.mode == ConsistencyMode::exactBounds ||
                rule.mode == ConsistencyMode::withinBounds) {
                Bounds value{};
                for (auto &item : value.mins) {
                    item = reader.f32();
                }
                for (auto &item : value.maxs) {
                    item = reader.f32();
                }
                rule.bounds = value;
            }
            data.rules.push_back(rule);
        }
        reader.end();
        return get(create(std::move(data), context, provider));
    });
}
auto Catalog::data() const noexcept -> const CatalogData & {
    return storage->data;
}
auto Catalog::bytes() const noexcept -> std::span<const std::byte> {
    return storage->bytes;
}
auto Catalog::digest() const noexcept -> Digest {
    return storage->digest;
}
auto Catalog::resolve(ResourceReference reference) const noexcept -> const Resource * {
    if (!validKind(reference.kind) || reference.id.value == 0 ||
        reference.id.value > data().counts[kindIndex(reference.kind)]) {
        return nullptr;
    }
    std::uint64_t ordinal = reference.id.value - 1;
    for (std::size_t index = 0; index < kindIndex(reference.kind); ++index) {
        ordinal += data().counts[index];
    }
    return &data().resources[static_cast<std::size_t>(ordinal)];
}
auto Catalog::validate(ResourceReference reference, bool nullable) const noexcept -> Result<void> {
    if (!validKind(reference.kind) ||
        (reference.id.value == 0 ? !nullable : resolve(reference) == nullptr)) {
        return std::unexpected{Failure{ErrorCode::capacity}};
    }
    return {};
}
} // namespace extendedPrecache::data
