#pragma once
#include <nijigenerate/autorig/deterministic/contracts.hpp>
namespace nijigenerate::autorig::deterministic::depth {
using Json = core::settings::SettingsJsonValue;
using contracts::Point2;
using contracts::Point3;
double ngRigCompactSupport(Point2, Json, Json);
compat::Slice<double> ngRigEvaluateDepth(compat::Slice<Point2>, Json, Json, compat::Slice<double> host = {});
compat::Slice<Point2> ngRigLocalCorrections(compat::Slice<Point2>, compat::Slice<Point2>, Point3, Point3,
    Json, Json, double);
}
