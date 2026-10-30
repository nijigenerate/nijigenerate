#pragma once

#include "bone_status_types.hpp"
#include "bone_status_fwd.hpp"
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/retained_vec3_array.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <math/mat4.hpp>
#include <param/parameter.hpp>
#include <puppet.hpp>
#include <cmath>
#include <limits>
#include <memory>

namespace nijigenerate::commands::depth::bone_status {

using ext::nodes::ExDepthRigRoot;
using nicxlive::core::nodes::Node;
using nicxlive::core::param::Parameter;
using nicxlive::core::Puppet;
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Mat4;
using Keypoint = compat::Vec2u;
using Outline = compat::RetainedVec3Array;

struct DepthBoneUpdateStatus {
    std::shared_ptr<ExDepthRigRoot> root;
    std::shared_ptr<Node> target;
    std::shared_ptr<Parameter> parameter;
    Keypoint keypoint{};
    DepthBoneUpdateState state{};
    compat::RetainedString reason;
    compat::RetainedString detail;
    std::size_t expectedWork{};
    std::size_t appliedWork{};
    std::size_t queuedWork{};
    std::size_t processingWork{};
    std::size_t staleWork{};
    std::uint32_t retryCount{};
    std::uint64_t generation{};
    bool hasLocalBounds{};
    Vec2 localBoundsMin{NAN, NAN};
    Vec2 localBoundsMax{NAN, NAN};
    Outline localOutline;
    Outline pendingLocalOutline;
    Vec3 labelLocal{NAN, NAN, NAN};
    bool hasLabelWorld{};
    Vec3 labelWorld{NAN, NAN, NAN};
};

void ngDepthBoneUpdateDetected(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target,
    std::shared_ptr<Parameter> parameter, Keypoint keypoint, compat::RetainedString reason);
void ngDepthBoneUpdatePlanned(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target,
    std::shared_ptr<Parameter> parameter, Keypoint keypoint, compat::RetainedString reason,
    std::size_t expectedWork);
void ngDepthBoneUpdateQueued(std::uint32_t batchId, std::shared_ptr<ExDepthRigRoot> root,
    std::shared_ptr<Node> target, std::shared_ptr<Parameter> parameter, Keypoint keypoint,
    compat::RetainedString reason, std::uint32_t retryCount = 0);
void ngDepthBoneUpdateProcessing(std::uint32_t batchId, std::shared_ptr<ExDepthRigRoot> root,
    std::shared_ptr<Node> target);
void ngDepthBoneUpdateApplied(std::uint32_t batchId, std::shared_ptr<ExDepthRigRoot> root,
    std::shared_ptr<Node> target);
void ngDepthBoneUpdateStale(std::uint32_t batchId, std::shared_ptr<ExDepthRigRoot> root,
    std::shared_ptr<Node> target, compat::RetainedString detail);
void ngDepthBoneUpdateBatchCanceled(std::uint32_t batchId, compat::RetainedString detail = {});
void ngDepthBoneUpdateSetOwnerToken(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target,
    actions::AsyncActionToken token);
void ngDepthBoneUpdateFailed(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target,
    std::shared_ptr<Parameter> parameter, Keypoint keypoint, compat::RetainedString reason,
    compat::RetainedString detail);
void ngDepthBoneUpdateAbort(compat::RetainedString detail);
compat::Slice<DepthBoneUpdateStatus> ngDepthBoneUpdateStatuses(std::shared_ptr<Puppet> puppet,
    bool includeExpired = false);
bool ngBuildDepthBoneUpdateTargetOutline(std::shared_ptr<Node> targetNode, Outline& lines,
    Mat4& targetToWorld, Vec3& labelWorld);
void ngClearDepthBoneUpdateStatuses();

} // namespace nijigenerate::commands::depth::bone_status

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::depth::bone_status::DepthBoneUpdateStatus> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.depth.bone_status.DepthBoneUpdateStatus)";
    inline static constexpr char unqualifiedArgument[] = "(DepthBoneUpdateStatus)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.depth.bone_status.DepthBoneUpdateStatus";
    inline static constexpr char unqualifiedElement[] = "DepthBoneUpdateStatus";
};
}
