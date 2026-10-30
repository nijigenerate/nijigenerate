#include "detail/bone_job_state.hpp"

namespace nijigenerate::commands::depth::bone::detail {

thread_local compat::Slice<DepthBoneAllKeypointJob> depthBoneAllKeypointJobs;
thread_local compat::Slice<DepthBoneGpuQueuedJob> depthBoneGpuSubmissionQueue;
thread_local compat::Slice<DepthBoneGpuRefreshJob> depthBoneGpuRefreshJobs;
thread_local compat::Slice<DepthBoneGpuCompletedJob> depthBoneGpuCompletedJobs;
thread_local DepthBoneCanceledBatchMap canceledDepthBoneGpuBatches;
thread_local std::uint32_t nextDepthBoneGpuBatchId = 1;

} // namespace nijigenerate::commands::depth::bone::detail
