#pragma once
#include "contracts.hpp"

namespace nijigenerate::autorig::deterministic::hierarchy {
core::settings::SettingsJsonValue ngRigCompileHierarchy(core::settings::SettingsJsonValue observation,
    core::settings::SettingsJsonArray carriers, core::settings::SettingsJsonValue scaffold);
} // namespace nijigenerate::autorig::deterministic::hierarchy
