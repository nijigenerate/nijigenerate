#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Included once by the original bone.d production translation unit.
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <memory>

namespace nijigenerate::actions::node::detail {
// Existing real platform fault implementation in actions/node.cpp.
[[noreturn]] void failActionObjectReference();
}

namespace nijigenerate::commands::depth::bone {
namespace {

std::shared_ptr<ext::nodes::ExDepthBone> findStandardDepthBone(
        std::shared_ptr<ext::nodes::ExDepthRigRoot> root, compat::RetainedString boneId) {
    if (!root) actions::node::detail::failActionObjectReference();
    const auto bones = root->depthBones();
    for (auto bone : bones) {
        if (!bone) actions::node::detail::failActionObjectReference();
        if (bone->boneId.view() == boneId.view() || bone->getName().view() == boneId.view()) return bone;
    }
    return nullptr;
}

} // namespace
} // namespace nijigenerate::commands::depth::bone
