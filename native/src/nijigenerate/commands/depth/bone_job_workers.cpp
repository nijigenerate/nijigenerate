#include "detail/bone_job_workers.hpp"
#include "detail/bone_input.hpp"
#include "detail/bone_action_scope.hpp"
#include "detail/bone_dirty_providers.hpp"
#include "detail/bone_debug_log.hpp"
#include "bone_gpu_async.hpp"
#include "bone_gpu_queries.hpp"
#include "bone_status.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/project.hpp>
#include <array>
#include <new>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <utility>

namespace nijigenerate::commands::depth::bone::detail {
namespace {

template<class Array> decltype(auto) workerJobElement(Array& jobs, std::size_t index, std::size_t sourceLine) {
    const auto length = jobs.size();
    if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), sourceLine);
    return jobs[index];
}

template<class Array> auto workerJobSlice(const Array& jobs, std::size_t first, std::size_t last,
        std::size_t sourceLine) {
    const auto length = jobs.size();
    if (first > last || last > length) throw compat::ngStaticError<compat::ArraySliceError>(
        first, last, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), sourceLine);
    return jobs.slice(first, last);
}

template<class Record> void removeWorkerJobAt(compat::Slice<Record>& jobs, std::size_t index,
        std::size_t sourceLine) {
    auto before = workerJobSlice(jobs, 0, index, sourceLine);
    auto after = workerJobSlice(jobs, index + 1, jobs.size(), sourceLine);
    if (before.empty() && after.empty()) {
        jobs = {};
        return;
    }
    try {
        std::vector<Record> values;
        values.reserve(before.size() + after.size());
        for (auto value : before) values.push_back(value);
        for (auto value : after) values.push_back(value);
        jobs = compat::Slice<Record>(std::move(values));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}

[[noreturn]] void reportAsyncFailure(std::string_view pattern, compat::RetainedStringSlice error,
        std::size_t sourceLine) {
    std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(error)};
    auto message = compat::RetainedString(core::translation::ngFormatTyped(pattern, arguments));
    abortDepthBoneGpuRefresh(message);
    writeDepthBoneGpuFatalLog(message);
    failInput(message.view(), sourceLine);
}

bool batchIsCanceled(std::uint32_t batchId) {
    return canceledDepthBoneGpuBatches.contains(batchId);
}

} // namespace

bool submitDepthBoneGpuRefreshJob(DepthBoneGpuQueuedJob queued, compat::RetainedStringSlice& error) {
    error.reset();
    auto packet = queued.packet;
    auto dispatch = packet.dispatchPacket();
    std::uint32_t jobId = 0;
    if (!bone_gpu_async::ngSubmitDepthBoneGpuAsync(dispatch, jobId, error)) return false;
    depthBoneGpuRefreshJobs.append(DepthBoneGpuRefreshJob{jobId, packet, queued.reason,
        queued.actionSink, queued.batchId, queued.batchExpected, queued.actionToken});
    if (queued.actionSink && queued.actionToken.active()) queued.actionSink->markAsyncRunning();
    bone_status::ngDepthBoneUpdateProcessing(queued.batchId, packet.root, packet.target);
    return true;
}

void enqueueDepthBoneGpuRefreshBatch(compat::Slice<DepthBoneGpuOffsetPacket> packets,
        compat::RetainedStringSlice reason, std::shared_ptr<actions::AsyncGroupAction> sink,
        actions::AsyncActionToken token) {
    if (packets.empty()) return;
    if (!token.valid() && sink) token = sink->asyncToken();
    auto batchId = nextDepthBoneGpuBatchId++;
    if (nextDepthBoneGpuBatchId == 0) nextDepthBoneGpuBatchId = 1;
    for (auto packet : packets) {
        depthBoneGpuSubmissionQueue.append(DepthBoneGpuQueuedJob{
            packet, reason, sink, batchId, packets.size(), token});
        bone_status::ngDepthBoneUpdateQueued(batchId, packet.root, packet.target, packet.parameter,
            packet.keypoint, reason.retainedString(), packet.staleRetryCount);
    }
}

void completeDepthBoneGpuJob(DepthBoneGpuRefreshJob job, nicxlive::core::math::Vec2Array offsets, bool valid) {
    depthBoneGpuCompletedJobs.append(DepthBoneGpuCompletedJob{job.packet, job.reason, job.actionSink,
        job.batchId, job.batchExpected, offsets, valid, job.actionToken});
}

void completeDepthBoneGpuJob(DepthBoneGpuQueuedJob job, nicxlive::core::math::Vec2Array offsets, bool valid) {
    depthBoneGpuCompletedJobs.append(DepthBoneGpuCompletedJob{job.packet, job.reason, job.actionSink,
        job.batchId, job.batchExpected, offsets, valid, job.actionToken});
}

bool processDepthBoneGpuSubmissionQueue() {
    if (depthBoneGpuSubmissionQueue.empty()) return false;
    bool submitted = false;
    std::size_t count = 0;
    while (!depthBoneGpuSubmissionQueue.empty() && count < DepthBoneGpuSubmissionsPerFrame) {
        auto queued = workerJobElement(depthBoneGpuSubmissionQueue, 0, 2058);
        if (batchIsCanceled(queued.batchId)) {
            depthBoneGpuSubmissionQueue = workerJobSlice(
                depthBoneGpuSubmissionQueue, 1, depthBoneGpuSubmissionQueue.size(), 2060);
            cleanupCanceledDepthBoneGpuBatch(queued.batchId);
            continue;
        }
        compat::RetainedStringSlice staleReason;
        if (!isDepthBoneGpuQueuedJobCurrent(queued, staleReason)) {
            auto target = queued.packet.target;
            depthBoneDebugLog("[DepthBoneRefresh] discard queued stale batch: batch=%s target=%s key=(%s,%s) reason=%s",
                queued.batchId, target ? target->getName()
                    : nicxlive::core::common::NullableString::fromStaticLiteral("(null)"),
                queued.packet.keypoint.x, queued.packet.keypoint.y, staleReason);
            cancelDepthBoneGpuBatch(queued.batchId, staleReason);
            continue;
        }
        if (depthBoneGpuRefreshJobs.size() >= DepthBoneGpuMaxInFlight) break;
        depthBoneGpuSubmissionQueue = workerJobSlice(
            depthBoneGpuSubmissionQueue, 1, depthBoneGpuSubmissionQueue.size(), 2080);
        compat::RetainedStringSlice error;
        if (!submitDepthBoneGpuRefreshJob(queued, error))
            reportAsyncFailure("Depth Bone GPU async submit failed: %s", error, 2086);
        else submitted = true;
        ++count;
    }
    return submitted;
}

bool processDepthBoneGpuCompletedJobs() {
    bool changed = false;
    compat::Slice<std::uint32_t> batchIds;
    auto completed = depthBoneGpuCompletedJobs;
    for (auto job : completed) {
        bool found = false;
        for (auto batchId : batchIds) {
            if (batchId == job.batchId) { found = true; break; }
        }
        if (!found) batchIds.append(job.batchId);
    }
    std::size_t processed = 0;
    for (auto batchId : batchIds) {
        if (processed >= DepthBoneGpuCompletedBatchesPerFrame) break;
        changed = applyDepthBoneGpuCompletedBatch(batchId) || changed;
        ++processed;
    }
    return changed;
}

bool processDepthBoneGpuRefreshJobs() {
    if (depthBoneGpuRefreshJobs.empty()) return false;
    bool changed = false;
    std::size_t processed = 0;
    std::size_t i = 0;
    while (i < depthBoneGpuRefreshJobs.size() && processed < DepthBoneGpuReadbacksPerFrame) {
        auto job = workerJobElement(depthBoneGpuRefreshJobs, i, 2584);
        bone_gpu_async::NgDepthBoneGpuAsyncResult result;
        compat::RetainedStringSlice error;
        if (!bone_gpu_async::ngPollDepthBoneGpuAsync(job.jobId, result, error))
            reportAsyncFailure("Depth Bone GPU async poll failed: %s", error, 2591);
        if (!result.ready) { ++i; continue; }
        if (batchIsCanceled(job.batchId)) {
            removeWorkerJobAt(depthBoneGpuRefreshJobs, i, 2599);
            cleanupCanceledDepthBoneGpuBatch(job.batchId);
            ++processed;
            continue;
        }
        compat::RetainedStringSlice staleReason;
        if (!isDepthBoneGpuRefreshJobCurrent(job, staleReason)) {
            bone_status::ngDepthBoneUpdateStale(job.batchId, job.packet.root, job.packet.target,
                staleReason.retainedString());
            requeueStaleDepthBoneGpuBatch(job, staleReason);
            ++processed;
            continue;
        }
        auto offsets = ngDepthBoneGpuReadbackToOffsets(result.xs, result.ys);
        completeDepthBoneGpuJob(job, offsets, true);
        removeWorkerJobAt(depthBoneGpuRefreshJobs, i, 2617);
        ++processed;
    }
    return processDepthBoneGpuCompletedJobs() || changed;
}

bool processDepthBoneAllKeypointJobs() {
    if (depthBoneAllKeypointJobs.empty()) return false;
    std::size_t remaining = DepthBoneAllKeypointsPerFrame;
    bool changed = false;
    std::size_t i = 0;
    while (i < depthBoneAllKeypointJobs.size()) {
        // The original interior pointer retains its GC allocation through the
        // immediate callback. Keep the real native backing alive too.
        auto storage = depthBoneAllKeypointJobs;
        auto* job = &workerJobElement(storage, i, 3586);
        if (!job->root || !isLiveDepthRigRoot(job->root) || !job->parameter || !project::incActivePuppet()) {
            removeWorkerJobAt(depthBoneAllKeypointJobs, i, 3588);
            continue;
        }
        if (job->settleFrames > 0) { --job->settleFrames; ++i; continue; }
        compat::Slice<compat::Vec2u> chunk;
        const auto closest = compat::ngRequireObject(job->parameter)->findClosestKeypoint();
        compat::Vec2u visual{closest.x, closest.y};
        std::size_t neededCount = compat::ngRequireObject(job->parameter)->isVec2 ? 4 : 2;
        auto keypoints = job->keypoints;
        for (auto kp : keypoints) {
            if (neededCount == 0) break;
            if (kp.x < visual.x || kp.x > static_cast<std::uint32_t>(visual.x + 1u)
                    || kp.y < visual.y || kp.y > static_cast<std::uint32_t>(visual.y + 1u)) continue;
            auto key = keypointKey(kp);
            if (job->processed.contains(key.view())) continue;
            chunk.append(kp);
            job->processed[key] = true;
            --neededCount;
        }
        while (remaining > 0 && job->nextIndex < job->keypoints.size()) {
            auto kp = workerJobElement(job->keypoints, job->nextIndex++, 3611);
            auto key = keypointKey(kp);
            if (job->processed.contains(key.view())) continue;
            chunk.append(kp);
            job->processed[key] = true;
            --remaining;
        }
        if (!chunk.empty()) {
            changed = runWithDepthBoneRefreshActionSink(job->actionSink, [&] {
                return ngRefreshDepthBoneDeformKeypoints(job->root, job->parameter, chunk, visual,
                    job->reason, job->targetUuid);
            }) || changed;
        }
        if (job->processed.size() >= job->keypoints.size()) {
            removeWorkerJobAt(depthBoneAllKeypointJobs, i, 3627);
            continue;
        }
        ++i;
        if (remaining == 0) break;
    }
    return changed;
}

} // namespace nijigenerate::commands::depth::bone::detail
