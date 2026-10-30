#include "detail/bone_dirty_state.hpp"

namespace nijigenerate::commands::depth::bone::detail {

// Ordinary D module state starts independently in each thread. These are actual
// retained objects and value fields, not UUID caches or process-shared mirrors.
thread_local std::shared_ptr<ext::nodes::ExDepthRigRoot> lastDepthBoneDirtyRoot;
thread_local std::shared_ptr<nicxlive::core::param::Parameter> lastDepthBoneDirtyParameter;
thread_local compat::Vec2u lastDepthBoneDirtyKeypoint{};
thread_local std::shared_ptr<actions::AsyncGroupAction> depthBoneRefreshActionSink;
thread_local std::size_t depthBoneRefreshSuppressed = 0;

} // namespace nijigenerate::commands::depth::bone::detail
