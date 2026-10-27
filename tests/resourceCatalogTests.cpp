#include <extendedPrecache/domain/resourceCatalog.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#include "testSuite.hpp"

namespace {

using namespace extendedPrecache;
using extendedPrecache::tests::TestSuite;
using namespace std::string_view_literals;

constexpr auto legacyPathByteLimit = 63U;
constexpr auto testLimits =
    CatalogLimits::create(ResourceCapacity{1024}, ResourcePathByteLimit{legacyPathByteLimit})
        .value();

static_assert(!CatalogLimits::create(ResourceCapacity{0},
                                     ResourcePathByteLimit{legacyPathByteLimit}));
static_assert(!CatalogLimits::create(ResourceCapacity{1}, ResourcePathByteLimit{0}));
static_assert(!IndexEncoding::create(0));
static_assert(!IndexEncoding::create(33));
static_assert(!IndexEncoding::create(256));
static_assert(IndexEncoding::create(1)->maxValue() == 1);
static_assert(IndexEncoding::create(9)->maxValue() == 511);
static_assert(IndexEncoding::create(12)->maxValue() == 4095);
static_assert(IndexEncoding::create(32)->maxValue() == std::numeric_limits<std::uint32_t>::max());
static_assert(!std::is_constructible_v<ResourceIndex<ResourceKind::model>, std::uint32_t>);
static_assert(!std::equality_comparable_with<ResourceIndex<ResourceKind::model>,
                                             ResourceIndex<ResourceKind::sound>>);

template <ResourceKind kind> void testCategory(TestSuite &suite) {
    ResourceCatalog<kind> catalog{testLimits};
    const auto first = catalog.intern("resources/example.dat");
    if (!suite.check(first.has_value(), "register canonical path")) {
        return;
    }
    static_cast<void>(suite.check(first->value() == 1, "reserve index zero"));
    static_cast<void>(suite.check(catalog.intern("resources/example.dat") == first,
                                  "deduplicate within category"));
    static_cast<void>(suite.check(catalog.size() == 1, "duplicate consumes no capacity"));
    static_cast<void>(
        suite.check(catalog.find("resources/example.dat") == *first, "find registered name"));
    static_cast<void>(suite.check(!catalog.find("resources/missing.dat"), "missing lookup"));
}

void testCapacityAndEncoding(TestSuite &suite) {
    ResourceCatalog<ResourceKind::model> catalog{testLimits};
    const auto legacyEncoding = IndexEncoding::create(9).value();
    constexpr auto legacyFirstUnrepresentableIndex = 512U;

    for (std::uint32_t index = 1; index <= testLimits.entryCapacity(); ++index) {
        const auto path = "models/example" + std::to_string(index) + ".mdl";
        const auto registered = catalog.intern(path);
        if (!suite.check(registered.has_value(), "register up to configured capacity")) {
            return;
        }
        static_cast<void>(suite.check(registered->value() == index, "stable consecutive indices"));
        static_cast<void>(suite.check(legacyEncoding.canRepresent(*registered) ==
                                          (index < legacyFirstUnrepresentableIndex),
                                      "storage capacity is independent of wire width"));
    }

    const auto overflow = catalog.intern("models/overflow.mdl");
    static_cast<void>(suite.check(!overflow && overflow.error() == ResourceError::capacityReached,
                                  "reject overflow explicitly"));
    static_cast<void>(
        suite.check(catalog.size() == testLimits.entryCapacity(), "overflow preserves catalog"));
    static_cast<void>(suite.check(catalog.intern("models/example1.mdl")->value() == 1,
                                  "deduplicate even when full"));

    const auto resourceListEncoding = IndexEncoding::create(12).value();
    static_cast<void>(suite.check(resourceListEncoding.canRepresent(4095), "12-bit upper bound"));
    static_cast<void>(
        suite.check(!resourceListEncoding.canRepresent(4096), "12 bits cannot encode count 4096"));
}

void testPathsAndOwnership(TestSuite &suite) {
    ResourceCatalog<ResourceKind::generic> catalog{testLimits};
    constexpr auto invalidPaths = std::array{
        ""sv,   "/file"sv,   "../file"sv, "a/../file"sv, "./file"sv,  "a/./file"sv, "a//file"sv,
        "a/"sv, "C:/file"sv, "a\\file"sv, "a\0file"sv,   "a\nfile"sv, "a\x7F"sv,
    };
    for (const auto path : invalidPaths) {
        const auto result = catalog.intern(path);
        static_cast<void>(suite.check(!result && result.error() == ResourceError::invalidPath,
                                      "reject noncanonical path"));
    }
    static_cast<void>(suite.check(catalog.size() == 0, "invalid paths consume no indices"));

    const auto exactPath = std::string(legacyPathByteLimit, 'a');
    static_cast<void>(
        suite.check(catalog.intern(exactPath).has_value(), "accept exact byte limit"));
    const auto tooLong = catalog.intern(exactPath + 'a');
    static_cast<void>(suite.check(!tooLong && tooLong.error() == ResourceError::pathTooLong,
                                  "reject path beyond byte limit"));

    auto original = std::string{"resources/owned.dat"};
    const auto owned = catalog.intern(original);
    original.assign("resources/changed.dat");
    static_cast<void>(suite.check(owned && catalog.find("resources/owned.dat") == *owned,
                                  "catalog owns a copy of its key"));
    static_cast<void>(suite.check(!catalog.find(original), "caller mutation cannot change key"));

    const auto lower = catalog.intern("resources/case.dat");
    const auto upper = catalog.intern("resources/Case.dat");
    static_cast<void>(
        suite.check(lower && upper && *lower != *upper, "do not impose engine case normalization"));
}

void testFreeze(TestSuite &suite) {
    ResourceCatalog<ResourceKind::sound> catalog{testLimits};
    const auto existing = catalog.intern("weapons/example.wav");
    catalog.freeze();
    catalog.freeze();
    static_cast<void>(suite.check(catalog.intern("weapons/example.wav") == existing,
                                  "resolve existing entry after freeze"));
    const auto newEntry = catalog.intern("weapons/other.wav");
    static_cast<void>(suite.check(!newEntry && newEntry.error() == ResourceError::catalogFrozen,
                                  "freeze forbids new resources"));
    static_cast<void>(suite.check(catalog.size() == 1, "freeze preserves entries"));

    ResourceCatalog<ResourceKind::sound> emptyCatalog{testLimits};
    emptyCatalog.freeze();
    const auto rejected = emptyCatalog.intern("weapons/example.wav");
    static_cast<void>(suite.check(!rejected && rejected.error() == ResourceError::catalogFrozen,
                                  "freeze empty catalog"));
}

} // namespace

auto main() -> int {
    TestSuite suite;
    testCategory<ResourceKind::model>(suite);
    testCategory<ResourceKind::sound>(suite);
    testCategory<ResourceKind::generic>(suite);
    testCategory<ResourceKind::event>(suite);
    testCategory<ResourceKind::decal>(suite);
    testCapacityAndEncoding(suite);
    testPathsAndOwnership(suite);
    testFreeze(suite);
    return suite.finish();
}
