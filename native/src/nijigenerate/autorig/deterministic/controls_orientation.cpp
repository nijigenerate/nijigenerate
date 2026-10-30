#include "controls_orientation.hpp"
#include "controls.hpp"
#include "controls_dependencies.hpp"
#include <nijigenerate/autorig/solver/quadratic.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <string_view>

namespace nijigenerate::autorig::deterministic::controls_detail {
namespace {

template<std::size_t N>
void enforce(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) throw compat::ThrowableException(contracts::detail::copyContractMessage(std::string_view(message,N-1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), line);
}

std::int32_t sourceInt(std::size_t value) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

} // namespace

ControlOrientationInput prepareControlOrientation(compat::Slice<contracts::Point2> rest,
    core::settings::SettingsJsonValue triangles, compat::Slice<double> offsets) {
    enforce(offsets.size() == rest.size() * 2, "Local control vertex count mismatch", 139);
    struct Constraint {
        std::array<std::size_t, 3> indices{};
        contracts::Point3 coefficients;
    };
    compat::Slice<Constraint> constraints;
    ControlOrientationInput prepared;
    prepared.originalOffsets = offsets;
    for (const auto triangle : triangles.as_array()) {
        const auto values = contracts::ngRigNumbers(triangle);
        enforce(values.size() == 3, "Invalid native control triangle", 145);
        std::array<std::size_t, 3> indices{};
        for (std::size_t i = 0; i < 3; ++i) {
            const double value = values[i];
            enforce(value >= 0 && value < rest.size() && value == static_cast<std::size_t>(value),
                "Control triangle index out of range", 148);
            indices[i] = static_cast<std::size_t>(value);
        }
        const auto first = rest[indices[0]];
        const auto second = rest[indices[1]];
        const auto third = rest[indices[2]];
        const double before = (second[0] - first[0]) * (third[1] - first[1]) -
            (second[1] - first[1]) * (third[0] - first[0]);
        if (before == 0) continue;
        const double xa = first[0] + offsets[indices[0] * 2];
        const double xb = second[0] + offsets[indices[1] * 2];
        const double xc = third[0] + offsets[indices[2] * 2];
        const contracts::Point3 coefficients{(xc - xb) / before, (xa - xc) / before, (xb - xa) / before};
        double ratio = 0;
        for (std::size_t i = 0; i < 3; ++i)
            ratio += coefficients[i] * (rest[indices[i]][1] + offsets[indices[i] * 2 + 1]);
        if (ratio < prepared.minimum) prepared.minimum = ratio;
        constraints.append(Constraint{indices, coefficients});
    }
    if (prepared.minimum >= .002) return prepared;
    prepared.requiresSolve = true;
    auto& problem = prepared.problem;
    problem.variables = sourceInt(rest.size());
    problem.constraints = sourceInt(constraints.size());
    for (std::size_t i = 0; i < rest.size(); ++i) {
        problem.objective.append(solver::quadratic::SparseEntry{sourceInt(i), sourceInt(i), 1});
        problem.linear.append(-(rest[i][1] + offsets[i * 2 + 1]));
    }
    for (std::size_t row = 0; row < constraints.size(); ++row) {
        const auto constraint = constraints[row];
        for (std::size_t i = 0; i < 3; ++i)
            problem.matrix.append(solver::quadratic::SparseEntry{
                sourceInt(row), sourceInt(constraint.indices[i]), constraint.coefficients[i]});
        problem.lower.append(.004);
        problem.upper.append(std::numeric_limits<double>::infinity());
    }
    prepared.options = solver::quadratic::SolverOptions{30'000, 1e-8, 30, true};
    return prepared;
}

compat::Slice<double> projectSolvedOrientation(compat::Slice<contracts::Point2> rest,
    compat::Slice<double> offsets, solver::quadratic::SolverResult result) {
    enforce(result.solved() && result.constraintViolation < 1e-6,
        "Native control orientation projection failed", 177);
    auto projected = offsets.dup();
    for (std::size_t i = 0; i < rest.size(); ++i)
        projected[i * 2 + 1] = result.solution[i] - rest[i][1];
    return projected;
}

} // namespace nijigenerate::autorig::deterministic::controls_detail

namespace nijigenerate::autorig::deterministic::controls {

compat::Slice<double> ngRigProjectControlOrientation(compat::Slice<contracts::Point2> rest,
    core::settings::SettingsJsonValue triangles, compat::Slice<double> offsets,
    std::shared_ptr<framework::AutoRigTaskContext> context) {
    auto prepared = controls_detail::prepareControlOrientation(rest, triangles, offsets);
    if (!prepared.requiresSolve) return offsets;
    contracts::ngRigCheckpoint(context);
    auto result = solver::quadratic::ngSolveQuadratic(prepared.problem, prepared.options,
        [context] { return context && context->isCanceled(); });
    contracts::ngRigCheckpoint(context);
    return controls_detail::projectSolvedOrientation(rest, offsets, result);
}

} // namespace nijigenerate::autorig::deterministic::controls
