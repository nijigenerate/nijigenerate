#pragma once

#include <math/mat4.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nodes/deformable.hpp>
#include <limits>

namespace nijigenerate::commands::depth::bone::detail {

// Production value transport for the scalar-first inmath quaternion operations
// used here. Domain nodes, vectors and matrices remain actual runtime types.
struct BoneQuaternion {
    float w = std::numeric_limits<float>::quiet_NaN();
    float x = std::numeric_limits<float>::quiet_NaN();
    float y = std::numeric_limits<float>::quiet_NaN();
    float z = std::numeric_limits<float>::quiet_NaN();
    BoneQuaternion normalized() const;
    nicxlive::core::math::Mat4 toMatrix4() const;
};

bool isTerminalDepthBoneSource(std::shared_ptr<ext::nodes::ExDepthBone> bone,
    compat::Slice<std::shared_ptr<ext::nodes::ExDepthBone>> sources);
BoneQuaternion quatFromUnitVectors(nicxlive::core::math::Vec3 from, nicxlive::core::math::Vec3 to);
nicxlive::core::math::Mat4 composeMatrix(nicxlive::core::math::Vec3 translation, BoneQuaternion rotation);
nicxlive::core::math::Vec3 transformPoint(nicxlive::core::math::Mat4 matrix, nicxlive::core::math::Vec3 point);
nicxlive::core::math::Mat4 targetToRootMatrix(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::nodes::Node> target);
std::shared_ptr<ext::nodes::ExDepthBone> findBoneByUuid(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::uint64_t uuid);
std::shared_ptr<nicxlive::core::nodes::Node> findNodeByUuid(std::shared_ptr<nicxlive::core::nodes::Node> root,
    std::uint64_t uuid);
std::shared_ptr<nicxlive::core::nodes::Node> findActiveNodeByUuid(std::uint64_t uuid);
nicxlive::core::math::Vec2 copyVertex2(std::shared_ptr<nicxlive::core::nodes::Deformable> target,
    std::size_t index);
float targetBoundsSize(std::shared_ptr<nicxlive::core::nodes::Deformable> target);

} // namespace nijigenerate::commands::depth::bone::detail
