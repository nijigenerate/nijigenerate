#include "face.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <cmath>
#include <limits>
#include <array>
#include <bit>
#include <string_view>
namespace nijigenerate::autorig::deterministic::face {
using namespace solver::quadratic;
namespace {
template<std::size_t N> void require(bool ok, const char (&message)[N], std::size_t sourceLine) {
    if (!ok) throw compat::ThrowableException(contracts::detail::copyContractMessage(std::string_view(message, N - 1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/face.d"), sourceLine);
}
std::int32_t nextRow(QuadraticProblem& problem) {
    const auto old = problem.constraints;
    problem.constraints = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(old) + 1u);
    return old;
}
double cross(RigPoint a, RigPoint b) { return a[0] * b[1] - a[1] * b[0]; }
RigPoint difference(RigPoint a, RigPoint b) { return {a[0] - b[0], a[1] - b[1]}; }
void checkAxes(compat::Slice<double> xs, compat::Slice<double> ys, std::size_t count) {
    require(xs.size() >= 2 && ys.size() >= 2 && count == xs.size() * ys.size()
        && count <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()), "Invalid face grid dimensions", 21);
    for (auto axis : compat::Slice<compat::Slice<double>>{xs, ys}) for (std::size_t i = 0; i < axis.size(); ++i)
        require(std::isfinite(axis[i]) && (i == 0 || axis[i] > axis[i - 1]), "Grid axes must increase strictly", 24);
}
double constraints(compat::Slice<RigPoint> points, compat::Slice<double> xs, compat::Slice<double> ys,
    RigPoint direction, QuadraticProblem& problem) {
    double minimum = std::numeric_limits<double>::infinity();
    for (std::size_t j = 0; j < ys.size() - 1; ++j) for (std::size_t i = 0; i < xs.size() - 1; ++i) {
        auto a = static_cast<std::int32_t>(j * xs.size() + i), b = a + 1;
        auto c = a + static_cast<std::int32_t>(xs.size()), d = c + 1;
        double area = (xs[i + 1] - xs[i]) * (ys[j + 1] - ys[j]);
        const compat::Slice<compat::Slice<std::int32_t>> edgeSets{
            {a, b, a, c}, {c, d, b, d}, {a, b, b, d}, {c, d, a, c}};
        for (auto edges : edgeSets) {
            auto e = difference(points[edges[1]], points[edges[0]]), f = difference(points[edges[3]], points[edges[2]]);
            double ratio = cross(e, f) / area; if (ratio < minimum) minimum = ratio;
            auto row = nextRow(problem);
            double ce = cross(direction, f) / area, cf = cross(e, direction) / area;
            problem.matrix.append(compat::Slice<SparseEntry>{
                {row, edges[1], -ce}, {row, edges[0], ce}, {row, edges[3], -cf}, {row, edges[2], cf}});
            problem.lower.append(-std::numeric_limits<double>::infinity()); problem.upper.append(ratio - .055);
        }
    }
    return minimum;
}
}
FaceProjection ngPreserveFaceOrientation(compat::Slice<RigPoint> points, compat::Slice<double> xs,
    compat::Slice<double> ys, compat::Slice<RigPoint> uv, RigPoint direction, double width,
    compat::Slice<bool> protectedVertices, CancellationHandler canceled) {
    checkAxes(xs, ys, points.size());
    require(uv.size() == points.size() && (protectedVertices.empty() || protectedVertices.size() == points.size()),
        "Face vertex metadata dimensions differ", 53);
    require(std::isfinite(width) && width > 0, "Invalid face width", 55);
    for (auto point : points) for (double v : point) require(std::isfinite(v), "Nonfinite face point", 56);
    for (auto point : uv) for (double v : point) require(std::isfinite(v), "Nonfinite face UV", 57);
    for (double v : direction) require(std::isfinite(v), "Nonfinite depth direction", 58);
    FaceProjection result; result.points = points.duplicate();
    double norm = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1]);
    if (norm < 1e-9) return result; for (auto& v : direction) v /= norm;
    QuadraticProblem problem; problem.variables = static_cast<std::int32_t>(points.size());
    problem.linear.resize(points.size(), 0.);
    result.rawMinimum = constraints(points, xs, ys, direction, problem); result.correctedMinimum = result.rawMinimum;
    if (result.rawMinimum >= .055) return result;
    for (std::int32_t a = 0; a < problem.variables; ++a) problem.objective.append(SparseEntry{a, a, 1});
    for (std::size_t j = 0; j < ys.size(); ++j) for (std::size_t i = 0; i < xs.size(); ++i) {
        auto a = static_cast<std::int32_t>(j * xs.size() + i);
        auto edge = [&](std::int32_t b, double distance) {
            double weight = width * .04 / distance; weight *= weight;
            problem.objective.append(compat::Slice<SparseEntry>{{a, a, weight}, {b, b, weight}, {a, b, -weight}});
        };
        if (i + 1 < xs.size()) edge(a + 1, xs[i + 1] - xs[i]);
        if (j + 1 < ys.size()) edge(a + static_cast<std::int32_t>(xs.size()), ys[j + 1] - ys[j]);
    }
    for (std::size_t i = 0; i < uv.size(); ++i) {
        auto p = uv[i]; bool allowed = (p[0] < .3 || p[0] > .7 || p[1] < .2 || p[1] > .8)
            && (protectedVertices.empty() || !protectedVertices[i]);
        double bound = allowed ? width * .04 : 0;
        problem.matrix.append(SparseEntry{nextRow(problem), static_cast<std::int32_t>(i), 1});
        problem.lower.append(-bound); problem.upper.append(bound);
    }
    result.solver = ngSolveQuadratic(problem, SolverOptions{}, canceled);
    require(result.solver.solved(), "No bounded contour projection preserving depth", 92);
    require(result.solver.constraintViolation <= 1e-5, "Contour solution violates bounds", 93);
    for (std::size_t i = 0; i < result.solver.solution.size(); ++i) {
        double s = result.solver.solution[i]; result.points[i][0] += s * direction[0]; result.points[i][1] += s * direction[1];
        if (std::abs(s) > result.maximumCorrection) result.maximumCorrection = std::abs(s);
    }
    QuadraticProblem verification; result.correctedMinimum = constraints(result.points, xs, ys, direction, verification);
    require(result.correctedMinimum >= .0549, "Contour projection failed Jacobian constraints", 101); return result;
}
compat::Slice<double> ngFitGridDepth(compat::Slice<double> xs, compat::Slice<double> ys, compat::Slice<double> base,
    compat::Slice<RigPoint> query, compat::Slice<double> truth, double tolerance, std::size_t anchorCount,
    CancellationHandler canceled) {
    checkAxes(xs, ys, base.size());
    require(query.size() == truth.size() && anchorCount <= query.size() && tolerance > 0 && std::isfinite(tolerance),
        "Invalid depth fitting inputs", 109);
    auto n = static_cast<std::int32_t>(base.size());
    require(n <= std::numeric_limits<std::int32_t>::max() / 2, "Depth grid too large", 112);
    double limit = tolerance * .98; QuadraticProblem problem; problem.variables = n * 2;
    problem.linear.resize(static_cast<std::size_t>(n) * 2, 0.);
    for (std::int32_t k = 0; k < n; ++k) {
        require(std::isfinite(base[k]) && base[k] >= 0, "Invalid base depth", 119); problem.linear[n + k] = 1;
        auto row = nextRow(problem); problem.matrix.append(SparseEntry{row, k, 1});
        problem.lower.append(-base[k] > -limit ? -base[k] : -limit); problem.upper.append(limit);
        row = nextRow(problem); problem.matrix.append(SparseEntry{row, n + k, 1});
        problem.lower.append(0); problem.upper.append(std::numeric_limits<double>::infinity());
        for (double sign : compat::Slice<double>{-1., 1.}) {
            row = nextRow(problem);
            problem.matrix.append(compat::Slice<SparseEntry>{{row, k, sign}, {row, n + k, -1}});
            problem.lower.append(-std::numeric_limits<double>::infinity()); problem.upper.append(0);
        }
    }
    for (std::size_t k = 0; k < query.size(); ++k) {
        auto point = query[k];
        require(std::isfinite(point[0]) && std::isfinite(point[1]) && std::isfinite(truth[k]),
            "Nonfinite depth sample", 137);
        std::size_t i = 0, j = 0;
        while (i + 2 < xs.size() && point[0] > xs[i + 1]) ++i;
        while (j + 2 < ys.size() && point[1] > ys[j + 1]) ++j;
        double u = (point[0] - xs[i]) / (xs[i + 1] - xs[i]), v = (point[1] - ys[j]) / (ys[j + 1] - ys[j]);
        auto a = static_cast<std::int32_t>(j * xs.size() + i);
        std::array indices{a, a + 1, a + static_cast<std::int32_t>(xs.size()), a + static_cast<std::int32_t>(xs.size()) + 1};
        std::array weights{(1 - u) * (1 - v), u * (1 - v), (1 - u) * v, u * v};
        double residual = truth[k]; auto row = nextRow(problem);
        for (std::size_t h = 0; h < 4; ++h) {
            residual -= weights[h] * base[indices[h]]; problem.matrix.append(SparseEntry{row, indices[h], weights[h]});
        }
        bool anchor = k >= query.size() - anchorCount;
        problem.lower.append(residual - (anchor ? 0 : limit)); problem.upper.append(residual + (anchor ? 0 : limit));
    }
    auto result = ngSolveQuadratic(problem, SolverOptions{}, canceled);
    if (!result.solved() || result.constraintViolation > 1e-5) return {};
    auto fitted = base.duplicate(); for (std::int32_t k = 0; k < n; ++k) fitted[k] += result.solution[k]; return fitted;
}
}
