#pragma once

#include "contracts.hpp"
#include <nijigenerate/autorig/solver/quadratic_types.hpp>
#include <limits>

namespace nijigenerate::autorig::deterministic::controls_detail {

// Real pre-solver data only; this is not the public projection's completed result.
struct ControlOrientationInput {
    compat::Slice<double> originalOffsets;
    double minimum = std::numeric_limits<double>::infinity();
    bool requiresSolve = false;
    solver::quadratic::QuadraticProblem problem;
    solver::quadratic::SolverOptions options;
};

ControlOrientationInput prepareControlOrientation(compat::Slice<contracts::Point2> rest,
    core::settings::SettingsJsonValue triangles, compat::Slice<double> offsets);

// The original post-solver body; the caller must provide the actual solver result.
compat::Slice<double> projectSolvedOrientation(compat::Slice<contracts::Point2> rest,
    compat::Slice<double> offsets, solver::quadratic::SolverResult result);

// The public body calls the actual context, solver and canonical imported checkpoint.

} // namespace nijigenerate::autorig::deterministic::controls_detail
