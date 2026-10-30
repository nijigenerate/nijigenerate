#include "bone.hpp"
#include "detail/bone_startup_hooks.hpp"
#include <mutex>

namespace nijigenerate::commands::depth::bone {
namespace {
std::once_flag depthBoneModuleInitialized;
}

void ngInitDepthBoneModule() {
    std::call_once(depthBoneModuleInitialized, [] {
        actions::depthboneinvalidation::ngDepthBoneMutationHook = &ngDepthBoneMutationChanged;
        actions::ngClaimAsyncGroupActionHook = &detail::claimDepthBoneAsyncGroupAction;
    });
}

} // namespace nijigenerate::commands::depth::bone
