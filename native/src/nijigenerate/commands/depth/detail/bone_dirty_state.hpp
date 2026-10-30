#pragma once

#include "bone_refresh_state.hpp"
#include "../bone.hpp"
#include <nijigenerate/compat/slice.hpp>

namespace nijigenerate::commands::depth::bone::detail {

struct DepthBoneDirtyRequest {
    std::shared_ptr<ext::nodes::ExDepthRigRoot> root;
    std::shared_ptr<nicxlive::core::param::Parameter> parameter;
    compat::Vec2u keypoint{};
    std::uint32_t targetUuid = 0;
    DepthBoneDirtyScope dirtyScope = DepthBoneDirtyScope::Keypoint;
    compat::RetainedStringSlice reason;
    std::shared_ptr<actions::AsyncGroupAction> actionSink;
    bool settleBeforeDispatch = false;
    actions::AsyncActionToken actionToken;
};
extern thread_local compat::Slice<DepthBoneDirtyRequest> depthBoneDirtyRequests;
bool sameDirtyParameter(DepthBoneDirtyRequest request, std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, std::uint32_t targetUuid);

} // namespace nijigenerate::commands::depth::bone::detail
