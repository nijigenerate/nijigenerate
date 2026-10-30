#include "nijigenerate/autorig/deterministic/detail/geometry_axis.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include "contracts_file.hpp"
#include <cmath>
#include <cstddef>
#include <string_view>

namespace nijigenerate::autorig::deterministic::geometry::detail {
void lines(compat::Slice<double> axis) {
    if (!(axis.size() >= 2 && axis[0] == 0 && axis[axis.size() - 1] == 1))
        throw compat::ThrowableException{contracts::detail::copyContractMessage("Grid axis must span [0,1]"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"), 124};
    for (std::size_t i = 0; i < axis.size(); ++i) {
        const auto value = axis[i];
        if (!(std::isfinite(value) && (i == 0 || value > axis[i - 1])))
            throw compat::ThrowableException{contracts::detail::copyContractMessage("Unordered grid axis"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"), 125};
    }
}
}
