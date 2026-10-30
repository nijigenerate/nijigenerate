#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
#include "../bone_gpu_packet.hpp"
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <cstdint>
#include <memory>

namespace nijigenerate::commands::depth::bone::detail {

float strictJsonNumber(core::settings::SettingsJsonValue value, compat::RetainedString name);
bool depthBoneGpuPacketMatchesScope(DepthBoneGpuOffsetPacket& packet,
    std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, std::uint32_t targetUuid);

} // namespace nijigenerate::commands::depth::bone::detail
