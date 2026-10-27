#pragma once
#include "binary.hpp"
#include <extendedPrecache/data/application.hpp>
#include <nlohmann/json.hpp>

namespace extendedPrecache::data::detail {
using Json = nlohmann::json;
auto parseApplication(std::string_view document) -> ApplicationDefinition;
void validateApplicationDefinition(const ApplicationDefinition &definition,
                                   const DigestProvider &provider);
auto parseModules(const Json &root) -> std::vector<ModuleProfile>;
auto parseEffects(const Json &root) -> std::vector<EffectPolicy>;
auto parseServices(const Json &root) -> std::vector<ServicePolicy>;
void closed(const Json &object, std::initializer_list<std::string_view> keys);
auto number(const Json &object, std::string_view key, std::uint64_t minimum, std::uint64_t maximum)
    -> std::uint64_t;
auto text(const Json &object, std::string_view key) -> const std::string &;
auto boolean(const Json &object, std::string_view key) -> bool;
auto digestValue(const Json &object, std::string_view key) -> Digest;
auto list(const Json &object, std::string_view key, std::size_t maximum = 65535) -> const Json &;
auto deliveries(const Json &object) -> Deliveries;
template <typename Enum, std::size_t size>
auto named(const Json &object, std::string_view key,
           const std::array<std::string_view, size> &names) -> Enum {
    const auto &value = text(object, key);
    const auto found = std::ranges::find(names, value);
    require(found != names.end(), ErrorCode::invalidSchema);
    return static_cast<Enum>(found - names.begin());
}
inline constexpr auto directionNames = std::array<std::string_view, 2>{"S2C", "C2S"};
inline constexpr auto resourcePolicyNames =
    std::array<std::string_view, 3>{"typedReferences", "resourceFree", "boundedLegacy"};
} // namespace extendedPrecache::data::detail
