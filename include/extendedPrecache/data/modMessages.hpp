#pragma once
#include <extendedPrecache/data/application.hpp>

namespace extendedPrecache::data {
struct ModMessage final {
    ModuleId owner;
    ModMessageId id;
    SchemaId schema;
    ValueSet values;
    auto operator==(const ModMessage &) const -> bool = default;
};
struct ModContext final {
    const ApplicationContract &application;
    ValueContext values;
    Direction direction;
    Delivery delivery;
    Phase phase;
    RecordBudget budget;
};
[[nodiscard]] auto encodeModMessage(const ModMessage &message, ModContext context) noexcept
    -> Result<Bytes>;
[[nodiscard]] auto decodeModMessage(std::span<const std::byte> bytes, ModContext context) noexcept
    -> Result<ModMessage>;
class NativeMessageValidator {
  public:
    virtual ~NativeMessageValidator() = default;
    [[nodiscard]] virtual auto validate(ModuleId module, const MessageDefinition &message,
                                        std::span<const std::byte> payload,
                                        const ReferenceValidator &resources) const noexcept
        -> Result<void> = 0;
};
[[nodiscard]] auto validateNativeMessage(ModuleId module, ModMessageId message,
                                         std::span<const std::byte> payload, ModContext context,
                                         const NativeMessageValidator &validator) noexcept
    -> Result<void>;
} // namespace extendedPrecache::data
