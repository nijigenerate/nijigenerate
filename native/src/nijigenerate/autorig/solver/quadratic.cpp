#include <nijigenerate/autorig/solver/quadratic.hpp>
#include <nijigenerate/autorig/solver/quadratic_sparse.hpp>
#include <nijigenerate/autorig/solver/osqp_abi.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace nijigenerate::autorig::solver::quadratic {
namespace {
double& constraintElement(compat::Slice<double>& values, std::size_t index) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/solver/quadratic.d"), 121);
    return values.data()[index];
}
}
SolverResult ngSolveQuadratic(QuadraticProblem problem, SolverOptions options, CancellationHandler canceled) {
    using detail::enforce;
    enforce(problem.variables > 0 && problem.constraints >= 0, "Invalid optimization dimensions",85);
    enforce(problem.linear.size() == static_cast<std::size_t>(problem.variables) &&
        problem.lower.size() == static_cast<std::size_t>(problem.constraints) &&
        problem.upper.size() == static_cast<std::size_t>(problem.constraints),
        "Optimization vector dimensions differ",86);
    enforce(options.iterations > 0 && std::isfinite(options.tolerance) && options.tolerance > 0 &&
        std::isfinite(options.seconds) && options.seconds > 0, "Invalid solver options",88);
    for (const auto value : problem.linear) enforce(std::isfinite(value), "Nonfinite linear objective",90);
    auto lower = problem.lower.duplicate();
    auto upper = problem.upper.duplicate();
    constexpr auto infinity = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < lower.size(); ++i) {
        enforce(lower[i] <= upper[i] && lower[i] != infinity && upper[i] != -infinity,
            "Invalid optimization bounds",94);
        if (lower[i] == -infinity) lower[i] = -1e30;
        if (upper[i] == infinity) upper[i] = 1e30;
    }
    auto p = detail::compress(problem.objective, problem.variables, problem.variables, true);
    auto a = detail::compress(problem.matrix, problem.constraints, problem.variables, false);
    enforce(!canceled || !canceled(), "Optimization canceled",101);
    if (core::tasks::TaskFiber::hasCurrent()) core::tasks::TaskFiber::yield();
    enforce(!canceled || !canceled(), "Optimization canceled",103);
    SolverResult result;
    result.solution.resize(static_cast<std::size_t>(problem.variables), std::numeric_limits<double>::quiet_NaN());
    std::array<double, 3> diagnostics;
    diagnostics.fill(std::numeric_limits<double>::quiet_NaN());
    const auto error = ng_osqp_solve(problem.variables, problem.constraints,
        p.pointers.data(), p.rows.data(), p.values.data(), static_cast<int>(p.values.size()),
        a.pointers.data(), a.rows.data(), a.values.data(), static_cast<int>(a.values.size()),
        problem.linear.data(), lower.data(), upper.data(), options.iterations, options.tolerance,
        options.seconds, options.polish, result.solution.data(), diagnostics.data(),
        &result.status, &result.iterations);
    enforce(error == 0, "OSQP setup or solve failed",112);
    enforce(!canceled || !canceled(), "Optimization canceled",113);
    result.primalResidual = diagnostics[0];
    result.dualResidual = diagnostics[1];
    result.objective = diagnostics[2];
    if (!result.solved()) { result.solution.clear(); return result; }
    for (const auto value : result.solution) enforce(std::isfinite(value), "OSQP returned nonfinite solution",118);
    compat::Slice<double> actual;
    actual.resize(static_cast<std::size_t>(problem.constraints), std::numeric_limits<double>::quiet_NaN());
    std::fill(actual.begin(), actual.end(), 0.0);
    for (const auto entry : problem.matrix) {
        auto& destination = constraintElement(actual, static_cast<std::size_t>(entry.row));
        const double contribution = entry.value * constraintElement(
            result.solution, static_cast<std::size_t>(entry.column));
        destination += contribution;
    }
    for (std::size_t i = 0; i < actual.size(); ++i) {
        enforce(std::isfinite(actual[i]), "Constraint evaluation overflow",123);
        auto violation = problem.lower[i] - actual[i];
        if (actual[i] - problem.upper[i] > violation) violation = actual[i] - problem.upper[i];
        if (violation > result.constraintViolation) result.constraintViolation = violation;
    }
    return result;
}
}
