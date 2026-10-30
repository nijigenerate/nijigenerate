#pragma once

#include "contracts.hpp"
#include <nodes/part.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {

contracts::Point2 toRoot(std::shared_ptr<nicxlive::core::nodes::Node> node, double x, double y);
// The guarded main-thread snapshot is defined in native_stage_context.cpp.
core::settings::SettingsJsonValue textureMapping(std::shared_ptr<nicxlive::core::nodes::Part> part);
core::settings::SettingsJsonValue rootTextureMapping(std::shared_ptr<nicxlive::core::nodes::Part> part);
bool sameTextureMapping(core::settings::SettingsJsonValue a, core::settings::SettingsJsonValue b);

} // namespace nijigenerate::autorig::deterministic::native_detail
