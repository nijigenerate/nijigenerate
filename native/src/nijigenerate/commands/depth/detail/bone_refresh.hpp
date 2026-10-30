#pragma once

#include "bone_job_workers.hpp"
#include "bone_dirty_providers.hpp"

namespace nijigenerate::commands::depth::bone::detail {

// Real queue/supersede definitions are in bone_refresh.cpp.
void cancelSupersededDepthBoneGpuWork(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, std::uint32_t targetUuid,
    bool matchKeypoint = false, compat::Vec2u keypoint = {});
bool enqueueDepthBoneAllKeypoints(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::RetainedStringSlice reason,
    std::uint32_t targetUuid = 0, bool settleBeforeDispatch = false);

} // namespace nijigenerate::commands::depth::bone::detail
