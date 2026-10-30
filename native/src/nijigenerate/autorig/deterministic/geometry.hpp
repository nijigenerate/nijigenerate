#pragma once
#include "contracts.hpp"
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/core/settings_value.hpp"

namespace nijigenerate::autorig::deterministic::geometry {
compat::Slice<contracts::Point3> ngRigRotate(compat::Slice<contracts::Point3> points,
    contracts::Point3 pivot, contracts::Point3 angles);
bool ngRigSameTextureMapping(core::settings::SettingsJsonValue a, core::settings::SettingsJsonValue b);
double ngRigTriangleMinimumRatio(compat::Slice<contracts::Point2> rest,
    core::settings::SettingsJsonValue triangles, compat::Slice<double> offsets);
double ngRigVerifySourceUV(core::settings::SettingsJsonValue sourceRoot,
    core::settings::SettingsJsonValue currentRoot, double tolerance = .001);
core::settings::SettingsJsonValue ngRigSourceUVRegistration(core::settings::SettingsJsonValue source,
    core::settings::SettingsJsonValue current);
core::settings::SettingsJsonValue ngRigValidateGrid(compat::Slice<contracts::Point2> grid,
    std::size_t columns, double minimumRatio = 1e-8, compat::Slice<contracts::Point2> rest = {});
contracts::Point3 ngRigClosestTriangleWeights(contracts::Point2 p, contracts::Point2 a,
    contracts::Point2 b, contracts::Point2 c);
compat::Slice<contracts::Point2> ngRigUVGrid(compat::Slice<double> xs, compat::Slice<double> ys);
compat::Slice<contracts::Point2> ngRigSampleGrid(compat::Slice<contracts::Point2> grid,
    compat::Slice<double> xs, compat::Slice<double> ys, compat::Slice<contracts::Point2> query);
compat::Slice<contracts::Point2> ngRigFitGuide(compat::Slice<contracts::Point2> query,
    compat::FixedArray<double, 4> bounds, compat::Slice<contracts::Point2> source,
    compat::Slice<contracts::Point2> target);
}
