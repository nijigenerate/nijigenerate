#pragma once

#include "contracts.hpp"
#include <nijigenerate/autorig/framework.hpp>

namespace nijigenerate::autorig::deterministic::controls {

compat::Slice<double> ngRigProjectControlOrientation(compat::Slice<contracts::Point2> rest,
    core::settings::SettingsJsonValue triangles, compat::Slice<double> offsets,
    std::shared_ptr<framework::AutoRigTaskContext> context = {});
core::settings::SettingsJsonValue ngRigCompileFixedFootCorrections(core::settings::SettingsJsonValue state,
    core::settings::SettingsJsonValue program, std::shared_ptr<framework::AutoRigTaskContext> context = {});
core::settings::SettingsJsonValue ngRigCompileControls(core::settings::SettingsJsonValue state,
    std::shared_ptr<framework::AutoRigTaskContext> context = {});
core::settings::SettingsJsonValue ngRigCompileCheekCorrections(core::settings::SettingsJsonValue state,
    core::settings::SettingsJsonValue program, std::shared_ptr<framework::AutoRigTaskContext> context = {});

// All source bodies are enrolled by autorig_controls.cmake; runtime validation is separate.

} // namespace nijigenerate::autorig::deterministic::controls
