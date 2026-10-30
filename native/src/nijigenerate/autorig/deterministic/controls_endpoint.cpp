#include "controls_dependencies.hpp"
#include "linear.hpp"

namespace nijigenerate::autorig::deterministic::controls_detail {
compat::Slice<double> endpointCubic(compat::Slice<double> values) {
    linear::RigMatrix design, residual;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double t = static_cast<double>(i) / (values.size() - 1);
        design.append(linear::RigRow{t * (1 - t), t * (1 - t) * (2 * t - 1)});
        residual.append(linear::RigRow{values[i] - values[0] * (1 - t) - values[values.size() - 1] * t});
    }
    const auto coefficients = linear::ngRigLeastSquares(design, residual);
    compat::Slice<double> result;
    for (std::size_t i = 0; i < design.size(); ++i) {
        const double t = static_cast<double>(i) / (values.size() - 1);
        const auto row = design[i];
        result.append(values[0] * (1 - t) + values[values.size() - 1] * t +
            row[0] * coefficients[0][0] + row[1] * coefficients[1][0]);
    }
    return result;
}
}
