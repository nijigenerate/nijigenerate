#include "detail/bone_fit.hpp"
#include "detail/bone_vector_math.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <cmath>
#include <limits>

namespace nijigenerate::commands::depth::bone::detail {
using ext::nodes::ExDepthRigRoot;
using ext::nodes::ExDepthBone;
using nicxlive::core::nodes::Node;
using nicxlive::core::math::Vec3;

BoneQuaternion depthEditRotation(float pitch, float yaw, float roll) {
    // inmath's positional signature is (real roll, real pitch, real yaw).
    // Preserve float negation before widening the original application inputs.
    const long double sourceRoll = static_cast<long double>(-pitch);
    const long double sourcePitch = static_cast<long double>(-yaw);
    const long double sourceYaw = static_cast<long double>(roll);
    const auto cr = std::cos(sourceRoll / 2.0L);
    const auto cp = std::cos(sourcePitch / 2.0L);
    const auto cy = std::cos(sourceYaw / 2.0L);
    const auto sr = std::sin(sourceRoll / 2.0L);
    const auto sp = std::sin(sourcePitch / 2.0L);
    const auto sy = std::sin(sourceYaw / 2.0L);
    return {static_cast<float>(cr * cp * cy + sr * sp * sy),
        static_cast<float>(sr * cp * cy - cr * sp * sy),
        static_cast<float>(cr * sp * cy + sr * cp * sy),
        static_cast<float>(cr * cp * sy - sr * sp * cy)};
}

Vec3 depthBoneNodeRestPosition(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> node) {
    Vec3 result{0.0f, 0.0f, 0.0f};
    auto cursor = node;
    while (cursor && cursor != root) {
        result.x += cursor->localTransform.translation.x;
        result.y += cursor->localTransform.translation.y;
        result.z += cursor->localTransform.translation.z;
        // Retain the actual current parent across the virtual lock callback.
        // The traversal still reads the live parent after that callback.
        [[maybe_unused]] const auto parentLifetime = cursor->parentRef();
        if (cursor->lockToRootValue()) break;
        cursor = cursor->parentRef();
    }
    return result;
}

std::shared_ptr<ExDepthBone> firstDepthBoneChild(std::shared_ptr<ExDepthBone> bone) {
    const auto children = compat::ngRequireObject(bone)->childrenList();
    for (auto child : children) {
        if (auto childBone = std::dynamic_pointer_cast<ExDepthBone>(child)) return childBone;
    }
    return nullptr;
}

Vec3 depthBonePlanarDirection(Vec3 direction) {
    direction.z = 0.0f;
    if (segmentLength({0.0f, 0.0f, 0.0f}, direction) > 1e-4f) return direction;
    return {0.0f, 0.0f, 0.0f};
}

void effectiveDepthBoneRest(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<ExDepthBone> bone,
        Vec3& head, Vec3& tail) {
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    head = {nan, nan, nan};
    tail = {nan, nan, nan};
    head = depthBoneNodeRestPosition(root, bone);
    if (auto childBone = firstDepthBoneChild(bone)) {
        const auto childPoint = depthBoneNodeRestPosition(root, childBone);
        auto direction = depthBonePlanarDirection({childPoint.x - head.x, childPoint.y - head.y, childPoint.z - head.z});
        if (segmentLength({0.0f, 0.0f, 0.0f}, direction) <= 1e-4f)
            direction = depthBonePlanarDirection({bone->restTail.x - bone->restHead.x,
                bone->restTail.y - bone->restHead.y, bone->restTail.z - bone->restHead.z});
        if (segmentLength({0.0f, 0.0f, 0.0f}, direction) <= 1e-4f) direction = {0.0f, 100.0f, 0.0f};
        tail = {head.x + direction.x, head.y + direction.y, head.z + direction.z};
        return;
    }
    if (auto parentBone = std::dynamic_pointer_cast<ExDepthBone>(bone->parentRef())) {
        const auto parentPoint = depthBoneNodeRestPosition(root, parentBone);
        auto direction = depthBonePlanarDirection({head.x - parentPoint.x, head.y - parentPoint.y, head.z - parentPoint.z});
        if (segmentLength({0.0f, 0.0f, 0.0f}, direction) <= 1e-4f)
            direction = depthBonePlanarDirection({bone->restTail.x - bone->restHead.x,
                bone->restTail.y - bone->restHead.y, bone->restTail.z - bone->restHead.z});
        if (segmentLength({0.0f, 0.0f, 0.0f}, direction) <= 1e-4f) direction = {0.0f, 100.0f, 0.0f};
        tail = {head.x + direction.x, head.y + direction.y, head.z + direction.z};
        return;
    }
    auto fallback = depthBonePlanarDirection({bone->restTail.x - bone->restHead.x,
        bone->restTail.y - bone->restHead.y, bone->restTail.z - bone->restHead.z});
    if (segmentLength({0.0f, 0.0f, 0.0f}, fallback) <= 1e-4f) fallback = {0.0f, 100.0f, 0.0f};
    tail = {head.x + fallback.x, head.y + fallback.y, head.z + fallback.z};
}

} // namespace nijigenerate::commands::depth::bone::detail
