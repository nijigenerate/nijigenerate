#include "exdepthrig_create.hpp"
#include "exdepthrig_helpers.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <array>
#include <string_view>

namespace nijigenerate::ext::nodes {

void ngAddStandardDepthSkeleton(std::shared_ptr<ExDepthRigRoot> root, float scale) {
    if (!root) throw compat::ThrowableException(
        detail::copyDepthRigEnforceMessage(compat::RetainedString::fromStaticLiteral("Depth rig root is required")),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), 526);
    std::array<float, 4> bounds;
    bool hasBounds = false;
    if (root->parentPtr()) {
        auto parent = root->parentPtr();
        bounds = compat::ngRequireObject(parent)->getCombinedBounds(true);
        const auto width = bounds[2] - bounds[0];
        const auto height = bounds[3] - bounds[1];
        hasBounds = width > 1e-4f && height > 1e-4f;
    }
    if (!hasBounds) bounds = {-180.0f, -300.0f, 180.0f, 300.0f};
    auto rootToLocal = core::math::source_matrix::inverse(root->transform().matrix());
    const auto width = (bounds[2] - bounds[0]) * scale;
    const auto height = (bounds[3] - bounds[1]) * scale;
    const auto cx = (bounds[0] + bounds[2]) * 0.5f;
    const auto top = bounds[1];
    const auto bottom = bounds[3];
    // Invoke the existing separately authored/owned actual point producer.
    const auto p = [&](float x, float y, float z = 0) {
        return detail::depthSkeletonPoint(rootToLocal, width, height, cx, top, bottom, scale, x, y, z);
    };
    using compat::RetainedString;
    auto pelvis = ngCreateDepthBone(root, RetainedString::fromStaticLiteral("Pelvis"), p(0, 0.58f), p(0, 0.47f));
    auto spine = ngCreateDepthBone(pelvis, RetainedString::fromStaticLiteral("Spine"), p(0, 0.47f), p(0, 0.34f));
    auto chest = ngCreateDepthBone(spine, RetainedString::fromStaticLiteral("Chest"), p(0, 0.34f), p(0, 0.22f));
    auto neck = ngCreateDepthBone(chest, RetainedString::fromStaticLiteral("Neck"), p(0, 0.22f), p(0, 0.16f));
    auto head = ngCreateDepthBone(neck, RetainedString::fromStaticLiteral("Head"), p(0, 0.16f), p(0, 0.06f));
    compat::ngRequireObject(head)->allowParentToTargets = false;
    auto clavicleL = ngCreateDepthBone(chest, RetainedString::fromStaticLiteral("Clavicle.L"),
        p(0, 0.25f), p(-0.14f, 0.25f));
    auto upperArmL = ngCreateDepthBone(clavicleL, RetainedString::fromStaticLiteral("UpperArm.L"),
        p(-0.14f, 0.25f), p(-0.26f, 0.42f));
    auto forearmL = ngCreateDepthBone(upperArmL, RetainedString::fromStaticLiteral("Forearm.L"),
        p(-0.26f, 0.42f), p(-0.31f, 0.58f));
    ngCreateDepthBone(forearmL, RetainedString::fromStaticLiteral("Hand.L"), p(-0.31f, 0.58f), p(-0.33f, 0.64f));
    auto clavicleR = ngCreateDepthBone(chest, RetainedString::fromStaticLiteral("Clavicle.R"),
        p(0, 0.25f), p(0.14f, 0.25f));
    auto upperArmR = ngCreateDepthBone(clavicleR, RetainedString::fromStaticLiteral("UpperArm.R"),
        p(0.14f, 0.25f), p(0.26f, 0.42f));
    auto forearmR = ngCreateDepthBone(upperArmR, RetainedString::fromStaticLiteral("Forearm.R"),
        p(0.26f, 0.42f), p(0.31f, 0.58f));
    ngCreateDepthBone(forearmR, RetainedString::fromStaticLiteral("Hand.R"), p(0.31f, 0.58f), p(0.33f, 0.64f));
    auto thighL = ngCreateDepthBone(pelvis, RetainedString::fromStaticLiteral("Thigh.L"),
        p(-0.08f, 0.58f), p(-0.12f, 0.76f));
    auto shinL = ngCreateDepthBone(thighL, RetainedString::fromStaticLiteral("Shin.L"),
        p(-0.12f, 0.76f), p(-0.12f, 0.94f));
    auto footL = ngCreateDepthBone(shinL, RetainedString::fromStaticLiteral("Foot.L"),
        p(-0.12f, 0.94f), p(-0.18f, 0.98f));
    compat::ngRequireObject(footL)->lockToRootValue() = true;
    auto thighR = ngCreateDepthBone(pelvis, RetainedString::fromStaticLiteral("Thigh.R"),
        p(0.08f, 0.58f), p(0.12f, 0.76f));
    auto shinR = ngCreateDepthBone(thighR, RetainedString::fromStaticLiteral("Shin.R"),
        p(0.12f, 0.76f), p(0.12f, 0.94f));
    auto footR = ngCreateDepthBone(shinR, RetainedString::fromStaticLiteral("Foot.R"),
        p(0.12f, 0.94f), p(0.18f, 0.98f));
    compat::ngRequireObject(footR)->lockToRootValue() = true;
}

} // namespace nijigenerate::ext::nodes
