#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
#include "../bone_gpu_packet.hpp"
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <memory>

namespace nijigenerate::commands::depth::bone::detail {

std::shared_ptr<CommandResult> enqueueDepthBoneGpuDeformCommand(
    std::shared_ptr<ext::nodes::ExDepthRigRoot> rigRoot,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
    compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> actualTargets,
    compat::RetainedStringSlice reason, bool writePreview, bool writeBinding);

} // namespace nijigenerate::commands::depth::bone::detail
