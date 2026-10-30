#pragma once

#include "bone_job_state.hpp"
#include "bone_flush.hpp"
#include <nijigenerate/compat/const_slice.hpp>

namespace nijigenerate::commands::depth::bone::detail {

inline constexpr std::size_t DepthBoneAllKeypointsPerFrame = 4;
inline constexpr std::size_t DepthBoneGpuSubmissionsPerFrame = 8;
inline constexpr std::size_t DepthBoneGpuReadbacksPerFrame = 2;
inline constexpr std::size_t DepthBoneGpuCompletedBatchesPerFrame = 1;
inline constexpr std::size_t DepthBoneGpuMaxInFlight = 8;

bool submitDepthBoneGpuRefreshJob(DepthBoneGpuQueuedJob queued, compat::RetainedStringSlice& error);
void enqueueDepthBoneGpuRefreshBatch(compat::Slice<DepthBoneGpuOffsetPacket> packets,
    compat::RetainedStringSlice reason, std::shared_ptr<actions::AsyncGroupAction> sink,
    actions::AsyncActionToken token = {});
void completeDepthBoneGpuJob(DepthBoneGpuRefreshJob job, nicxlive::core::math::Vec2Array offsets, bool valid);
void completeDepthBoneGpuJob(DepthBoneGpuQueuedJob job, nicxlive::core::math::Vec2Array offsets, bool valid);

compat::Slice<DepthBoneGpuOffsetPacket> depthBoneGpuBatchPackets(std::uint32_t batchId);
bool sameVec2Array(nicxlive::core::math::Vec2Array a, nicxlive::core::math::Vec2Array b);
bool sameFloatArray(compat::ConstSlice<float> a, compat::ConstSlice<float> b);
bool matrixApproxEqual(nicxlive::core::math::Mat4 a, nicxlive::core::math::Mat4 b);
// Freshness/regeneration have actual bodies in bone_job_freshness.cpp.
// Packet building, completed writeback and refresh have separate production units.
bool isDepthBoneGpuQueuedJobCurrent(DepthBoneGpuQueuedJob& job, compat::RetainedStringSlice& reason);
bool isDepthBoneGpuRefreshJobCurrent(DepthBoneGpuRefreshJob& job, compat::RetainedStringSlice& reason);
void requeueStaleDepthBoneGpuBatch(DepthBoneGpuRefreshJob job, compat::RetainedStringSlice reason);
bool applyDepthBoneGpuCompletedBatch(std::uint32_t batchId);
bool ngRefreshDepthBoneDeformKeypoints(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Slice<compat::Vec2u> keypoints,
    compat::Vec2u visualKeypoint, compat::RetainedStringSlice reason, std::uint32_t targetUuid = 0);

} // namespace nijigenerate::commands::depth::bone::detail
