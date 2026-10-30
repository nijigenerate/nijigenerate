#include "detail/bone_job_workers.hpp"
#include "detail/bone_runtime.hpp"
#include "detail/bone_hash.hpp"
#include "bone_packet_build.hpp"
#include "bone_constants.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/nullable_string.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::commands::depth::bone::detail {
using nicxlive::core::math::Vec2Array;
using nicxlive::core::math::Mat4;

namespace {
template<class... Args> void logFreshness(std::string_view pattern, Args&&... args) {
    std::array<core::translation::TypedFormatArgument, sizeof...(Args)> arguments{
        core::translation::ngFormatArgument(std::forward<Args>(args))...};
    writeDepthBoneGpuFatalLog(compat::RetainedString(core::translation::ngFormatTyped(pattern, arguments)));
}
} // namespace

compat::Slice<DepthBoneGpuOffsetPacket> depthBoneGpuBatchPackets(std::uint32_t batchId) {
    compat::Slice<DepthBoneGpuOffsetPacket> packets;
    auto queued = depthBoneGpuSubmissionQueue;
    for (auto job : queued) if (job.batchId == batchId) packets.append(job.packet);
    auto refresh = depthBoneGpuRefreshJobs;
    for (auto job : refresh) if (job.batchId == batchId) packets.append(job.packet);
    auto completed = depthBoneGpuCompletedJobs;
    for (auto job : completed) if (job.batchId == batchId) packets.append(job.packet);
    return packets;
}

bool sameVec2Array(Vec2Array a, Vec2Array b) {
    if (a.size() != b.size()) return false;
    const auto length = a.size();
    for (std::size_t i = 0; i < length; ++i) {
        if (std::abs(a[i].x - b[i].x) > 0.0001f || std::abs(a[i].y - b[i].y) > 0.0001f) return false;
    }
    return true;
}

bool sameFloatArray(compat::ConstSlice<float> a, compat::ConstSlice<float> b) {
    if (a.size() != b.size()) return false;
    const auto length = a.size();
    for (std::size_t i = 0; i < length; ++i) {
        if (a[i] == b[i]) continue;
        const bool bothNaN = a[i] != a[i] && b[i] != b[i];
        if (bothNaN) continue;
        if (!std::isfinite(a[i]) || !std::isfinite(b[i])) return false;
        if (std::abs(a[i] - b[i]) > 0.0001f) return false;
    }
    return true;
}

bool matrixApproxEqual(Mat4 a, Mat4 b) {
    for (int x = 0; x < 4; ++x)
        for (int y = 0; y < 4; ++y)
            if (std::abs(a[x][y] - b[x][y]) > 0.0001f) return false;
    return true;
}

bool isDepthBoneGpuRefreshJobCurrent(DepthBoneGpuRefreshJob& job, compat::RetainedStringSlice& reason) {
    reason.reset();
    auto target = job.packet.target;
    if (!target) {
        reason = compat::RetainedString::fromStaticLiteral("target was deleted");
        return false;
    }
    auto targetNode = std::static_pointer_cast<nicxlive::core::nodes::Node>(target);
    if (!targetNode || findActiveNodeByUuid(targetNode->getUUID()) != targetNode) {
        reason = compat::RetainedString::fromStaticLiteral("target is no longer attached to the active puppet");
        return false;
    }
    if (!isLiveDepthRigRoot(job.packet.root)) {
        reason = compat::RetainedString::fromStaticLiteral("depth rig root is no longer live");
        return false;
    }
    const auto currentVertices = target->getVertices();
    if (!sameVec2Array(currentVertices, job.packet.vertices)) {
        reason = compat::RetainedString::fromStaticLiteral("target vertices changed while GPU job was pending");
        return false;
    }
    auto currentRawDepths = snapshotTargetDepths(target);
    if (!sameFloatArray(currentRawDepths, job.packet.rawDepths)) {
        reason = compat::RetainedString::fromStaticLiteral("target depths changed while GPU job was pending");
        return false;
    }
    if (!job.packet.writeBinding) {
        auto currentTargetToRoot = targetToRootMatrix(job.packet.root, targetNode);
        if (!matrixApproxEqual(currentTargetToRoot, job.packet.targetToRoot)) {
            reason = compat::RetainedString::fromStaticLiteral("target transform changed while GPU job was pending");
            return false;
        }
    }
    std::ptrdiff_t bindingIndex;
    {
        const auto bindingRoot = job.packet.root;
        const auto targetUuid = target->getUUID();
        bindingIndex = compat::ngRequireObject(bindingRoot)->findBindingIndex(targetUuid);
    }
    if (bindingIndex < 0) {
        reason = compat::RetainedString::fromStaticLiteral("depth rig binding was removed while GPU job was pending");
        return false;
    }
    DepthBoneGpuOffsetPacket currentPacket;
    compat::RetainedStringSlice buildError;
    // Retain the actual array backing for the interior pointer across the call.
    auto bindings = job.packet.root->bindings;
    if (!ngBuildDepthBoneGpuOffsetPacket(job.packet.root, &bindings[static_cast<std::size_t>(bindingIndex)],
            target, job.packet.parameter, job.packet.keypoint, currentPacket, buildError,
            job.packet.writePreview, job.packet.writeBinding)) {
        try {
            reason = compat::RetainedString(
                std::string("depth rig packet could not be rebuilt while GPU job was pending: ")
                + std::string(buildError.view()));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        }
        return false;
    }
    if (currentPacket.boneCount != job.packet.boneCount
            || currentPacket.sourceCount != job.packet.sourceCount
            || currentPacket.maxInfluences != job.packet.maxInfluences
            || std::abs(currentPacket.influenceRadiusFloor - job.packet.influenceRadiusFloor) > 0.0001f
            || std::abs(currentPacket.radiusScale - job.packet.radiusScale) > 0.0001f
            || !sameFloatArray(currentPacket.bones, job.packet.bones)
            || !sameFloatArray(currentPacket.sourceInputs, job.packet.sourceInputs)) {
        reason = compat::RetainedString::fromStaticLiteral("depth rig binding changed while GPU job was pending");
        return false;
    }
    if (job.packet.parameter) {
        const auto currentParameterStructureHash =
            depthBoneParameterStructureHash(job.packet.root, job.packet.parameter);
        if (currentParameterStructureHash != job.packet.parameterStructureHash) {
            reason = compat::RetainedString::fromStaticLiteral(
                "depth bone parameter structure changed while GPU job was pending");
            return false;
        }
        const auto currentPoseHash =
            depthBonePoseKeyHash(job.packet.root, job.packet.parameter, job.packet.keypoint);
        if (currentPoseHash != job.packet.poseHash) {
            reason = compat::RetainedString::fromStaticLiteral("depth bone pose changed while GPU job was pending");
            return false;
        }
    }
    return true;
}

bool isDepthBoneGpuQueuedJobCurrent(DepthBoneGpuQueuedJob& job, compat::RetainedStringSlice& reason) {
    DepthBoneGpuRefreshJob pending;
    pending.packet = job.packet;
    return isDepthBoneGpuRefreshJobCurrent(pending, reason);
}

void requeueStaleDepthBoneGpuBatch(DepthBoneGpuRefreshJob job, compat::RetainedStringSlice staleReason) {
    auto targetNode = std::static_pointer_cast<nicxlive::core::nodes::Node>(job.packet.target);
    {
        const auto targetName = targetNode ? targetNode->getName()
            : nicxlive::core::common::NullableString::fromStaticLiteral("(null)");
        logFreshness("Depth Bone GPU async batch stale: target=%s key=(%s,%s) reason=%s",
            targetName, job.packet.keypoint.x, job.packet.keypoint.y, staleReason);
    }
    auto originalPackets = depthBoneGpuBatchPackets(job.batchId);
    compat::Slice<DepthBoneGpuOffsetPacket> retryPackets;
    compat::RetainedStringSlice failure;
    for (auto packet : originalPackets) {
        auto packetTarget = std::static_pointer_cast<nicxlive::core::nodes::Node>(packet.target);
        if (!packet.root || !isLiveDepthRigRoot(packet.root) || !packet.parameter || !packetTarget
                || findActiveNodeByUuid(packetTarget->getUUID()) != packetTarget) {
            failure = compat::RetainedString::fromStaticLiteral("batch target is no longer available");
            break;
        }
        if (packet.staleRetryCount >= DepthBoneGpuMaxStaleRetries) {
            failure = compat::RetainedString::fromStaticLiteral("stale retry limit reached");
            break;
        }
        const auto bindingIndex = packet.root->findBindingIndex(packetTarget->getUUID());
        if (bindingIndex < 0) {
            failure = compat::RetainedString::fromStaticLiteral("batch target binding is no longer available");
            break;
        }
        DepthBoneGpuOffsetPacket retryPacket;
        auto bindings = packet.root->bindings;
        if (!ngBuildDepthBoneGpuOffsetPacket(packet.root, &bindings[static_cast<std::size_t>(bindingIndex)],
                packet.target, packet.parameter, packet.keypoint, retryPacket, failure,
                packet.writePreview, packet.writeBinding)) break;
        retryPacket.staleRetryCount = packet.staleRetryCount + 1;
        retryPackets.append(retryPacket);
    }
    cancelDepthBoneGpuBatch(job.batchId,
        retryPackets.size() == originalPackets.size() && !retryPackets.empty()
            ? compat::RetainedString::fromStaticLiteral("Stale batch is being regenerated")
            : compat::RetainedString::fromStaticLiteral("Stale batch could not be regenerated"), false);
    if (retryPackets.size() != originalPackets.size() || retryPackets.empty()) {
        logFreshness("Depth Bone GPU stale batch retry failed: %s", failure);
        finishCanceledDepthBoneGpuTask(job.actionSink, job.actionToken);
        return;
    }
    enqueueDepthBoneGpuRefreshBatch(retryPackets, job.reason, job.actionSink, job.actionToken);
    {
        const auto targetName = targetNode ? targetNode->getName()
            : nicxlive::core::common::NullableString::fromStaticLiteral("(null)");
        logFreshness("Depth Bone GPU async batch requeued: target=%s key=(%s,%s) targets=%s retry=%s/%s",
            targetName, job.packet.keypoint.x, job.packet.keypoint.y, retryPackets.size(),
            retryPackets[0].staleRetryCount, DepthBoneGpuMaxStaleRetries);
    }
}

} // namespace nijigenerate::commands::depth::bone::detail
