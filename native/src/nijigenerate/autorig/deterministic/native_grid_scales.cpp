#include "native_grid_scales.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace nijigenerate::autorig::deterministic::native_detail {

compat::Slice<float> advanced(compat::Slice<double> desired, compat::Slice<double> bounds, bool headSurface) {
    double largest = std::max(1., std::max(std::abs(bounds[0]), std::abs(bounds[1])));
    for (const double value : desired) largest = std::max(largest, std::abs(value));
    const auto rounding = 8 * std::numeric_limits<float>::epsilon() * largest;
    // Anatomical landmarks can lie very close to an ordinary sampling knot.
    // Keep the grid from acquiring sliver cells at those coincidences.
    const double minSpacing = !headSurface || desired.size() < 2 ? 1e-4 : std::max(1e-4,
        (desired[desired.size()-1] - desired[0]) / (desired.size()-1) * .12);
    compat::Slice<double> nativeValues;
    compat::Slice<float> scales;
    for (const double value : desired) {
        if (!nativeValues.empty() && value - nativeValues[nativeValues.size() - 1] <=
            std::max(minSpacing, 1e-4 * std::max(std::abs(value),
                std::abs(nativeValues[nativeValues.size() - 1]))) + rounding) {
            if (headSurface && value == desired[desired.size()-1] && scales.size() > 1)
                scales[scales.size()-1] = static_cast<float>((value - bounds[0]) / (bounds[1] - bounds[0]));
            continue;
        }
        nativeValues.append(value);
        // Keep the original cast and evaluation order, without a new range guard.
        scales.append(static_cast<float>((value - bounds[0]) / (bounds[1] - bounds[0])));
    }
    return scales;
}

} // namespace nijigenerate::autorig::deterministic::native_detail
