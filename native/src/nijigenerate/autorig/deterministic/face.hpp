#pragma once
#include "contracts.hpp"
#include <nijigenerate/autorig/solver/quadratic.hpp>
namespace nijigenerate::autorig::deterministic::face {
using RigPoint = contracts::Point2;
struct FaceProjection {
    compat::Slice<RigPoint> points;
    double rawMinimum = 0, correctedMinimum = 0, maximumCorrection = 0;
    solver::quadratic::SolverResult solver;
};
FaceProjection ngPreserveFaceOrientation(compat::Slice<RigPoint>, compat::Slice<double>, compat::Slice<double>,
    compat::Slice<RigPoint>, RigPoint, double, compat::Slice<bool> protectedVertices = {},
    solver::quadratic::CancellationHandler canceled = {});
compat::Slice<double> ngFitGridDepth(compat::Slice<double>, compat::Slice<double>, compat::Slice<double>,
    compat::Slice<RigPoint>, compat::Slice<double>, double, std::size_t,
    solver::quadratic::CancellationHandler canceled = {});
}


namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::autorig::deterministic::face::FaceProjection> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.deterministic.face.FaceProjection)";
    inline static constexpr char unqualifiedArgument[] = "(FaceProjection)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.deterministic.face.FaceProjection";
    inline static constexpr char unqualifiedElement[] = "FaceProjection";
};
}
