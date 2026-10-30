#pragma once
#include "contracts.hpp"
#include "registered_landmarks.hpp"
namespace nijigenerate::autorig::deterministic::registered {
using Json = core::settings::SettingsJsonValue;
using Point2 = contracts::Point2;
compat::Slice<double> ngRigSampleRegisteredDepth(compat::Slice<double>, compat::Slice<double>,
    compat::Slice<double>, compat::Slice<Point2>);
Landmarks ngRigFacialLandmarks(Json observation);
Json ngRigRegisterProgram(Json program, Json observation);
}
