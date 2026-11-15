#pragma once

#include "contracts.hpp"
#include <nijigenerate/autorig/framework.hpp>
#include <array>
#include <span>

namespace nijigenerate::autorig::deterministic::controls {

struct ControlContourBound {
    std::array<std::size_t, 9> vertices;
    std::array<double, 9> weights;
    contracts::Point2 direction;
    double minimum;
    double targetWeight = 0.;
};

compat::Slice<double> ngRigProjectControlOrientation(compat::Slice<contracts::Point2> rest,
    core::settings::SettingsJsonValue triangles, compat::Slice<double> offsets,
    std::shared_ptr<framework::AutoRigTaskContext> context = {},
    compat::Slice<contracts::Point2> fixedInterpolatedPoints = {}, double minimumRatio = .004,
    double smoothingLength = 0., std::span<const ControlContourBound> contourBounds = {});
core::settings::SettingsJsonValue ngRigCompileFixedFootCorrections(core::settings::SettingsJsonValue state,
    core::settings::SettingsJsonValue program, std::shared_ptr<framework::AutoRigTaskContext> context = {});
core::settings::SettingsJsonValue ngRigCompileControls(core::settings::SettingsJsonValue state,
    std::shared_ptr<framework::AutoRigTaskContext> context = {});
core::settings::SettingsJsonValue ngRigCompileCheekCorrections(core::settings::SettingsJsonValue state,
    core::settings::SettingsJsonValue program, std::shared_ptr<framework::AutoRigTaskContext> context = {});

// All source bodies are enrolled by autorig_controls.cmake; runtime validation is separate.

} // namespace nijigenerate::autorig::deterministic::controls
