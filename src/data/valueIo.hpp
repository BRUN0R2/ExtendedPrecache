#pragma once
#include "binary.hpp"
#include <extendedPrecache/data/schema.hpp>

namespace extendedPrecache::data::detail {
void writeField(Writer &writer, const FieldDefinition &field, const FieldValue &value,
                ValueContext context);
auto readField(Reader &reader, const FieldDefinition &field, ValueContext context,
               AllocationBudget &allocation) -> FieldValue;
} // namespace extendedPrecache::data::detail
