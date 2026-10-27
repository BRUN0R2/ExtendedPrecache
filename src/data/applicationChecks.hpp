#pragma once
#include <extendedPrecache/data/application.hpp>

namespace extendedPrecache::data::detail {
void validateServiceSizes(const ApplicationContract &contract,
                          const session::CapabilityLimits &selected);
}
