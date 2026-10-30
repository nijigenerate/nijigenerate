#pragma once
#include "depthbone.hpp"
#include "depthbone_dispatch.hpp"
#include <nijigenerate/commands/depth/detail/bone_fit.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
namespace nijigenerate::panels::inspector::depthbone {
template<ModelEditSubMode mode,class T> requires (
    mode==ModelEditSubMode::Layout &&
    (std::derived_from<T,ext::nodes::ExDepthRigRoot>||std::derived_from<T,ext::nodes::ExDepthBone>))
void NodeInspector<mode,T>::run() {
    using namespace widgets;
    using core::translation::ngTranslateCString;
    const auto translated=[]<std::size_t N>(const char (&text)[N]) {
        return compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral(text)));
    };
    namespace depth=commands::depth::bone;using Id=depth::DepthBoneCommand;
    if(this->targets.empty())return;
    const auto self=std::static_pointer_cast<NodeInspector>(this->shared_from_this());
    auto context=[&] {
        auto result=std::allocate_shared<commands::Context>(
            nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
        result->inspectors(commands::Context::Inspectors{self});
        result->nodes(static_cast<commands::Context::Nodes>(Selection<Node>(this->targets)));return result;
    };
    if constexpr(std::derived_from<T,ext::nodes::ExDepthRigRoot>) {
        auto root=this->targets[0];
        if(ngBeginCategory(ngTranslateCString("Depth Rig").data())) {
            {
                const auto format=ngTranslateCString("Bones: %d");
                const auto count=static_cast<int>(compat::ngRequireObject(root)->depthBones().size());
                ImGui::Text(format.data(),count);
            }
            {
                const auto format=ngTranslateCString("Bindings: %d");
                const auto count=static_cast<int>(compat::ngRequireObject(root)->bindings.size());
                ImGui::Text(format.data(),count);
            }
            if(ImGui::Button(ngTranslateCString("Fit Z to Depth").data())) {
                auto ctx=context();auto targets=this->targets;
                for(auto target:targets)ngRunDepthBoneInspectorCommand(Id::FitDepthRigRootZToDepth,ctx,target);
            }
            ngTooltip(translated("Set descendant DepthBone translation.t.z values to the scaled depth values used by Depth Bone deformation."));
            if(ImGui::Button(ngTranslateCString("Add Standard Skeleton").data()))
                ngRunDepthBoneInspectorCommand(Id::AddStandardDepthSkeleton,context(),root);
            if(ImGui::Button(ngTranslateCString("Add Standard Parameters").data()))
                ngRunDepthBoneInspectorCommand(Id::AddStandardDepthParameters,context(),root);
        }
        ngEndCategory();
    } else {
        auto bone=this->targets[0];
        if(ngBeginCategory(ngTranslateCString("Depth Bone").data())) {
            {
                const auto input=compat::RetainedStringSlice(compat::ngRequireObject(bone)->boneId);
                const compat::SourceCString boneId(input);
                compat::detail::ngSourceCStringPostconditions(input.view(),boneId.data());
                ImGui::Text("%s",boneId.data());
            }
            if(ngButtonColored("\ue5d5")) {
                std::shared_ptr<Node> cursor=bone;
                while(cursor) {
                    if(auto root=std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(cursor)) {
                        depth::ngMarkDepthBoneDirtyAllKeypointsForArmedParameter(root,
                            compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("Force Depth Bone Refresh")));
                        break;
                    }
                    cursor=compat::ngRequireObject(cursor)->parentPtr();
                }
            }
            ngTooltip(translated("Force refresh all Depth Bone target keypoints"));
            float localDepth=std::numeric_limits<float>::quiet_NaN(),rootDepth=localDepth;
            if(depth::ngDepthRigNodeCurrentScaledDepth(bone,localDepth,rootDepth)) {
                ImGui::Text(ngTranslateCString("Current Depth: %.2f").data(),localDepth);
                ImGui::Text(ngTranslateCString("Root Depth: %.2f").data(),rootDepth);
            }
            if(ImGui::Button(ngTranslateCString("Fit Z to Depth").data())) {
                auto ctx=context();auto targets=this->targets;
                for(auto target:targets)ngRunDepthBoneInspectorCommand(Id::FitDepthBoneZToDepth,ctx,target);
            }
            ngTooltip(translated("Set translation.t.z to the scaled depth value used by Depth Bone deformation."));
            float head[3]{compat::ngRequireObject(bone)->restHead.x,
                compat::ngRequireObject(bone)->restHead.y,compat::ngRequireObject(bone)->restHead.z};
            float tail[3]{compat::ngRequireObject(bone)->restTail.x,
                compat::ngRequireObject(bone)->restTail.y,compat::ngRequireObject(bone)->restTail.z};
            float roll=compat::ngRequireObject(bone)->restRoll;
            bool changed=false;ImGui::PushItemWidth(160);
            changed=ImGui::DragFloat3(ngTranslateCString("Rest Head Position").data(),head,1.f)||changed;
            changed=ImGui::DragFloat3(ngTranslateCString("Rest Tail Position").data(),tail,1.f)||changed;
            changed=ImGui::DragFloat(ngTranslateCString("Rest Roll Angle").data(),&roll,.01f)||changed;ImGui::PopItemWidth();
            if(changed)ngRunDepthBoneInspectorCommand(Id::SetDepthBoneRest,context(),bone,
                {head[0],head[1],head[2]},{tail[0],tail[1],tail[2]},roll);
            bool lockRotation=compat::ngRequireObject(bone)->lockRotation;
            bool lockTranslation=compat::ngRequireObject(bone)->lockTranslation;
            bool allowParent=compat::ngRequireObject(bone)->allowParentToTargets,changedConstraint=false;
            changedConstraint=ngCheckbox(ngTranslateCString("Lock Rotation").data(),&lockRotation)||changedConstraint;
            changedConstraint=ngCheckbox(ngTranslateCString("Lock Translation").data(),&lockTranslation)||changedConstraint;
            changedConstraint=ngCheckbox(ngTranslateCString("Allow Parent to Targets").data(),&allowParent)||changedConstraint;
            ngTooltip(translated("When enabled, parent bone rotation and translation also affect targets bound to this bone."));
            if(changedConstraint) {
                const auto constraintContext=context();
                const auto constraint=[&] {
                    try {
                        std::string json="{\"lockRotation\":";json+=lockRotation?"true":"false";
                        json+=",\"lockTranslation\":";json+=lockTranslation?"true":"false";
                        json+=",\"allowParentToTargets\":";json+=allowParent?"true":"false";json+='}';
                        return compat::RetainedString(std::move(json));
                    } catch(const std::bad_alloc&) {
                        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                    } catch(const std::length_error&) {
                        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                    }
                }();
                ngRunDepthBoneInspectorCommand(Id::SetDepthBoneConstraint,constraintContext,bone,{},{},0,
                    constraint);
            }
        }
        ngEndCategory();
    }
}
}
