#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include "bone_dirty_state.hpp"

namespace nijigenerate::commands::depth::bone {

namespace detail {

// Dispatch is defined in bone_flush_dispatch.cpp, abort in bone_job_control.cpp
// and fatal logging in bone_fatal_log.cpp. Workers are in bone_job_workers.cpp;
// Refresh entries/supersede/all-key enqueue are in bone_refresh.cpp; actual
// packet source payload is in bone_packet_source.cpp, completed writeback in
// bone_completed_batch.cpp. The bound observer producer remains required.
// No successful empty flush, false pending or discarded failure substitute.
void flushDepthBoneDirty();
void abortDepthBoneGpuRefresh(compat::RetainedStringSlice statusDetail = {});
void writeDepthBoneGpuFatalLog(compat::RetainedStringSlice message);
bool hasProcessedAllKeypoints(compat::Slice<DepthBoneDirtyRequest> processed, DepthBoneDirtyRequest request);
bool hasRootAllKeypointsRequest(compat::Slice<DepthBoneDirtyRequest> requests, DepthBoneDirtyRequest request);
bool ngRefreshDepthBoneDeform(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
    compat::RetainedStringSlice reason, std::uint32_t targetUuid = 0);
bool ngRefreshDepthBoneDeformAllKeypoints(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
    compat::RetainedStringSlice reason, std::uint32_t targetUuid = 0, bool settleBeforeDispatch = false);
bool processDepthBoneAllKeypointJobs();
bool processDepthBoneGpuSubmissionQueue();
bool processDepthBoneGpuRefreshJobs();
bool processDepthBoneGpuCompletedJobs();

} // namespace detail
} // namespace nijigenerate::commands::depth::bone
