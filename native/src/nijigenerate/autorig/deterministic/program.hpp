#pragma once
#include "contracts.hpp"
#include <optional>
namespace nijigenerate::autorig::deterministic::program {
core::settings::SettingsJsonValue ngRigCompileProgram(core::settings::SettingsJsonValue observation,
    core::settings::SettingsJsonValue evidence,
    std::optional<compat::RetainedStringSlice> observationDigest = std::nullopt);
}
