#pragma once
#include <nijigenerate/autorig/framework.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <memory>

namespace nijigenerate::autorig::deterministic::editor {
core::settings::SettingsJsonValue ngApplyFaceProjection(core::settings::SettingsJsonValue projection,
    core::settings::SettingsJsonValue target, std::shared_ptr<framework::AutoRigTaskContext> task);
}
