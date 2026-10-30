#include "detail/bone_runtime.hpp"
#include "detail/bone_fit.hpp"
#include "detail/bone_input.hpp"
#include "detail/bone_action_scope.hpp"
#include "detail/bone_vector_math.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <param/binding_impl.hpp>
#include <algorithm>
#include <cmath>

namespace nijigenerate::commands::depth::bone::detail {
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Mat4;
using ext::nodes::ExDepthBone;
using nicxlive::core::common::NullableString;

namespace {
// Adapted from permitted inmath 1.0.6 linalg.d. Retain the operation order,
// conjugation-only inverse and non-unit vector polynomial. See
// port/licenses/inmath-depthbone-quaternion.txt for the MIT notice.
BoneQuaternion multiply(BoneQuaternion a, BoneQuaternion b) {
    return {-a.x * b.x - a.y * b.y - a.z * b.z + a.w * b.w,
        a.x * b.w + a.y * b.z - a.z * b.y + a.w * b.x,
        -a.x * b.z + a.y * b.w + a.z * b.x + a.w * b.y,
        a.x * b.y - a.y * b.x + a.z * b.w + a.w * b.z};
}

BoneQuaternion inverse(BoneQuaternion value) {
    return {value.w, -value.x, -value.y, -value.z};
}

Vec3 rotate(BoneQuaternion q, Vec3 v) {
    const float ww = q.w * q.w;
    const float w2 = q.w * 2;
    const float wx2 = w2 * q.x;
    const float wy2 = w2 * q.y;
    const float wz2 = w2 * q.z;
    const float xx = q.x * q.x;
    const float x2 = q.x * 2;
    const float xy2 = x2 * q.y;
    const float xz2 = x2 * q.z;
    const float yy = q.y * q.y;
    const float yz2 = 2 * q.y * q.z;
    const float zz = q.z * q.z;
    return {
        ww * v.x + wy2 * v.z - wz2 * v.y + xx * v.x + xy2 * v.y + xz2 * v.z - zz * v.x - yy * v.x,
        xy2 * v.x + yy * v.y + yz2 * v.z + wz2 * v.x - zz * v.y + ww * v.y - wx2 * v.z - xx * v.y,
        xz2 * v.x + yz2 * v.y + zz * v.z - wy2 * v.x - yy * v.z + wx2 * v.y - xx * v.z + ww * v.z};
}

BoneQuaternion axisRotation(long double angle, Vec3 axis) {
    if (angle == 0) return {1.0f, 0.0f, 0.0f, 0.0f};
    angle /= 2;
    const float sine = static_cast<float>(std::sin(angle));
    return {static_cast<float>(std::cos(angle)), axis.x * sine, axis.y * sine, axis.z * sine};
}

Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
} // namespace

compat::Slice<float> snapshotTargetDepths(std::shared_ptr<nicxlive::core::nodes::Deformable> target) {
    compat::Slice<float> result;
    if (!target) return result;
    result.resize(target->getVertices().size(), std::numeric_limits<float>::quiet_NaN());
    auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(target);
    if (!mapped) return result;
    auto stored = mapped->copyDepths();
    const auto count = std::min(result.size(), stored.size());
    if (count > 0) std::copy_n(stored.begin(), count, result.begin());
    return result;
}

float parameterValue(std::shared_ptr<nicxlive::core::nodes::Node> node,
        std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u cursor,
        NullableString key, float fallback) {
    if (!node || !parameter) return fallback;
    auto binding = std::dynamic_pointer_cast<nicxlive::core::param::ValueParameterBinding>(
        parameter->getBinding(node, key));
    if (binding) return binding->getValue({cursor.x, cursor.y});
    return fallback;
}

RuntimeDepthBoneMap buildDepthRigRuntime(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
        std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u cursor) {
    if (!root) failInput("Depth rig root is required", 1009);
    RuntimeDepthBoneMap runtime;
    const Vec3 upAxis{0.0f, 1.0f, 0.0f};
    auto restBones = root->depthBones();
    for (auto bone : restBones) {
        auto rb = sourceDepthBoneObject<RuntimeDepthBone>();
        rb->source = bone;
        effectiveDepthBoneRest(root, bone, rb->restHead, rb->restTail);
        rb->restLength = segmentLength(compat::ngRequireObject(bone)->restHead,
            compat::ngRequireObject(bone)->restTail);
        rb->restLength = segmentLength(rb->restHead, rb->restTail);
        if (rb->restLength <= 1e-4f) rb->restLength = 1e-4f;
        rb->restQuaternion = multiply(quatFromUnitVectors(upAxis, subtract(rb->restTail, rb->restHead)),
            axisRotation(bone->restRoll, upAxis));
        rb->bindMatrix = composeMatrix(rb->restHead, rb->restQuaternion);
        rb->inverseBindMatrix = core::math::source_matrix::inverse(rb->bindMatrix);
        if (parameter) {
            rb->poseTranslation = bone->lockTranslation ? Vec3{0.0f, 0.0f, 0.0f} : Vec3{
                parameterValue(bone, parameter, cursor, NullableString::fromStaticLiteral("transform.t.x"), 0),
                parameterValue(bone, parameter, cursor, NullableString::fromStaticLiteral("transform.t.y"), 0),
                parameterValue(bone, parameter, cursor, NullableString::fromStaticLiteral("transform.t.z"), 0)};
            const auto pitch = parameterValue(bone, parameter, cursor,
                NullableString::fromStaticLiteral("transform.r.x"), 0);
            const auto yaw = parameterValue(bone, parameter, cursor,
                NullableString::fromStaticLiteral("transform.r.y"), 0);
            const auto roll = parameterValue(bone, parameter, cursor,
                NullableString::fromStaticLiteral("transform.r.z"), 0);
            rb->poseYaw = bone->lockRotation ? 0.0f : yaw;
            rb->poseWithoutYawQuaternion = bone->lockRotation
                ? BoneQuaternion{1, 0, 0, 0} : depthEditRotation(pitch, 0.0f, roll);
            rb->poseQuaternion = bone->lockRotation
                ? BoneQuaternion{1, 0, 0, 0} : depthEditRotation(pitch, yaw, roll);
        } else {
            Vec3 restLocalTranslation;
            if (auto parentBone = std::dynamic_pointer_cast<ExDepthBone>(bone->parentRef())) {
                auto parentRest = runtime.find(parentBone->getUUID());
                restLocalTranslation = parentRest ? subtract(rb->restHead, (*parentRest)->restHead) : rb->restHead;
            } else {
                restLocalTranslation = rb->restHead;
            }
            rb->poseTranslation = bone->lockTranslation ? Vec3{0, 0, 0}
                : subtract(bone->localTransform.translation, restLocalTranslation);
            rb->poseYaw = bone->lockRotation ? 0.0f : bone->localTransform.rotation.y;
            rb->poseWithoutYawQuaternion = bone->lockRotation ? BoneQuaternion{1, 0, 0, 0}
                : depthEditRotation(bone->localTransform.rotation.x, 0.0f, bone->localTransform.rotation.z);
            rb->poseQuaternion = bone->lockRotation ? BoneQuaternion{1, 0, 0, 0}
                : depthEditRotation(bone->localTransform.rotation.x,
                    bone->localTransform.rotation.y, bone->localTransform.rotation.z);
        }
        runtime.set(bone->getUUID(), rb);
    }

    auto parentBones = root->depthBones();
    for (auto bone : parentBones) {
        auto rb = runtime.at(compat::ngRequireObject(bone)->getUUID(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), 1070);
        if (auto parentBone = std::dynamic_pointer_cast<ExDepthBone>(bone->parentRef())) {
            if (auto parent = runtime.find(parentBone->getUUID())) {
                rb->parent = *parent;
                rb->localRestQuaternion = multiply(inverse(rb->parent->restQuaternion), rb->restQuaternion);
                rb->localRestOffset = rotate(inverse(rb->parent->restQuaternion),
                    subtract(rb->restHead, rb->parent->restHead));
            } else {
                rb->localRestQuaternion = rb->restQuaternion;
                rb->localRestOffset = rb->restHead;
            }
        } else {
            rb->localRestQuaternion = rb->restQuaternion;
            rb->localRestOffset = rb->restHead;
        }
    }

    auto poseBones = root->depthBones();
    for (auto bone : poseBones) {
        auto rb = runtime.at(compat::ngRequireObject(bone)->getUUID(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), 1087);
        if (rb->parent && bone->allowParentToTargets && !bone->lockToRootValue()) {
            rb->worldHead = add(add(rb->parent->worldHead, rotate(rb->parent->worldQuaternion,
                rb->localRestOffset)), rotate(rb->parent->worldQuaternion, rb->poseTranslation));
            rb->worldPosePrefix = multiply(rb->parent->worldQuaternion, rb->localRestQuaternion);
        } else {
            rb->worldHead = add(rb->restHead, rb->poseTranslation);
            rb->worldPosePrefix = rb->restQuaternion;
        }
        rb->worldQuaternion = multiply(rb->worldPosePrefix, rb->poseQuaternion);
        rb->worldTail = add(rb->worldHead, rotate(rb->worldQuaternion, {0, rb->restLength, 0}));
        rb->skinMatrix = composeMatrix(rb->worldHead, rb->worldQuaternion) * rb->inverseBindMatrix;
    }
    return runtime;
}

Mat4 depthBoneNoYawSkinMatrix(std::shared_ptr<RuntimeDepthBone> bone) {
    if (!bone) return Mat4::identity();
    if (bone->poseYaw == 0.0f) return bone->skinMatrix;
    const auto sourceWorld = multiply(bone->worldPosePrefix, bone->poseWithoutYawQuaternion);
    return composeMatrix(bone->worldHead, sourceWorld) * bone->inverseBindMatrix;
}

void appendVec3(compat::Slice<float>& values, Vec3 value) {
    values.append(value.x);
    values.append(value.y);
    values.append(value.z);
}

void appendMat4(compat::Slice<float>& values, Mat4 value) {
    for (int i = 0; i < 16; ++i) values.append(value[i / 4][i % 4]);
}

} // namespace nijigenerate::commands::depth::bone::detail
