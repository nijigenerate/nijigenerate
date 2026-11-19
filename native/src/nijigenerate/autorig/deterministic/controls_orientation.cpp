#include "controls_orientation.hpp"
#include "controls.hpp"
#include "controls_dependencies.hpp"
#include "controls_skin.hpp"
#include <nijigenerate/autorig/solver/quadratic.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <string_view>
#include <string>
#include <vector>
#include <algorithm>
#include <set>

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
    core::settings::SettingsJsonValue triangles, compat::Slice<double> offsets, double minimumRatio) {
    enforce(offsets.size() == rest.size() * 2, "Local control vertex count mismatch", 139);
    struct Constraint {
        std::array<std::size_t, 3> indices{};
        contracts::Point3 coefficients;
        double ratio;
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
        constraints.append(Constraint{indices, coefficients, ratio});
    }
    if (prepared.minimum >= minimumRatio / 2) return prepared;
    prepared.requiresSolve = true;
    auto& problem = prepared.problem;
    problem.variables = sourceInt(rest.size());
    problem.constraints = sourceInt(constraints.size());
    for (std::size_t i = 0; i < rest.size(); ++i) {
        problem.objective.append(solver::quadratic::SparseEntry{sourceInt(i), sourceInt(i), 1});
        // Solve a correction to the requested pose, not absolute mesh coordinates.
        // This is the same least-squares objective without a large translation.
        problem.linear.append(0);
    }
    for (std::size_t row = 0; row < constraints.size(); ++row) {
        const auto constraint = constraints[row];
        double norm = 0;
        for (auto coefficient : constraint.coefficients) norm += coefficient * coefficient;
        norm = std::sqrt(norm);
        if (!(norm > 0)) {
            prepared.requiresPlanar = true;
            return prepared;
        }
        for (std::size_t i = 0; i < 3; ++i)
            problem.matrix.append(solver::quadratic::SparseEntry{
                sourceInt(row), sourceInt(constraint.indices[i]), constraint.coefficients[i] / norm});
        problem.lower.append((minimumRatio - constraint.ratio) / norm);
        problem.upper.append(std::numeric_limits<double>::infinity());
    }
    prepared.options = solver::quadratic::SolverOptions{30'000, 1e-8, 30, true};
    return prepared;
}

compat::Slice<double> projectSolvedOrientation(compat::Slice<contracts::Point2> rest,
    compat::Slice<double> offsets, solver::quadratic::SolverResult result) {
    if (!(result.solved() && result.constraintViolation < 1e-6)) {
        const auto message = std::string("Native control orientation projection failed: status=") +
            std::to_string(result.status) + " violation=" + std::to_string(result.constraintViolation);
        throw compat::ThrowableException(contracts::detail::copyContractMessage(message),
            compat::RetainedString::fromStaticLiteral("native/autorig/controls_orientation.cpp"),177);
    }
    auto projected = offsets.duplicate();
    for (std::size_t i = 0; i < rest.size(); ++i)
        projected[i * 2 + 1] += result.solution[i];
    return projected;
}

} // namespace nijigenerate::autorig::deterministic::controls_detail

namespace nijigenerate::autorig::deterministic::controls {

namespace {
// Penalize differences per unit rest distance, so dense triangles cannot
// concentrate a repair into a spike on the drawn contour.
void regularizeOrientation(solver::quadratic::QuadraticProblem& problem,
    compat::Slice<contracts::Point2> rest, core::settings::SettingsJsonValue triangles,
    double length, std::size_t dimensions, compat::Slice<double> current = {},
    compat::Slice<double> requested = {}) {
    if (length <= 0) return;
    std::set<std::pair<std::size_t, std::size_t>> edges;
    for (auto triangle : triangles.as_array()) {
        const auto indices = contracts::ngRigNumbers(triangle);
        for (std::size_t i = 0; i < 3; ++i) {
            const auto a = std::size_t(indices[i]), b = std::size_t(indices[(i+1)%3]);
            if (a != b) edges.emplace(std::min(a,b), std::max(a,b));
        }
    }
    for (const auto& [a,b] : edges) {
        const double distance = std::hypot(rest[a][0]-rest[b][0], rest[a][1]-rest[b][1]);
        const double ratio = length / std::max(distance, length * .05);
        const double weight = ratio * ratio;
        for (std::size_t axis = 0; axis < dimensions; ++axis) {
            const auto i = static_cast<std::int32_t>(a*dimensions+axis);
            const auto j = static_cast<std::int32_t>(b*dimensions+axis);
            problem.objective.append({i,i,weight});
            problem.objective.append({j,j,weight});
            problem.objective.append({i,j,-weight});
            if (!current.empty()) {
                const double difference = (current[i]-requested[i])-(current[j]-requested[j]);
                problem.linear[i] += weight*difference;
                problem.linear[j] -= weight*difference;
            }
        }
    }
}

compat::Slice<double> projectPlanarOrientation(compat::Slice<contracts::Point2> rest,
    core::settings::SettingsJsonValue triangles, compat::Slice<double> requested,
    compat::Slice<contracts::Point2> anchors, double minimumRatio,
    std::shared_ptr<framework::AutoRigTaskContext> context, double smoothingLength,
    std::span<const ControlContourBound> contourBounds = {}) {
    using namespace solver::quadratic;
    struct Face { std::array<std::size_t,3> ids; double area; };
    std::vector<Face> faces;
    for (auto triangle : triangles.as_array()) {
        const auto numbers=contracts::ngRigNumbers(triangle);
        Face face{{std::size_t(numbers[0]),std::size_t(numbers[1]),std::size_t(numbers[2])},0};
        const auto a=rest[face.ids[0]],b=rest[face.ids[1]],c=rest[face.ids[2]];
        face.area=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
        if (face.area!=0) faces.push_back(face);
    }
    auto values=requested.duplicate();
    auto position=[&](const auto& offsets,std::size_t i) {
        return contracts::Point2{rest[i][0]+offsets[i*2],rest[i][1]+offsets[i*2+1]};
    };
    const auto contourValue = [&](const ControlContourBound& bound, const auto& offsets) {
        double value = 0.;
        for (std::size_t i = 0; i < bound.vertices.size(); ++i) {
            const auto point = position(offsets,bound.vertices[i]);
            value += bound.weights[i]*(point[0]*bound.direction[0]+point[1]*bound.direction[1]);
        }
        return value;
    };
    auto deficit=[&](const auto& offsets) {
        double loss=0,minimum=std::numeric_limits<double>::infinity();
        for (const auto& face:faces) {
            const auto a=position(offsets,face.ids[0]),b=position(offsets,face.ids[1]),c=position(offsets,face.ids[2]);
            const double ratio=((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))/face.area;
            minimum=std::min(minimum,ratio);
            const double violation=std::max(0.,minimumRatio-ratio);
            loss+=violation*violation;
        }
        for (const auto& bound : contourBounds) {
            if (bound.targetWeight > 0.) continue;
            const double violation = std::max(0.,bound.minimum-contourValue(bound,offsets));
            if (violation > 1e-4) minimum = -std::numeric_limits<double>::infinity();
            loss += violation*violation;
        }
        return std::pair{loss,minimum};
    };
    const auto targetLoss = [&](const auto& offsets) {
        double loss = 0.;
        for (const auto& bound : contourBounds) {
            if (bound.targetWeight <= 0.) continue;
            const double error = contourValue(bound,offsets)-bound.minimum;
            loss += bound.targetWeight*error*error;
        }
        return loss;
    };
    // A fixed horizontal coordinate can make the normal-only problem infeasible
    // after yaw/pitch composition. Linearize signed area in both coordinates,
    // while keeping the exact interpolated feature positions fixed.
    for (unsigned iteration=0;iteration<24;++iteration) {
        contracts::ngRigCheckpoint(context);
        const auto previous=deficit(values);
        if (previous.second>=minimumRatio*.99 && (iteration>0 || targetLoss(values)<1e-8)) return values;
        QuadraticProblem problem;
        problem.variables=static_cast<std::int32_t>(values.size());
        for (std::size_t i=0;i<values.size();++i) {
            problem.objective.append(SparseEntry{std::int32_t(i),std::int32_t(i),1.});
            problem.linear.append(values[i]-requested[i]);
        }
        regularizeOrientation(problem, rest, triangles, smoothingLength, 2, values, requested);
        for (const auto& bound : contourBounds) {
            std::map<std::int32_t, double> coefficients;
            for (std::size_t i = 0; i < bound.vertices.size(); ++i)
                for (std::size_t axis = 0; axis < 2; ++axis)
                    coefficients[std::int32_t(2*bound.vertices[i]+axis)] += bound.weights[i]*bound.direction[axis];
            const double residual = contourValue(bound,values)-bound.minimum;
            if (bound.targetWeight > 0.) {
                for (auto i = coefficients.begin(); i != coefficients.end(); ++i) {
                    problem.linear[i->first] += bound.targetWeight*residual*i->second;
                    for (auto j = i; j != coefficients.end(); ++j)
                        problem.objective.append({i->first,j->first,bound.targetWeight*i->second*j->second});
                }
                continue;
            }
            const auto row = problem.constraints++;
            for (const auto& [column, coefficient] : coefficients)
                if (std::abs(coefficient) > 1e-12) problem.matrix.append({row,column,coefficient});
            problem.lower.append(-residual);
            problem.upper.append(std::numeric_limits<double>::infinity());
        }
        for (const auto& face:faces) {
            const auto a=position(values,face.ids[0]),b=position(values,face.ids[1]),c=position(values,face.ids[2]);
            const double ratio=((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))/face.area;
            const std::array<double,6> gradient{(b[1]-c[1])/face.area,(c[0]-b[0])/face.area,
                (c[1]-a[1])/face.area,(a[0]-c[0])/face.area,
                (a[1]-b[1])/face.area,(b[0]-a[0])/face.area};
            double norm=0;for(auto value:gradient) norm+=value*value;
            norm=std::sqrt(norm);
            if (!(norm>0)) return requested;
            const auto row=problem.constraints++;
            for(std::size_t j=0;j<6;++j) problem.matrix.append(SparseEntry{
                row,std::int32_t(face.ids[j/2]*2+j%2),gradient[j]/norm});
            problem.lower.append((minimumRatio-ratio)/norm);
            problem.upper.append(std::numeric_limits<double>::infinity());
        }
        for(const auto point:anchors) for(const auto& face:faces) {
            const auto weights=controls_detail::barycentric(point,rest[face.ids[0]],rest[face.ids[1]],rest[face.ids[2]]);
            if (!(weights[0]>=-1e-7&&weights[1]>=-1e-7&&weights[2]>=-1e-7)) continue;
            for(std::size_t axis=0;axis<2;++axis) {
                const auto row=problem.constraints++;
                for(std::size_t j=0;j<3;++j) problem.matrix.append(SparseEntry{
                    row,std::int32_t(face.ids[j]*2+axis),weights[j]});
                problem.lower.append(0.);problem.upper.append(0.);
            }
            break;
        }
        const auto result=ngSolveQuadratic(problem,SolverOptions{30'000,1e-8,5,true},
            [context]{return context&&context->isCanceled();});
        if (!result.solved()||result.constraintViolation>=1e-6) break;
        bool accepted=false;
        for(double scale=1.;scale>=1./1024;scale*=.5) {
            auto next=values.duplicate();
            for(std::size_t i=0;i<next.size();++i) next[i]+=scale*result.solution[i];
            const auto measured=deficit(next);
            if ((measured.second>=minimumRatio*.99 &&
                (previous.second<minimumRatio*.99 || targetLoss(next)<=targetLoss(values))) ||
                measured.first<previous.first) {
                values=next;accepted=true;break;
            }
        }
        if(!accepted && previous.second>=minimumRatio*.99) return values;
        if(!accepted) break;
    }
    contracts::ngRigCheckpoint(context);
    return requested;
}
}

compat::Slice<double> ngRigProjectControlOrientation(compat::Slice<contracts::Point2> rest,
    core::settings::SettingsJsonValue triangles, compat::Slice<double> offsets,
    std::shared_ptr<framework::AutoRigTaskContext> context,
    compat::Slice<contracts::Point2> fixedInterpolatedPoints, double minimumRatio, double smoothingLength,
    std::span<const ControlContourBound> contourBounds) {
    if (!contourBounds.empty())
        return projectPlanarOrientation(rest, triangles, offsets, fixedInterpolatedPoints,
            minimumRatio, context, smoothingLength, contourBounds);
    auto prepared = controls_detail::prepareControlOrientation(rest, triangles, offsets, minimumRatio);
    if (!prepared.requiresSolve) return offsets;
    if (prepared.requiresPlanar)
        return projectPlanarOrientation(rest, triangles, offsets, fixedInterpolatedPoints,
            minimumRatio, context, smoothingLength);
    regularizeOrientation(prepared.problem, rest, triangles, smoothingLength, 1);
    for (const auto point : fixedInterpolatedPoints) for (const auto triangle : triangles.as_array()) {
        const auto indices = contracts::ngRigNumbers(triangle);
        const std::array<std::size_t, 3> ids{static_cast<std::size_t>(indices[0]),
            static_cast<std::size_t>(indices[1]), static_cast<std::size_t>(indices[2])};
        const auto weights = controls_detail::barycentric(point, rest[ids[0]], rest[ids[1]], rest[ids[2]]);
        if (!(weights[0] >= -1e-7 && weights[1] >= -1e-7 && weights[2] >= -1e-7)) continue;
        const auto row = prepared.problem.constraints++;
        for (std::size_t j = 0; j < 3; ++j)
            prepared.problem.matrix.append(solver::quadratic::SparseEntry{
                row, static_cast<std::int32_t>(ids[j]), weights[j]});
        prepared.problem.lower.append(0.);
        prepared.problem.upper.append(0.);
        break;
    }
    contracts::ngRigCheckpoint(context);
    auto result = solver::quadratic::ngSolveQuadratic(prepared.problem, prepared.options,
        [context] { return context && context->isCanceled(); });
    contracts::ngRigCheckpoint(context);
    if (!(result.solved() && result.constraintViolation < 1e-6)) {
        if (smoothingLength > 0) {
            try {
                return projectPlanarOrientation(rest, triangles, offsets, fixedInterpolatedPoints,
                    minimumRatio, context, smoothingLength);
            } catch (const compat::ThrowableException&) {
                contracts::ngRigCheckpoint(context);
                return ngRigProjectControlOrientation(rest, triangles, offsets, context,
                    fixedInterpolatedPoints, minimumRatio);
            }
        }
        return projectPlanarOrientation(rest,triangles,offsets,fixedInterpolatedPoints,minimumRatio,context,0.);
    }
    return controls_detail::projectSolvedOrientation(rest, offsets, result);
}

} // namespace nijigenerate::autorig::deterministic::controls
