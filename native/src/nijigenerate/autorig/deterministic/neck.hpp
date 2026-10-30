#pragma once
#include "contracts.hpp"
#include <nijigenerate/compat/slice.hpp>

namespace nijigenerate::autorig::deterministic::neck {
using contracts::Point2;
using contracts::Point3;
using core::settings::SettingsJsonValue;
SettingsJsonValue ngRigInferHeadFrame(compat::Slice<Point2> face, Point2 eyeRight, Point2 eyeLeft);
SettingsJsonValue ngRigInferNeckBase(compat::Slice<Point2> face, compat::Slice<Point2> neck,
    compat::Slice<Point2> torso, compat::Slice<Point2> garments = {}, SettingsJsonValue headFrame = {});
}
