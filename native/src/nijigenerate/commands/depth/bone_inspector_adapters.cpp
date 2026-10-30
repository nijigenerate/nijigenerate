#include "bone_commands.hpp"
#include <nijigenerate/panels/inspector/griddeform_depth_dispatch.hpp>
#include <nijigenerate/panels/inspector/depthbone_dispatch.hpp>

namespace nijigenerate::panels::inspector::griddeform {

void ngRunGridDepthBoneCommand(commands::depth::bone::DepthBoneCommand id,
        std::shared_ptr<commands::Context> context, std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
        std::shared_ptr<nicxlive::core::nodes::Node> target, std::shared_ptr<ext::nodes::ExDepthBone> bone,
        compat::RetainedString text, float scale) {
    using namespace commands::depth::bone;
    using Id = DepthBoneCommand;
    switch (id) {
    case Id::AddStandardDepthSkeleton:
        ngRunDepthBoneCommand<Id::AddStandardDepthSkeleton>(context, root, scale);
        return;
    case Id::PreviewDepthBoneInfluence:
        ngRunDepthBoneCommand<Id::PreviewDepthBoneInfluence>(context, root, target, bone);
        return;
    case Id::RemoveDepthBoneSource:
        ngRunDepthBoneCommand<Id::RemoveDepthBoneSource>(context, root, target, bone);
        return;
    case Id::AddDepthBoneSource:
        ngRunDepthBoneCommand<Id::AddDepthBoneSource>(context, root, target, bone);
        return;
    case Id::SetDepthBoneSourceSettings:
        ngRunDepthBoneCommand<Id::SetDepthBoneSourceSettings>(context, root, target, bone, text);
        return;
    case Id::PreviewDepthBoneDeform:
        ngRunDepthBoneCommand<Id::PreviewDepthBoneDeform>(context, root,
            compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>{target});
        return;
    case Id::ApplyDepthBoneDeform:
        ngRunDepthBoneCommand<Id::ApplyDepthBoneDeform>(context, root,
            compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>{target});
        return;
    case Id::SetDepthBoneInfluenceRule:
        ngRunDepthBoneCommand<Id::SetDepthBoneInfluenceRule>(context, root, target, text);
        return;
    case Id::GetDepthBoneInfluenceRule:
        ngRunDepthBoneCommand<Id::GetDepthBoneInfluenceRule>(context, root, target);
        return;
    default:
        throw std::invalid_argument("Unsupported GridDeformer depth bone command");
    }
}

} // namespace nijigenerate::panels::inspector::griddeform

namespace nijigenerate::panels::inspector::depthbone {

void ngRunDepthBoneInspectorCommand(commands::depth::bone::DepthBoneCommand id,
        std::shared_ptr<commands::Context> context, std::shared_ptr<nicxlive::core::nodes::Node> target,
        nicxlive::core::math::Vec3 head, nicxlive::core::math::Vec3 tail, float roll,
        compat::RetainedString constraint) {
    using namespace commands::depth::bone;
    using Id = DepthBoneCommand;
    switch (id) {
    case Id::FitDepthRigRootZToDepth:
        ngRunDepthBoneCommand<Id::FitDepthRigRootZToDepth>(context, target);
        return;
    case Id::FitDepthBoneZToDepth:
        ngRunDepthBoneCommand<Id::FitDepthBoneZToDepth>(context, target);
        return;
    case Id::AddStandardDepthSkeleton:
        ngRunDepthBoneCommand<Id::AddStandardDepthSkeleton>(context, target, 1.0f);
        return;
    case Id::AddStandardDepthParameters:
        ngRunDepthBoneCommand<Id::AddStandardDepthParameters>(context, target);
        return;
    case Id::SetDepthBoneRest:
        ngRunDepthBoneCommand<Id::SetDepthBoneRest>(context, target,
            compat::Slice<float>{head.x, head.y, head.z}, compat::Slice<float>{tail.x, tail.y, tail.z}, roll);
        return;
    case Id::SetDepthBoneConstraint:
        ngRunDepthBoneCommand<Id::SetDepthBoneConstraint>(context, target, constraint);
        return;
    default:
        throw std::invalid_argument("Unsupported DepthBone Inspector command");
    }
}

} // namespace nijigenerate::panels::inspector::depthbone
