#pragma once
#include "contracts.hpp"
namespace nijigenerate::autorig::deterministic::program {
core::settings::SettingsJsonValue ngRigCompileProgram(core::settings::SettingsJsonValue observation,
    core::settings::SettingsJsonValue evidence);
}
