#pragma once
#include <nijigenerate/commands/depth/bone.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
namespace nijigenerate::panels::inspector::griddeform {
// Connected to the actual M070 registered command classes by its command adapter.
void ngRunGridDepthBoneCommand(commands::depth::bone::DepthBoneCommand,
    std::shared_ptr<commands::Context>,std::shared_ptr<ext::nodes::ExDepthRigRoot>,
    std::shared_ptr<nicxlive::core::nodes::Node>,std::shared_ptr<ext::nodes::ExDepthBone> = {},
    compat::RetainedString = {},float scale = 1.f);
}
