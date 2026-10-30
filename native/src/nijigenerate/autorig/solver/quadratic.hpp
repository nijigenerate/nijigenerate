#pragma once
#include "quadratic_types.hpp"
#include <nijigenerate/compat/shared_delegate.hpp>

namespace nijigenerate::autorig::solver::quadratic {
using CancellationHandler = compat::SharedDelegate<bool()>;

SolverResult ngSolveQuadratic(QuadraticProblem problem, SolverOptions options = {},
    CancellationHandler canceled = {});
}
