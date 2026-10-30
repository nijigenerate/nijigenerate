#include "detail/bone_flush.hpp"
#include "detail/bone_action_scope.hpp"
#include "detail/bone_dirty_providers.hpp"
#include "detail/bone_debug_log.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <exception>
#include <utility>

namespace nijigenerate::commands::depth::bone::detail {

bool hasProcessedAllKeypoints(compat::Slice<DepthBoneDirtyRequest> processed, DepthBoneDirtyRequest request) {
    for (auto done : processed) {
        if (done.dirtyScope != DepthBoneDirtyScope::AllKeypoints || done.root != request.root) continue;
        if (done.actionSink != request.actionSink) continue;
        if (done.targetUuid != request.targetUuid) continue;
        if (!done.parameter || done.parameter == request.parameter) return true;
    }
    return false;
}

bool hasRootAllKeypointsRequest(compat::Slice<DepthBoneDirtyRequest> requests, DepthBoneDirtyRequest request) {
    if (!request.parameter) return false;
    for (auto candidate : requests) {
        if (candidate.actionSink != request.actionSink) continue;
        if (candidate.targetUuid != request.targetUuid) continue;
        if (candidate.root == request.root && !candidate.parameter
                && candidate.dirtyScope == DepthBoneDirtyScope::AllKeypoints) return true;
    }
    return false;
}

void flushDepthBoneDirty() {
    if (!depthBoneDirtyRequests.empty()) {
        auto requests = depthBoneDirtyRequests;
        depthBoneDirtyRequests.resize(0);
        try {
            depthBoneDebugLog("[DepthBoneRefresh] flush: requests=%s", requests.size());
            compat::Slice<DepthBoneDirtyRequest> processed;
            for (auto request : requests) {
                if (!isLiveDepthRigRoot(request.root)) continue;
                if (hasRootAllKeypointsRequest(requests, request)) continue;
                if (request.dirtyScope == DepthBoneDirtyScope::Keypoint
                        && hasProcessedAllKeypoints(processed, request)) continue;
                runWithDepthBoneRefreshActionSink(request.actionSink, [&request] {
                    if (request.dirtyScope == DepthBoneDirtyScope::AllKeypoints)
                        return ngRefreshDepthBoneDeformAllKeypoints(request.root, request.parameter,
                            request.keypoint, request.reason, request.targetUuid, request.settleBeforeDispatch);
                    return ngRefreshDepthBoneDeform(request.root, request.parameter, request.keypoint,
                        request.reason, request.targetUuid);
                });
                processed.append(request);
            }
        } catch (...) {
            // Run scope(failure) outside a noexcept destructor. Preserve the
            // original Throwable if this cleanup throws a collateral Exception;
            // a collateral Error instead bypasses an original Exception.
            const auto failure = std::current_exception();
            const auto failDetachedOwners = [&requests] {
                for (auto request : requests) {
                    if (request.actionSink && request.actionToken.active()
                            && request.actionSink->pendingAsyncCount() > 0)
                        request.actionSink->markAsyncFailed();
                }
            };
            try {
                std::rethrow_exception(failure);
            } catch (compat::Throwable& first) {
                auto retained = ::nicxlive::core::d_runtime::ngRetainCurrentThrowable(first);
                try {
                    failDetachedOwners();
                } catch (compat::Throwable& second) {
                    auto collateral = ::nicxlive::core::d_runtime::ngRetainCurrentThrowable(second);
                    if (!dynamic_cast<compat::ThrowableError*>(&first)) {
                        if (auto error = dynamic_cast<compat::ThrowableError*>(&second)) {
                            error->bypassedException = std::move(retained);
                            throw;
                        }
                    }
                    compat::Throwable::chainTogether(std::move(retained), std::move(collateral));
                }
                std::rethrow_exception(failure);
            } catch (...) {
                failDetachedOwners();
                throw;
            }
        }
    }
    processDepthBoneAllKeypointJobs();
    processDepthBoneGpuSubmissionQueue();
    processDepthBoneGpuRefreshJobs();
    processDepthBoneGpuCompletedJobs();
}

} // namespace nijigenerate::commands::depth::bone::detail
