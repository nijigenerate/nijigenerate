#pragma once

#include "bone_gpu_packet.hpp"
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::commands::depth::bone {

// Actual parent construction is in bone_packet_build.cpp and requires the real
// separately reviewed source-payload child. No successful empty packet fallback.
bool ngBuildDepthBoneGpuOffsetPacket(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    ext::nodes::ExDepthRigBinding* binding, std::shared_ptr<nicxlive::core::nodes::Deformable> target,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u cursor,
    DepthBoneGpuOffsetPacket& packet, compat::RetainedStringSlice& error,
    bool writePreview = false, bool writeBinding = true);

} // namespace nijigenerate::commands::depth::bone
