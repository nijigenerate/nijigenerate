#include "detail/bone_fit.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/depth/common/targetview.hpp>
#include <cmath>
#include <limits>

namespace nijigenerate::commands::depth::bone::detail {
using ext::nodes::ExDepthRigRoot;
using ext::nodes::ExDepthBoneSourceSettings;
using nicxlive::core::nodes::Deformable;
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Mat4;

float depthWorldScale(std::shared_ptr<ExDepthRigRoot> root) {
    if (!root || !project::incActivePuppet()) return 1.0f;
    compat::Slice<std::shared_ptr<Deformable>> targets;
    auto bindings = root->bindings;
    for (auto& binding : bindings) {
        auto puppet = project::incActivePuppet();
        auto target = std::dynamic_pointer_cast<Deformable>(
            compat::ngRequireObject(puppet)->findNodeById(static_cast<std::uint32_t>(binding.targetUuid)));
        if (target) targets.append(target);
    }
    const auto scale = viewport::depth::common::targetview::ngDepthDisplayScaleForTargetsInNodeSpace(root, targets);
    return scale > 0.0f ? scale : 1.0f;
}

float depthBoneSourceTangentSlope(float rotation) {
    const auto normalized = ext::nodes::ngNormalizeDepthBoneSourceRotation(rotation);
    auto cosine = static_cast<float>(std::cos(normalized));
    if (std::abs(cosine) < 1e-4f) cosine = cosine < 0.0f ? -1e-4f : 1e-4f;
    return static_cast<float>(std::sin(normalized)) / cosine;
}

Vec3 depthBoneSourceRestLocal(float x, float y, float rawDepth,
        ExDepthBoneSourceSettings setting, float depthUnitScale) {
    const auto distance = (rawDepth * setting.depthScale + setting.depthOffset) * depthUnitScale;
    return {x + distance * depthBoneSourceTangentSlope(setting.rotation), y, distance};
}

float depthBoneSourceRotationPivotXShift(std::shared_ptr<Deformable> target,
        compat::ConstSlice<float> scaledDepths, Mat4 targetToRoot, Vec3 boneRestHead,
        ExDepthBoneSourceSettings setting, float worldScale) {
    float planeDepth = std::numeric_limits<float>::quiet_NaN();
    float bestDistanceSq = std::numeric_limits<float>::infinity();
    bool found = false;
    const auto vertices = compat::ngRequireObject(target)->getVertices();
    const auto vertexCount = vertices.size();
    for (std::size_t i = 0; i < vertexCount; ++i) {
        if (i >= scaledDepths.size()) break;
        const auto adjustedDepth = scaledDepths[i] * setting.depthScale + setting.depthOffset * worldScale;
        if (!std::isfinite(adjustedDepth)) continue;
        // The source foreach carries a lane view; it reads x/y only at this sample.
        const auto vertex = vertices.at(i);
        const auto sample = transformPoint(targetToRoot, {vertex.x, vertex.y, adjustedDepth});
        if (!std::isfinite(sample.x) || !std::isfinite(sample.y) || !std::isfinite(sample.z)) continue;
        const auto dx = sample.x - boneRestHead.x;
        const auto dy = sample.y - boneRestHead.y;
        const auto distanceSq = dx * dx + dy * dy;
        if (!found || distanceSq < bestDistanceSq) {
            found = true;
            bestDistanceSq = distanceSq;
            planeDepth = sample.z;
        }
    }
    if (!found) {
        const auto sample = transformPoint(targetToRoot, {0.0f, 0.0f, setting.depthOffset * worldScale});
        planeDepth = std::isfinite(sample.z) ? sample.z : 0.0f;
    }
    const auto planeDistance = planeDepth - boneRestHead.z;
    return planeDistance * depthBoneSourceTangentSlope(setting.rotation);
}

} // namespace nijigenerate::commands::depth::bone::detail
