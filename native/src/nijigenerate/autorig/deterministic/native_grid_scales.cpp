#include "native_grid_scales.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace nijigenerate::autorig::deterministic::native_detail {

compat::Slice<float> advanced(compat::Slice<double> desired, compat::Slice<double> bounds) {
    double largest = std::max(1., std::max(std::abs(bounds[0]), std::abs(bounds[1])));
    for (const double value : desired) largest = std::max(largest, std::abs(value));
    const auto rounding = 8 * std::numeric_limits<float>::epsilon() * largest;
    compat::Slice<double> nativeValues;
    compat::Slice<float> scales;
    for (const double value : desired) {
        if (!nativeValues.empty() && value - nativeValues[nativeValues.size() - 1] <=
            std::max(1e-4, 1e-4 * std::max(std::abs(value),
                std::abs(nativeValues[nativeValues.size() - 1]))) + rounding)
            continue;
        nativeValues.append(value);
        // Keep the original cast and evaluation order, without a new range guard.
        scales.append(static_cast<float>((value - bounds[0]) / (bounds[1] - bounds[0])));
    }
    return scales;
}

} // namespace nijigenerate::autorig::deterministic::native_detail
