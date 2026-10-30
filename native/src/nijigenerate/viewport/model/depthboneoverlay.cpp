#include "depthboneoverlay.hpp"
#include <nijigenerate/commands/depth/bone_effective_pivots.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cmath>
#include <array>
#include <vector>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::model::depthboneoverlay {
using namespace nicxlive::core::math;
using namespace ext::nodes;
using namespace commands::depth::bone;
using namespace core::dbg;
using Root = std::shared_ptr<ExDepthRigRoot>;
using Bone = std::shared_ptr<ExDepthBone>;
namespace {
Vec4 transform(const Mat4& matrix, Vec4 point) {
    const std::array<float, 4> input{point.x, point.y, point.z, point.w};
    std::array<float, 4> output{};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            output[row] = nicxlive::core::math::detail::inmathAccumulate(output[row],
                nicxlive::core::math::detail::inmathProduct(matrix[row][column], input[column]));
    return {output[0], output[1], output[2], output[3]};
}
}
DepthBoneOverlayGeometry buildDepthBoneOverlayGeometry(Root root, Bone selectedBone) {
    DepthBoneOverlayGeometry geometry;
    if (!root) return geometry;
    const auto rootToLocal = core::math::source_matrix::inverse(compat::ngRequireObject(root)->transform().matrix());
    const auto bonePoint = [&](const Bone& bone) {
        const auto world = transform(compat::ngRequireObject(bone)->transform().matrix(), {0, 0, 0, 1});
        const auto local = transform(rootToLocal, world);
        return Vec3{local.x, local.y, local.z};
    };
    for (const auto bone : compat::ngRequireObject(root)->depthBones()) {
        const auto point = bonePoint(bone);
        if (bone == selectedBone) geometry.selectedPoints.push_back(point);
        else geometry.points.push_back(point);
        if (auto parent = std::dynamic_pointer_cast<ExDepthBone>(compat::ngRequireObject(bone)->parentPtr())) {
            const auto parentPoint = bonePoint(parent);
            const auto line = [&] {
                try {
                    return std::vector<Vec3>{parentPoint, point};
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
            }();
            auto& lines = bone == selectedBone ? geometry.parentLines :
                parent == selectedBone ? geometry.childLines : geometry.lines;
            lines.append(line);
        }
    }
    return geometry;
}
void drawDepthBones(Root root, Bone selectedBone) {
    auto geometry = buildDepthBoneOverlayGeometry(root, selectedBone);
    if (!root) return;
    if (!geometry.lines.empty()) {
        inDbgSetBuffer(geometry.lines);
        inDbgDrawLines({0.55f, 0.75f, 1, 1}, root->transform().matrix());
    }
    if (!geometry.parentLines.empty()) {
        inDbgSetBuffer(geometry.parentLines);
        inDbgDrawLines({1, 1, 0, 1}, root->transform().matrix());
    }
    if (!geometry.childLines.empty()) {
        inDbgSetBuffer(geometry.childLines);
        inDbgDrawLines({0.2f, 1, 0.8f, 1}, root->transform().matrix());
    }
    if (!geometry.points.empty()) {
        inDbgPointsSize(4);
        inDbgSetBuffer(geometry.points);
        inDbgDrawPoints({0.55f, 0.75f, 1, 1}, root->transform().matrix());
    }
    if (!geometry.selectedPoints.empty()) {
        inDbgPointsSize(12);
        inDbgSetBuffer(geometry.selectedPoints);
        inDbgDrawPoints({1, 0.9f, 0.2f, 1}, root->transform().matrix());
        inDbgPointsSize(5);
        inDbgSetBuffer(geometry.selectedPoints);
        inDbgDrawPoints({1, 1, 1, 1}, root->transform().matrix());
    }
    inDbgPointsSize(4);
    drawDepthBoneEffectivePivots(root, selectedBone);
}
namespace {
void appendDashedLine(Vec3Array& lines, Vec3 start, Vec3 end) {
    const auto delta = end - start;
    for (int segment = 0; segment < 16; ++segment) {
        if ((segment & 1) != 0) continue;
        const float t0 = static_cast<float>(segment) / 16;
        const float t1 = static_cast<float>(segment + 1) / 16;
        lines.push_back(start + delta * t0);
        lines.push_back(start + delta * t1);
    }
}
}
void depthBoneEffectivePivotSelectionChanged(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> nodes) {
    Bone bone;
    for (const auto& node : nodes) {
        if (auto candidate = std::dynamic_pointer_cast<ExDepthBone>(node)) { bone = candidate; break; }
    }
    Root root;
    for (std::shared_ptr<nicxlive::core::nodes::Node> cursor = bone; cursor; cursor = cursor->parentPtr()) {
        if (auto candidate = std::dynamic_pointer_cast<ExDepthRigRoot>(cursor)) { root = candidate; break; }
    }
    if (!project::ngShowDepthBones) { root = nullptr; bone = nullptr; }
    ngSetDepthBoneEffectivePivotSelection(root, bone);
    ngFlushDepthBoneEffectivePivotDirty();
}
void drawDepthBoneEffectivePivots(Root root, Bone selectedBone) {
    if (!root || !selectedBone) return;
    ngSetDepthBoneEffectivePivotSelection(root, selectedBone);
    Vec3Array selectedLines, selectedPoints;
    for (const auto pivot : ngCachedDepthBoneSourceEffectivePivots(root, selectedBone)) {
        if (std::abs(pivot.rotationPivotXShift) <= 1e-4f) continue;
        appendDashedLine(selectedLines, pivot.bonePoint, pivot.effectivePoint);
        selectedPoints.push_back(pivot.effectivePoint);
    }
    if (!selectedLines.empty()) {
        inDbgSetBuffer(selectedLines);
        inDbgDrawLines({0.65f, 1, 0.15f, 1}, root->transform().matrix());
    }
    if (!selectedPoints.empty()) {
        inDbgPointsSize(14);
        inDbgSetBuffer(selectedPoints);
        inDbgDrawPoints({0.65f, 1, 0.15f, 1}, root->transform().matrix());
        inDbgPointsSize(6);
        inDbgSetBuffer(selectedPoints);
        inDbgDrawPoints({1, 1, 1, 1}, root->transform().matrix());
    }
    inDbgPointsSize(4);
}
}
