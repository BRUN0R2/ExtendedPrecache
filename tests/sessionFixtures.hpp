#pragma once

#include <extendedPrecache/session/configuration.hpp>
#include <extendedPrecache/session/frameCodec.hpp>

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace extendedPrecache::tests {
using FixtureJson = nlohmann::json;
using namespace extendedPrecache::session;

inline auto readText(const std::filesystem::path &path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Cannot read fixture " + path.string()};
    }
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
inline auto readFixture(const std::filesystem::path &path) -> FixtureJson {
    return FixtureJson::parse(readText(path));
}
inline auto fixtureVector(const FixtureJson &object) -> CapabilityVector {
    std::array<std::uint32_t, capabilityCount> values{};
    for (std::size_t index = 0; index < capabilityCount; ++index) {
        values[index] = object.at(capabilityNames[index]).get<std::uint32_t>();
    }
    return CapabilityVector{values};
}
struct FixtureChanges final {
    const FixtureJson &values;
};
inline auto changedFixture(FixtureJson input, FixtureChanges changes) -> FixtureJson {
    for (const auto &change : changes.values) {
        const auto remove = change.contains("remove");
        const auto path = change.at(remove ? "remove" : "path").get<std::vector<std::string>>();
        auto *parent = &input;
        for (std::size_t index = 0; index + 1 < path.size(); ++index) {
            parent = &parent->at(path[index]);
        }
        if (remove) {
            parent->erase(path.back());
        } else {
            (*parent)[path.back()] = change.at("value");
        }
    }
    return input;
}
inline auto fixtureRejection(std::string_view name) -> RejectCode {
    constexpr auto names = std::array<std::string_view, 14>{
        "malformedFrame",       "unsupportedVersion",  "unexpectedMessage",
        "unexpectedSequence",   "unsupportedContract", "capacityInsufficient",
        "invalidConfiguration", "storageUnavailable",  "transcriptMismatch",
        "deadlineExceeded",     "mapChanged",          "legacyIncompatible",
        "transportUnavailable", "budgetExceeded"};
    const auto found = std::ranges::find(names, name);
    if (found == names.end()) {
        throw std::runtime_error{"Unknown rejection fixture"};
    }
    return static_cast<RejectCode>(std::distance(names.begin(), found) + 1);
}
template <typename T> auto fixtureIdentity(std::string_view hex) -> T {
    const auto bytes = decodeHex(hex).value();
    T result;
    if (bytes.size() != result.bytes.size()) {
        throw std::runtime_error{"Invalid fixture identity"};
    }
    std::ranges::copy(bytes, result.bytes.begin());
    return result;
}
} // namespace extendedPrecache::tests
