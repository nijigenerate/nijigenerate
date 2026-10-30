#pragma once

#include "../bone_gpu_packet.hpp"
#include "bone_dirty_state.hpp"
#include <nijigenerate/core/selector/tokenizer.hpp>

namespace nijigenerate::commands::depth::bone {

bool ngHasPendingDepthBoneRefreshForSink(std::shared_ptr<actions::GroupAction> sink);
std::size_t ngPendingDepthBoneRefreshWorkForSink(std::shared_ptr<actions::GroupAction> sink);

namespace detail {

struct DepthBoneAllKeypointJob {
    std::shared_ptr<ext::nodes::ExDepthRigRoot> root;
    std::shared_ptr<nicxlive::core::param::Parameter> parameter;
    std::uint32_t targetUuid = 0;
    compat::Slice<compat::Vec2u> keypoints;
    core::selector::SelectorMap<compat::RetainedStringSlice, bool,
        core::selector::SelectorStringHash, core::selector::SelectorStringEqual> processed;
    std::size_t nextIndex = 0;
    compat::RetainedStringSlice reason;
    std::shared_ptr<actions::AsyncGroupAction> actionSink;
    std::size_t settleFrames = 0;
    actions::AsyncActionToken actionToken;

    DepthBoneAllKeypointJob() = default;
    DepthBoneAllKeypointJob(const DepthBoneAllKeypointJob&) = default;
    DepthBoneAllKeypointJob& operator=(const DepthBoneAllKeypointJob&) = default;
    // Moving a native container entry must retain the original D record's owners.
    DepthBoneAllKeypointJob(DepthBoneAllKeypointJob&& other)
        : DepthBoneAllKeypointJob(static_cast<const DepthBoneAllKeypointJob&>(other)) {}
    DepthBoneAllKeypointJob& operator=(DepthBoneAllKeypointJob&& other) {
        return operator=(static_cast<const DepthBoneAllKeypointJob&>(other));
    }
};

struct DepthBoneGpuRefreshJob {
    std::uint32_t jobId = 0;
    DepthBoneGpuOffsetPacket packet;
    compat::RetainedStringSlice reason;
    std::shared_ptr<actions::AsyncGroupAction> actionSink;
    std::uint32_t batchId = 0;
    std::size_t batchExpected = 0;
    actions::AsyncActionToken actionToken;
};

struct DepthBoneGpuQueuedJob {
    DepthBoneGpuOffsetPacket packet;
    compat::RetainedStringSlice reason;
    std::shared_ptr<actions::AsyncGroupAction> actionSink;
    std::uint32_t batchId = 0;
    std::size_t batchExpected = 0;
    actions::AsyncActionToken actionToken;
};

struct DepthBoneGpuCompletedJob {
    DepthBoneGpuOffsetPacket packet;
    compat::RetainedStringSlice reason;
    std::shared_ptr<actions::AsyncGroupAction> actionSink;
    std::uint32_t batchId = 0;
    std::size_t batchExpected = 0;
    nicxlive::core::math::Vec2Array offsets;
    bool valid = false;
    actions::AsyncActionToken actionToken;
};

using DepthBoneCanceledBatchMap = core::selector::SelectorMap<std::uint32_t, bool>;
extern thread_local compat::Slice<DepthBoneAllKeypointJob> depthBoneAllKeypointJobs;
extern thread_local compat::Slice<DepthBoneGpuQueuedJob> depthBoneGpuSubmissionQueue;
extern thread_local compat::Slice<DepthBoneGpuRefreshJob> depthBoneGpuRefreshJobs;
extern thread_local compat::Slice<DepthBoneGpuCompletedJob> depthBoneGpuCompletedJobs;
extern thread_local DepthBoneCanceledBatchMap canceledDepthBoneGpuBatches;
extern thread_local std::uint32_t nextDepthBoneGpuBatchId;

bool depthBoneGpuBatchHasPendingWork(std::uint32_t batchId);
void cleanupCanceledDepthBoneGpuBatch(std::uint32_t batchId);
void cancelDepthBoneGpuBatch(std::uint32_t batchId, compat::RetainedStringSlice statusDetail = {},
    bool settleOwners = true);
void finishCanceledDepthBoneGpuTask(std::shared_ptr<actions::AsyncGroupAction> sink,
    actions::AsyncActionToken token);

} // namespace detail
} // namespace nijigenerate::commands::depth::bone
