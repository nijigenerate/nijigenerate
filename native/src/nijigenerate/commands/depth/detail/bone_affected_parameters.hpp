#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Included once by the original bone.d production translation unit.
#include "bone_dirty_providers.hpp"
#include "bone_refresh_state.hpp"
#include <nijigenerate/project.hpp>
#include <puppet.hpp>

namespace nijigenerate::commands::depth::bone::detail {

compat::Slice<std::shared_ptr<nicxlive::core::param::Parameter>> depthBoneAffectedParameters(
        std::shared_ptr<ext::nodes::ExDepthRigRoot> rigRoot) {
    using nicxlive::core::param::Parameter;
    compat::Slice<std::shared_ptr<Parameter>> result;
    const auto puppet = project::incActivePuppet();
    if (!rigRoot || !puppet) return result;

    const auto hasParam = [&result](std::shared_ptr<Parameter> parameter) {
        for (auto existing : result) if (existing == parameter) return true;
        return false;
    };

    const auto armed = project::incArmedParameter();
    if (depthBoneParameterDrivesRig(rigRoot, armed)) result.append(armed);
    if (lastDepthBoneDirtyRoot == rigRoot &&
        depthBoneParameterDrivesRig(rigRoot, lastDepthBoneDirtyParameter) &&
        !hasParam(lastDepthBoneDirtyParameter)) {
        result.append(lastDepthBoneDirtyParameter);
    }

    // Capture the actual retained array descriptor once, as the source foreach does.
    const auto parameters = puppet->parameters;
    for (auto parameter : parameters) {
        if (!parameter || hasParam(parameter)) continue;
        if (depthBoneParameterDrivesRig(rigRoot, parameter)) result.append(parameter);
    }
    return result;
}

} // namespace nijigenerate::commands::depth::bone::detail
