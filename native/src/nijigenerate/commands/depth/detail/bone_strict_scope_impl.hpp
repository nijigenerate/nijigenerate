#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Included once by the production bone.cpp intake.
#include "bone_strict_scope.hpp"
#include "bone_dirty_providers.hpp"
#include "bone_input.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cmath>
#include <new>
#include <stdexcept>
#include <string>

namespace nijigenerate::commands::depth::bone::detail {

float strictJsonNumber(core::settings::SettingsJsonValue value, compat::RetainedString name) try {
    float result;
    if (value.is_int64()) result = static_cast<float>(value.as_int64());
    else if (value.is_uint64()) result = static_cast<float>(value.as_uint64());
    else if (value.is_double()) result = static_cast<float>(value.as_double());
    else failInput(std::string(name.view()) + " must be a number", 1519);
    if (!std::isfinite(result))
        failInput(std::string(name.view()) + " must be finite", 1521);
    return result;
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
}

bool depthBoneGpuPacketMatchesScope(DepthBoneGpuOffsetPacket& packet,
        std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
        std::shared_ptr<nicxlive::core::param::Parameter> parameter, std::uint32_t targetUuid) {
    if (packet.root != root) return false;
    if (parameter && packet.parameter != parameter) return false;
    std::shared_ptr<nicxlive::core::nodes::Node> targetNode = packet.target;
    return depthBoneTargetMatchesScope(root, targetNode, targetUuid);
}

} // namespace nijigenerate::commands::depth::bone::detail
