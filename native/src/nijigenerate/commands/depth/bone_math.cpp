#include "detail/bone_math.hpp"
#include "detail/bone_vector_math.hpp"
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/project.hpp>
#include <cmath>

namespace nijigenerate::commands::depth::bone::detail {
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Mat4;
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::Deformable;
using ext::nodes::ExDepthBone;
using ext::nodes::ExDepthRigRoot;

namespace {
float minimum(float a, float b) { return b < a ? b : a; }
float maximum(float a, float b) { return a < b ? b : a; }
}

BoneQuaternion BoneQuaternion::normalized() const {
    // The source rounds this component sum before converting it to real for
    // sqrt and finally narrowing its magnitude back to the component type.
    const float squared = w*w + x*x + y*y + z*z;
    const float magnitude = static_cast<float>(std::sqrt(static_cast<long double>(squared)));
    if (magnitude != 0) return {w/magnitude, x/magnitude, y/magnitude, z/magnitude};
    return *this;
}

Mat4 BoneQuaternion::toMatrix4() const {
    auto result = Mat4::identity();
    const float xx=x*x, xy=x*y, xz=x*z, xw=x*w;
    const float yy=y*y, yz=y*z, yw=y*w, zz=z*z, zw=z*w;
    result[0][0] = 1 - 2*(yy+zz);
    result[0][1] = 2*(xy-zw);
    result[0][2] = 2*(xz+yw);
    result[1][0] = 2*(xy+zw);
    result[1][1] = 1 - 2*(xx+zz);
    result[1][2] = 2*(yz-xw);
    result[2][0] = 2*(xz-yw);
    result[2][1] = 2*(yz+xw);
    result[2][2] = 1 - 2*(xx+yy);
    return result;
}

bool isTerminalDepthBoneSource(std::shared_ptr<ExDepthBone> bone,
    compat::Slice<std::shared_ptr<ExDepthBone>> sources) {
    for (auto candidate : sources) {
        if (compat::ngRequireObject(candidate)->parentRef() == bone) return false;
    }
    return true;
}

BoneQuaternion quatFromUnitVectors(Vec3 from, Vec3 to) {
    from = normalizeVec(from);
    to = normalizeVec(to);
    float r = dotVec(from, to) + 1.0f;
    Vec3 axis;
    if (r < 1e-6f) {
        r = 0.0f;
        if (std::abs(from.x) > std::abs(from.z)) axis = {-from.y, from.x, 0};
        else axis = {0, -from.z, from.y};
    } else {
        axis = {from.y*to.z-to.y*from.z, from.z*to.x-to.z*from.x, from.x*to.y-to.x*from.y};
    }
    return BoneQuaternion{r,axis.x,axis.y,axis.z}.normalized();
}

Mat4 composeMatrix(Vec3 translation, BoneQuaternion rotation) {
    const auto translationMatrix = Mat4::translation(translation);
    const auto rotationMatrix = rotation.toMatrix4();
    return translationMatrix * rotationMatrix;
}

Vec3 transformPoint(Mat4 matrix, Vec3 point) {
    return core::math::ngTransformHomogeneousPoint(matrix, point);
}

Mat4 targetToRootMatrix(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target) {
    if (!root || !target) return Mat4::identity();
    const auto rootMatrix = root->transform().matrix();
    const auto inverseRoot = core::math::source_matrix::inverse(rootMatrix);
    const auto targetMatrix = target->transform().matrix();
    return inverseRoot * targetMatrix;
}

std::shared_ptr<ExDepthBone> findBoneByUuid(std::shared_ptr<ExDepthRigRoot> root, std::uint64_t uuid) {
    for (auto bone : compat::ngRequireObject(root)->depthBones()) {
        if (std::uint64_t(compat::ngRequireObject(bone)->getUUID()) == uuid) return bone;
    }
    return {};
}

std::shared_ptr<Node> findNodeByUuid(std::shared_ptr<Node> root, std::uint64_t uuid) {
    if (!root) return {};
    if (std::uint64_t(root->getUUID()) == uuid) return root;
    const auto children = root->childrenList();
    for (auto child : children) {
        if (auto found = findNodeByUuid(child, uuid)) return found;
    }
    return {};
}

std::shared_ptr<Node> findActiveNodeByUuid(std::uint64_t uuid) {
    auto puppet = project::incActivePuppet();
    if (!puppet || !puppet->root) return {};
    return findNodeByUuid(puppet->root, uuid);
}

Vec2 copyVertex2(std::shared_ptr<Deformable> target, std::size_t index) {
    auto vertex = compat::ngRequireObject(target)->getVertices()[index];
    return {vertex.x, vertex.y};
}

float targetBoundsSize(std::shared_ptr<Deformable> target) {
    if (!target || target->getVertices().empty()) return 1.0f;
    auto low = copyVertex2(target, 0);
    auto high = low;
    const auto vertices = target->getVertices();
    for (std::size_t i = 1; i < vertices.size(); ++i) {
        auto vertex = vertices.at(i);
        low.x = minimum(low.x, vertex.x);
        low.y = minimum(low.y, vertex.y);
        high.x = maximum(high.x, vertex.x);
        high.y = maximum(high.y, vertex.y);
    }
    const Vec2 size{high.x-low.x, high.y-low.y};
    return maximum(1.0f, maximum(size.x, size.y));
}

} // namespace nijigenerate::commands::depth::bone::detail
