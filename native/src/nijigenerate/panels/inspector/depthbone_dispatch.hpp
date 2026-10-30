#pragma once
#include <nijigenerate/commands/depth/bone.hpp>
#include <nijigenerate/commands/context.hpp>
namespace nijigenerate::panels::inspector::depthbone {
// Implemented by the actual M070 command adapter; no direct mutation substitute.
void ngRunDepthBoneInspectorCommand(commands::depth::bone::DepthBoneCommand id,
    std::shared_ptr<commands::Context> context,std::shared_ptr<nicxlive::core::nodes::Node> target,
    nicxlive::core::math::Vec3 head={},nicxlive::core::math::Vec3 tail={},float roll=0,
    compat::RetainedString constraint={});
}
