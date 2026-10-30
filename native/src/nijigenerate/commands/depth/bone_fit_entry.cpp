#include "detail/bone_fit.hpp"
#include "detail/bone_action_scope.hpp"
#include <nijigenerate/actions/depthbone.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/translation.hpp>

namespace nijigenerate::commands::depth::bone {

bool ngFitDepthRigNodeTranslationZToCurrentDepth(std::shared_ptr<nicxlive::core::nodes::Node> node) {
    using ext::nodes::ExDepthRigRoot;
    using ext::nodes::ExDepthBone;
    using actions::depthbone::DepthBoneFitZChangeAction;
    using core::tasks::ngSetStatus;
    using core::translation::ngTranslate;
    using namespace detail;
    if (auto root = std::dynamic_pointer_cast<ExDepthRigRoot>(node)) {
        compat::Slice<std::shared_ptr<ExDepthBone>> changedBones;
        compat::Slice<float> oldValues;
        compat::Slice<float> newValues;
        bool fitted = false;
        for (auto bone : root->depthBones()) {
            fitted = fitOneDepthBoneZToDepth(root, bone, changedBones, oldValues, newValues) || fitted;
        }
        if (!fitted) {
            ngSetStatus(ngTranslate("Fit Z to Depth failed: no usable depth samples were found."));
            return false;
        }
        if (!changedBones.empty())
            core::actionstack::incActionPush(sourceDepthBoneObject<DepthBoneFitZChangeAction>(
                root, changedBones, oldValues, newValues));
        ngSetStatus(!changedBones.empty()
            ? ngTranslate("Fit Z to Depth updated descendant DepthBone translation.t.z values.")
            : ngTranslate("Fit Z to Depth completed; descendant DepthBone translation.t.z values were already aligned."));
        return true;
    }
    if (auto bone = std::dynamic_pointer_cast<ExDepthBone>(node)) {
        auto root = findDepthRigRoot(bone);
        if (!root) return false;
        compat::Slice<std::shared_ptr<ExDepthBone>> changedBones;
        compat::Slice<float> oldValues;
        compat::Slice<float> newValues;
        if (!fitOneDepthBoneZToDepth(root, bone, changedBones, oldValues, newValues)) {
            ngSetStatus(ngTranslate("Fit Z to Depth failed: no usable depth sample was found for the DepthBone."));
            return false;
        }
        if (!changedBones.empty())
            core::actionstack::incActionPush(sourceDepthBoneObject<DepthBoneFitZChangeAction>(
                root, changedBones, oldValues, newValues));
        ngSetStatus(!changedBones.empty()
            ? ngTranslate("Fit Z to Depth updated the DepthBone translation.t.z value.")
            : ngTranslate("Fit Z to Depth completed; the DepthBone translation.t.z value was already aligned."));
        return true;
    }
    return false;
}

} // namespace nijigenerate::commands::depth::bone
