#include "detail/bone_refresh.hpp"
#include "detail/bone_input.hpp"
#include "detail/bone_debug_log.hpp"
#include "detail/bone_strict_scope.hpp"
#include "bone_packet_build.hpp"
#include "bone_gpu_queries.hpp"
#include "bone_status.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/project.hpp>
#include <array>
#include <string_view>
#include <utility>

namespace nijigenerate::actions::node::detail {
[[noreturn]] void failActionObjectReference();
}

namespace nijigenerate::commands::depth::bone::detail {
using ext::nodes::ExDepthRigRoot;
using nicxlive::core::param::Parameter;
using nicxlive::core::nodes::Node;

namespace {
template<class... Args> compat::RetainedString refreshMessage(std::string_view pattern, Args&&... args) {
    std::array<core::translation::TypedFormatArgument, sizeof...(Args)> arguments{
        core::translation::ngFormatArgument(std::forward<Args>(args))...};
    return compat::RetainedString(core::translation::ngFormatTyped(pattern, arguments));
}

[[noreturn]] void failRefreshPacket(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target,
        std::shared_ptr<Parameter> parameter, compat::Vec2u keypoint,
        compat::RetainedStringSlice reason, compat::RetainedString message,
        std::size_t sourceLine) {
    bone_status::ngDepthBoneUpdateFailed(root, target, parameter, keypoint, reason.retainedString(), message);
    writeDepthBoneGpuFatalLog(message);
    failInput(message.view(), sourceLine);
}

compat::Slice<DepthBoneGpuOffsetPacket> buildRefreshPackets(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<Parameter> parameter, compat::Vec2u keypoint,
        compat::RetainedStringSlice reason, std::uint32_t targetUuid, bool writePreview, bool writeBinding,
        std::size_t missingTargetLine, std::size_t invalidPacketLine) {
    compat::Slice<DepthBoneGpuOffsetPacket> packets;
    auto bindings = root->bindings;
    for (auto& binding : bindings) {
        if (!depthBoneBindingMatchesTarget(root, binding, targetUuid)) continue;
        if (!hasValidDepthBoneSources(root, binding)) continue;
        auto puppet = project::incActivePuppet();
        if (!puppet) actions::node::detail::failActionObjectReference();
        auto targetNode = puppet->findNodeById(static_cast<std::uint32_t>(binding.targetUuid));
        auto deformable = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(targetNode);
        if (!deformable) {
            failRefreshPacket(root, targetNode, parameter, keypoint, reason,
                refreshMessage("Depth Bone GPU packet build failed: target uuid=%s is missing or not deformable",
                    binding.targetUuid), missingTargetLine);
        }
        DepthBoneGpuOffsetPacket packet;
        compat::RetainedStringSlice error;
        if (!ngBuildDepthBoneGpuOffsetPacket(root, &binding, deformable, parameter, keypoint,
                packet, error, writePreview, writeBinding)) {
            failRefreshPacket(root, targetNode, parameter, keypoint, reason,
                refreshMessage("Depth Bone GPU packet build failed: target=%s key=(%s,%s) reason=%s",
                    targetNode ? targetNode->getName()
                        : nicxlive::core::common::NullableString::fromStaticLiteral("(null)"),
                    keypoint.x, keypoint.y, error), invalidPacketLine);
        }
        packets.append(packet);
    }
    return packets;
}
} // namespace

bool ngRefreshDepthBoneDeform(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Parameter> parameter,
        compat::Vec2u keypoint, compat::RetainedStringSlice reason, std::uint32_t targetUuid) {
    if (!isLiveDepthRigRoot(root)) return false;
    if (!root || !project::incActivePuppet()) return false;
    {
        const auto rootName = root->getName();
        const auto parameterName = parameter ? parameter->getName()
            : nicxlive::core::common::NullableString::fromStaticLiteral("(none)");
        depthBoneDebugLog("[DepthBoneRefresh] refresh start: root=%s param=%s key=(%s,%s) reason=%s bindings=%s",
            rootName, parameterName, keypoint.x, keypoint.y, reason, root->bindings.size());
    }
    enforceDepthBoneGpuAvailable(compat::RetainedString::fromStaticLiteral("Depth Bone dirty refresh"));
    planDepthBoneUpdateTargets(root, parameter, keypoint, targetUuid, reason, 1);
    auto packets = buildRefreshPackets(root, parameter, keypoint, reason, targetUuid,
        true, bool(parameter), 3392, 3405);
    if (!packets.empty()) cancelSupersededDepthBoneGpuWork(root, parameter, targetUuid, true, keypoint);
    enqueueDepthBoneGpuRefreshBatch(packets, reason, depthBoneRefreshActionSink);
    return !packets.empty();
}

bool ngRefreshDepthBoneDeformKeypoints(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<Parameter> parameter, compat::Slice<compat::Vec2u> keypoints,
        compat::Vec2u visualKeypoint, compat::RetainedStringSlice reason, std::uint32_t targetUuid) {
    if (!isLiveDepthRigRoot(root)) return false;
    if (!root || !project::incActivePuppet()) return false;
    if (!parameter || keypoints.empty()) return false;
    {
        const auto rootName = root->getName();
        const auto parameterName = parameter->getName();
        depthBoneDebugLog("[DepthBoneRefresh] refresh start: root=%s param=%s scope=all-keypoints "
            "current=(%s,%s) reason=%s keypoints=%s bindings=%s",
            rootName, parameterName, visualKeypoint.x, visualKeypoint.y,
            reason, keypoints.size(), root->bindings.size());
    }
    enforceDepthBoneGpuAvailable(compat::RetainedString::fromStaticLiteral("Depth Bone all-keypoints refresh"));
    bool submitted = false;
    for (auto keypoint : keypoints) {
        auto packets = buildRefreshPackets(root, parameter, keypoint, reason, targetUuid,
            keypoint == visualKeypoint, true, 3451, 3467);
        enqueueDepthBoneGpuRefreshBatch(packets, reason, depthBoneRefreshActionSink);
        submitted = !packets.empty() || submitted;
    }
    return submitted;
}

void cancelSupersededDepthBoneGpuWork(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<Parameter> parameter, std::uint32_t targetUuid,
        bool matchKeypoint, compat::Vec2u keypoint) {
    compat::Slice<std::uint32_t> batchIds;
    auto include = [&](std::uint32_t batchId, DepthBoneGpuOffsetPacket& packet) {
        if (!depthBoneGpuPacketMatchesScope(packet, root, parameter, targetUuid)) return;
        if (matchKeypoint && packet.keypoint != keypoint) return;
        for (auto existing : batchIds) if (existing == batchId) return;
        batchIds.append(batchId);
    };
    auto queued = depthBoneGpuSubmissionQueue;
    for (auto& job : queued) include(job.batchId, job.packet);
    auto refresh = depthBoneGpuRefreshJobs;
    for (auto& job : refresh) include(job.batchId, job.packet);
    auto completed = depthBoneGpuCompletedJobs;
    for (auto& job : completed) include(job.batchId, job.packet);
    for (auto batchId : batchIds) cancelDepthBoneGpuBatch(batchId);
}

bool enqueueDepthBoneAllKeypoints(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<Parameter> parameter, compat::RetainedStringSlice reason,
        std::uint32_t targetUuid, bool settleBeforeDispatch) {
    if (!isLiveDepthRigRoot(root)) return false;
    if (!root || !parameter) return false;
    auto keypoints = depthBoneKeypoints(parameter);
    if (keypoints.empty()) return false;
    const auto closest = parameter->findClosestKeypoint();
    planDepthBoneUpdateTargets(root, parameter, compat::Vec2u{closest.x, closest.y},
        targetUuid, reason, keypoints.size());
    cancelSupersededDepthBoneGpuWork(root, parameter, targetUuid);
    auto jobs = depthBoneAllKeypointJobs;
    for (auto& job : jobs) {
        if (job.root == root && job.parameter == parameter && job.targetUuid == targetUuid) {
            if (job.actionSink != depthBoneRefreshActionSink) {
                if (job.actionSink && job.actionToken.active()) {
                    const auto receiver = job.actionSink;
                    if (!receiver) actions::node::detail::failActionObjectReference();
                    receiver->markAsyncFinished();
                }
                job.actionSink = depthBoneRefreshActionSink;
                if (depthBoneRefreshActionSink) {
                    const auto receiver = depthBoneRefreshActionSink;
                    if (!receiver) actions::node::detail::failActionObjectReference();
                    job.actionToken = receiver->asyncToken();
                } else job.actionToken = {};
                setDepthBoneUpdateOwner(root, targetUuid, job.actionToken);
            }
            job.keypoints = keypoints;
            job.processed.clear();
            job.nextIndex = 0;
            job.reason = reason;
            job.settleFrames = settleBeforeDispatch ? 1 : 0;
            return true;
        }
    }
    DepthBoneAllKeypointJob job;
    job.root = root;
    job.parameter = parameter;
    job.targetUuid = targetUuid;
    job.keypoints = keypoints;
    job.reason = reason;
    job.actionSink = depthBoneRefreshActionSink;
    job.settleFrames = settleBeforeDispatch ? 1 : 0;
    if (depthBoneRefreshActionSink) {
        const auto receiver = depthBoneRefreshActionSink;
        if (!receiver) actions::node::detail::failActionObjectReference();
        job.actionToken = receiver->asyncToken();
    } else job.actionToken = {};
    depthBoneAllKeypointJobs.append(job);
    return true;
}

bool ngRefreshDepthBoneDeformAllKeypoints(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<Parameter> parameter, compat::Vec2u currentKeypoint,
        compat::RetainedStringSlice reason, std::uint32_t targetUuid, bool settleBeforeDispatch) {
    if (!isLiveDepthRigRoot(root)) return false;
    if (!parameter) {
        auto parameters = depthBoneAffectedParameters(root);
        depthBoneDebugLog("[DepthBoneRefresh] resolve parameters: root=%s reason=%s resolved=%s",
            root ? root->getName() : nicxlive::core::common::NullableString::fromStaticLiteral("(null)"),
            reason, parameters.size());
        if (parameters.empty()) {
            failDepthBoneUpdateTargets(root, nullptr, currentKeypoint, targetUuid, reason,
                compat::RetainedString::fromStaticLiteral(
                    "No parameter drives this DepthBone rig, so no target writeback was queued"));
            return false;
        }
        bool queued = false;
        for (auto resolved : parameters)
            queued = enqueueDepthBoneAllKeypoints(root, resolved, reason, targetUuid, settleBeforeDispatch) || queued;
        return queued;
    }
    return enqueueDepthBoneAllKeypoints(root, parameter, reason, targetUuid, settleBeforeDispatch);
}

} // namespace nijigenerate::commands::depth::bone::detail
