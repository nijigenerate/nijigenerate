#pragma once

#include "contracts.hpp"

namespace nijigenerate::autorig::deterministic::anatomy {

// Uses the shared JSON carriers and the deterministic linear solver.
core::settings::SettingsJsonValue ngRigSolveScaffold(core::settings::SettingsJsonValue evidence,
    core::settings::SettingsJsonValue prior);
compat::Slice<double> ngRigDepthField(compat::Slice<contracts::Point2> query,
    core::settings::SettingsJsonValue domain, core::settings::SettingsJsonValue scaffold,
    core::settings::SettingsJsonValue prior);

} // namespace nijigenerate::autorig::deterministic::anatomy

namespace nijigenerate::autorig::deterministic::anatomy::detail {

// Geometry primitives use the canonical Point2 value type.
double length(contracts::Point2 point);
contracts::Point2 subtract(contracts::Point2 left, contracts::Point2 right);
double dot(contracts::Point2 left, contracts::Point2 right);

} // namespace nijigenerate::autorig::deterministic::anatomy::detail
