#include "detail/bone_job_state.hpp"
#include "detail/bone_action_scope.hpp"
#include "detail/bone_dirty_providers.hpp"
#include "detail/bone_flush.hpp"
#include "bone_gpu_async.hpp"
#include "bone_status.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <stdexcept>
#include <vector>

namespace nijigenerate::commands::depth::bone {
namespace detail {
namespace {

// Actual prefix/suffix concatenation replaces the descriptor, retaining old
// backing aliases. Never erase/shift records in the original shared storage.
template<class Record> void removeJobAt(compat::Slice<Record>& jobs, std::size_t index,
        std::size_t sourceLine = 0) {
    const auto slice = [&](std::size_t first, std::size_t last) {
        if (!sourceLine) return compat::ngArraySlice(jobs, first, last);
        const auto length = jobs.size();
        if (first > last || last > length) throw compat::ngStaticError<compat::ArraySliceError>(
            first, last, length,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), sourceLine);
        return jobs.slice(first, last);
    };
    auto before = slice(0, index);
    auto after = slice(index + 1, jobs.size());
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

template<class Array> decltype(auto) sourceJobElement(Array& jobs, std::size_t index, std::size_t sourceLine) {
    const auto length = jobs.size();
    if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), sourceLine);
    return jobs[index];
}

} // namespace

bool depthBoneGpuBatchHasPendingWork(std::uint32_t batchId) {
    auto queued = depthBoneGpuSubmissionQueue;
    for (auto job : queued) if (job.batchId == batchId) return true;
    auto refresh = depthBoneGpuRefreshJobs;
    for (auto job : refresh) if (job.batchId == batchId) return true;
    auto completed = depthBoneGpuCompletedJobs;
    for (auto job : completed) if (job.batchId == batchId) return true;
    return false;
}

void cleanupCanceledDepthBoneGpuBatch(std::uint32_t batchId) {
    if (!canceledDepthBoneGpuBatches.contains(batchId)) return;
    if (!depthBoneGpuBatchHasPendingWork(batchId)) canceledDepthBoneGpuBatches.erase(batchId);
}

void finishCanceledDepthBoneGpuTask(std::shared_ptr<actions::AsyncGroupAction> sink,
        actions::AsyncActionToken token) {
    if (sink && token.acceptsCompletion()) sink->markAsyncFinished();
}

void cancelDepthBoneGpuBatch(std::uint32_t batchId, compat::RetainedStringSlice statusDetail,
        bool settleOwners) {
    struct Owner {
        std::shared_ptr<actions::AsyncGroupAction> action;
        actions::AsyncActionToken token;
    };
    compat::Slice<Owner> owners;
    const auto retainOwner = [&owners, &settleOwners](std::shared_ptr<actions::AsyncGroupAction> action,
            actions::AsyncActionToken token) {
        if (!settleOwners || !action || !token.acceptsCompletion()) return;
        for (auto retainedOwners = owners; auto owner : retainedOwners)
            if (owner.action == action && owner.token.generation() == token.generation()) return;
        owners.append(Owner{action, token});
    };
    canceledDepthBoneGpuBatches[batchId] = true;
    bone_status::ngDepthBoneUpdateBatchCanceled(batchId, statusDetail.retainedString());
    std::size_t i = 0;
    while (i < depthBoneGpuSubmissionQueue.size()) {
        if (sourceJobElement(depthBoneGpuSubmissionQueue, i, 1631).batchId == batchId) {
            retainOwner(sourceJobElement(depthBoneGpuSubmissionQueue, i, 1632).actionSink,
                sourceJobElement(depthBoneGpuSubmissionQueue, i, 1633).actionToken);
            removeJobAt(depthBoneGpuSubmissionQueue, i, 1635);
            continue;
        }
        ++i;
    }
    i = 0;
    while (i < depthBoneGpuRefreshJobs.size()) {
        if (sourceJobElement(depthBoneGpuRefreshJobs, i, 1643).batchId == batchId) {
            retainOwner(sourceJobElement(depthBoneGpuRefreshJobs, i, 1644).actionSink,
                sourceJobElement(depthBoneGpuRefreshJobs, i, 1645).actionToken);
            bone_gpu_async::ngCancelDepthBoneGpuAsync(sourceJobElement(depthBoneGpuRefreshJobs, i, 1646).jobId);
            removeJobAt(depthBoneGpuRefreshJobs, i, 1648);
            continue;
        }
        ++i;
    }
    i = 0;
    while (i < depthBoneGpuCompletedJobs.size()) {
        if (sourceJobElement(depthBoneGpuCompletedJobs, i, 1656).batchId == batchId) {
            retainOwner(sourceJobElement(depthBoneGpuCompletedJobs, i, 1657).actionSink,
                sourceJobElement(depthBoneGpuCompletedJobs, i, 1658).actionToken);
            removeJobAt(depthBoneGpuCompletedJobs, i, 1660);
            continue;
        }
        ++i;
    }
    cleanupCanceledDepthBoneGpuBatch(batchId);
    for (auto owner : owners) finishCanceledDepthBoneGpuTask(owner.action, owner.token);
}

void cancelDepthBoneRefreshForAction(std::shared_ptr<actions::AsyncGroupAction> action) {
    if (!action) return;
    std::size_t i = 0;
    while (i < depthBoneDirtyRequests.size()) {
        if (compat::ngArrayAt(depthBoneDirtyRequests, i).actionSink == action) {
            removeJobAt(depthBoneDirtyRequests, i);
            continue;
        }
        ++i;
    }
    i = 0;
    while (i < depthBoneAllKeypointJobs.size()) {
        if (compat::ngArrayAt(depthBoneAllKeypointJobs, i).actionSink == action) {
            removeJobAt(depthBoneAllKeypointJobs, i);
            continue;
        }
        ++i;
    }
    DepthBoneCanceledBatchMap batches;
    const auto detach = [&](auto jobs) {
        for (auto& job : jobs) {
            if (job.actionSink != action) continue;
            batches[job.batchId] = true;
            job.actionSink = nullptr;
            job.actionToken = {};
        }
    };
    detach(depthBoneGpuSubmissionQueue);
    detach(depthBoneGpuRefreshJobs);
    detach(depthBoneGpuCompletedJobs);
    // Source AA key iteration has no sorted/insertion-order contract.
    for (const auto& batch : batches) cancelDepthBoneGpuBatch(batch.first);
}

bool mergeDepthBoneAsyncActions(std::shared_ptr<actions::AsyncGroupAction> current,
        std::shared_ptr<actions::AsyncGroupAction> incoming) {
    if (!current || !incoming) return false;
    cancelDepthBoneRefreshForAction(current);
    auto token = current->asyncToken();
    auto requests = depthBoneDirtyRequests;
    for (auto& request : requests) {
        if (request.actionSink != incoming) continue;
        request.actionSink = current;
        request.actionToken = token;
        setDepthBoneUpdateOwner(request.root, request.targetUuid, token);
    }
    auto allKeypoints = depthBoneAllKeypointJobs;
    for (auto& job : allKeypoints) {
        if (job.actionSink != incoming) continue;
        job.actionSink = current;
        job.actionToken = token;
        setDepthBoneUpdateOwner(job.root, job.targetUuid, token);
    }
    const auto transfer = [&](auto jobs) {
        for (auto& job : jobs) {
            if (job.actionSink != incoming) continue;
            job.actionSink = current;
            job.actionToken = token;
            bone_status::ngDepthBoneUpdateSetOwnerToken(job.packet.root, job.packet.target, token);
        }
    };
    transfer(depthBoneGpuSubmissionQueue);
    transfer(depthBoneGpuRefreshJobs);
    transfer(depthBoneGpuCompletedJobs);
    return true;
}

void abortDepthBoneGpuRefresh(compat::RetainedStringSlice statusDetail) {
    struct Owner {
        std::shared_ptr<actions::AsyncGroupAction> action;
        actions::AsyncActionToken token;
    };
    compat::Slice<Owner> owners;
    const auto retainOwner = [&owners](std::shared_ptr<actions::AsyncGroupAction> action,
            actions::AsyncActionToken token) {
        if (!action || !token.active()) return;
        for (auto retainedOwners = owners; auto owner : retainedOwners)
            if (owner.action == action && owner.token.generation() == token.generation()) return;
        owners.append(Owner{action, token});
    };
    const auto capture = [&](auto jobs) {
        for (auto job : jobs)
            retainOwner(job.actionSink, job.actionToken);
    };
    capture(depthBoneDirtyRequests);
    capture(depthBoneAllKeypointJobs);
    capture(depthBoneGpuSubmissionQueue);
    capture(depthBoneGpuRefreshJobs);
    capture(depthBoneGpuCompletedJobs);
    if (!statusDetail.empty()) bone_status::ngDepthBoneUpdateAbort(statusDetail.retainedString());
    auto refresh = depthBoneGpuRefreshJobs;
    for (auto job : refresh) bone_gpu_async::ngCancelDepthBoneGpuAsync(job.jobId);
    depthBoneDirtyRequests = {};
    depthBoneAllKeypointJobs = {};
    depthBoneGpuSubmissionQueue = {};
    depthBoneGpuRefreshJobs = {};
    depthBoneGpuCompletedJobs = {};
    canceledDepthBoneGpuBatches = {};
    for (auto owner : owners)
        if (owner.token.active() && owner.action->pendingAsyncCount() > 0) owner.action->markAsyncFailed();
}

} // namespace detail

bool ngHasPendingDepthBoneRefresh() {
    using namespace detail;
    return !depthBoneDirtyRequests.empty() || !depthBoneAllKeypointJobs.empty()
        || !depthBoneGpuSubmissionQueue.empty() || !depthBoneGpuRefreshJobs.empty()
        || !depthBoneGpuCompletedJobs.empty() || bone_gpu_async::ngPendingDepthBoneGpuAsyncJobCount() > 0;
}

bool ngHasPendingDepthBoneRefreshForSink(std::shared_ptr<actions::GroupAction> sink) {
    using namespace detail;
    const auto contains = [&](auto jobs) {
        for (auto job : jobs) if (job.actionSink == sink) return true;
        return false;
    };
    return contains(depthBoneDirtyRequests) || contains(depthBoneAllKeypointJobs)
        || contains(depthBoneGpuSubmissionQueue) || contains(depthBoneGpuRefreshJobs)
        || contains(depthBoneGpuCompletedJobs);
}

std::size_t ngPendingDepthBoneRefreshWorkForSink(std::shared_ptr<actions::GroupAction> sink) {
    using namespace detail;
    std::size_t result = 0;
    auto requests = depthBoneDirtyRequests;
    for (auto request : requests) if (request.actionSink == sink) ++result;
    auto allKeypoints = depthBoneAllKeypointJobs;
    for (auto job : allKeypoints) {
        if (job.actionSink != sink) continue;
        result += job.keypoints.size() > job.processed.size() ? job.keypoints.size() - job.processed.size() : 1;
    }
    const auto count = [&](auto jobs) {
        for (auto job : jobs) if (job.actionSink == sink) ++result;
    };
    count(depthBoneGpuSubmissionQueue);
    count(depthBoneGpuRefreshJobs);
    count(depthBoneGpuCompletedJobs);
    return result;
}

} // namespace nijigenerate::commands::depth::bone
