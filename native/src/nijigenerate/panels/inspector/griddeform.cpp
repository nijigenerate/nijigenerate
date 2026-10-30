#include "griddeform.hpp"
#include "griddeform_depth_dispatch.hpp"
#include "depthbone.hpp"
#include "node_run.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/actions/depthbone.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/commands/inspector/griddeform_commands.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <compat/source_allocator.hpp>
#include <cstring>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
namespace nijigenerate::panels::inspector::griddeform {
std::shared_ptr<commands::CommandResult> ngDispatchGridDynamicCommand(
    std::shared_ptr<commands::Context> context,bool& value) {
    using namespace commands::inspector::apply_node;
    return ::nijigenerate::commands::ngDispatchRegisteredCommand<GridDeformDynamicCommand>(
        ::nijigenerate::commands::inspector::apply_node::commands,
        InspectorNodeApplyCommand::GridDeformDynamic,std::move(context),
        {"InspectorNodeApplyCommand.GridDeformDynamic","InspectorNodeApplyCommand","GridDeformDynamicCommand"},value);
}
namespace {
using namespace ext::nodes;
using Root=std::shared_ptr<ExDepthRigRoot>;
using Bone=std::shared_ptr<ExDepthBone>;
using NodeRef=std::shared_ptr<Node>;
using Id=commands::depth::bone::DepthBoneCommand;
using Setting=ExDepthBoneSourceSettings;
std::shared_ptr<commands::Context> context(NodeRef target,std::shared_ptr<Parameter> parameter={}) {
    auto result=std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
    result->nodes(commands::Context::Nodes{target});
    if(parameter) result->armedParameters(commands::Context::Parameters{parameter});
    return result;
}
Bone findBone(Root root,std::uint64_t uuid) {
    for(auto bone:compat::ngRequireObject(root)->depthBones())
        if(compat::ngRequireObject(bone)->getUUID()==uuid) return bone;
    return {};
}
compat::RetainedString retainFormatted(std::string value) {
    try {
        return compat::RetainedString(std::move(value));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::RetainedString missingBoneLabel(std::uint64_t uuid) {
    try {
        return compat::RetainedString(std::string("#")+std::to_string(uuid));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class... T> compat::RetainedString format(std::string_view pattern,const T&... values) {
    const std::array<core::translation::TypedFormatArgument,sizeof...(T)> args{
        core::translation::ngFormatArgument(values)...};
    return retainFormatted(core::translation::ngFormatTyped(pattern,args));
}
template<std::size_t N> compat::RetainedString translateText(const char (&literal)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(literal));
}
std::uint64_t mergeSession(Root root,NodeRef target,Bone bone,compat::RetainedString property,SessionMap& sessions) {
    if(!root||!target||!bone) return 0;
    const auto rootUuid=root->getUUID();
    const auto targetUuid=target->getUUID();
    const auto boneUuid=bone->getUUID();
    auto key=format("%s:%s:%s:%s",rootUuid,targetUuid,boneUuid,property);
    if(ImGui::IsItemActivated()) sessions[key]=commands::depth::bone::ngCreateDepthBoneSourceSettingsMergeSession();
    auto found=sessions.find(key); auto session=found==sessions.end()?0:found->second;
    if(ImGui::IsItemDeactivatedAfterEdit()) sessions.erase(key);
    return session;
}
void setSettings(Root root,NodeRef target,Bone bone,Setting setting,std::shared_ptr<Parameter> parameter) {
    const auto commandContext=context(target,parameter);
    auto text=format("{\"weight\":%s,\"depthOffset\":%s,\"depthScale\":%s,\"rotation\":%s}",
        setting.weight,setting.depthOffset,setting.depthScale,setting.rotation);
    ngRunGridDepthBoneCommand(Id::SetDepthBoneSourceSettings,commandContext,root,target,bone,text);
}
void setInteractive(Root root,NodeRef target,Bone bone,Setting setting,compat::RetainedString property,
    std::uint64_t session,std::shared_ptr<Parameter> parameter) {
    commands::depth::bone::ngBeginDepthBoneSourceSettingsMerge(session,property);
    compat::ngRunWithScopeExit([&] {setSettings(root,target,bone,setting,parameter);},
        [] {commands::depth::bone::ngEndDepthBoneSourceSettingsMerge();});
}
void reorder(Root root,NodeRef target,std::uint64_t from,std::uint64_t to) {
    if(!root||!target||from==to) return;
    auto index=root->findBindingIndex(target->getUUID()); if(index<0) return;
    auto old=root->bindings.duplicate(); auto& binding=root->bindings[static_cast<std::size_t>(index)];
    bool hasFrom=false,hasTo=false;
    for(auto id:binding.sourceBoneUuids) {if(id==from)hasFrom=true;if(id==to)hasTo=true;}
    if(!hasFrom||!hasTo) return;
    compat::Slice<std::uint64_t> reordered;
    for(auto id:binding.sourceBoneUuids) {if(id==from)continue;if(id==to)reordered.append(from);reordered.append(id);}
    binding.sourceBoneUuids=reordered;binding.normalizeSourceSettings();
    core::actionstack::incActionPush(std::allocate_shared<actions::depthbone::DepthBoneSourceListChangeAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::depthbone::DepthBoneSourceListChangeAction>{},
        compat::RetainedString::fromStaticLiteral("Reorder Depth Bone Source"),root,old,root->bindings,false,target));
}
}
void ngDrawGridDepthBoneSources(std::shared_ptr<Node> target,std::shared_ptr<Parameter> parameter,
    Cursor cursor,SessionMap& sessions,ext::nodes::ExDepthTargetKind kind) {
    (void)cursor; using namespace widgets; using core::translation::ngTranslateCString;
    auto root=ngDepthRigRootForTarget(project::incActivePuppet(),target);
    if(ngBeginCategory(ngTranslateCString("Depth Bone Sources").data())) {
        if(!root) ImGui::Text(ngTranslateCString("No DepthRigRoot.").data());
        else {
            if(ngButtonColored("\ue145"))
                ngRunGridDepthBoneCommand(Id::AddStandardDepthSkeleton,context(root),root,{}, {},{},1.f);
            ngTooltip(compat::RetainedStringSlice(translateText("Add Standard Skeleton")));
            const auto index=root->findBindingIndex(compat::ngRequireObject(target)->getUUID());
            ngText(translateText("Depth Bone Sources").view());
            if(ImGui::BeginListBox("###DepthBoneSources",{0,128})) {
                if(index<0||root->bindings[static_cast<std::size_t>(index)].sourceBoneUuids.empty())
                    ngText(translateText("(Drag a Depth Bone Here)").view());
                else {
                    std::uint64_t remove=0;Bone preview;
                    auto retainedBindings=root->bindings;
                    auto& binding=retainedBindings[static_cast<std::size_t>(index)];binding.normalizeSourceSettings();
                    auto uuids=binding.sourceBoneUuids;
                    for(std::size_t i=0;i<uuids.size();++i) {
                        const auto uuid=uuids[i];auto bone=findBone(root,uuid);auto setting=binding.sourceSetting(uuid);
                        ImGui::PushID(static_cast<int>(i));
                        const auto edit=[&](const char* label,const char* property,float& field,float speed,
                            float low,float high,const char* display,bool rotation=false) {
                            float value=rotation?node::detail::degrees(field):field;
                            const bool changed=ImGui::DragFloat(label,&value,speed,low,high,display,
                                rotation?ImGuiSliderFlags_NoRoundToFormat:0);
                            const auto propertyName=compat::RetainedString::fromStaticView(property);
                            const auto session=mergeSession(root,target,bone,propertyName,sessions);
                            if(changed) {
                                field=rotation?ngNormalizeDepthBoneSourceRotation(node::detail::radians(value)):value;
                                if(bone)setInteractive(root,target,bone,setting,propertyName,session,parameter);
                            }
                        };
                        if(ImGui::BeginPopup("###DepthBoneSourceSettings")) {
                            if(bone&&ImGui::MenuItem(ngTranslateCString("Focus").data())) {
                                project::incFocusCamera(bone);project::incSelectNode(bone);
                            }
                            if(bone&&ImGui::MenuItem(ngTranslateCString("Preview").data()))preview=bone;
                            ImGui::Separator();
                            edit(ngTranslateCString("Weight").data(),"weight",setting.weight,.01f,0,1,"%.3f");
                            edit(ngTranslateCString("Depth Offset").data(),"depthOffset",setting.depthOffset,.01f,-10,10,"%.3f");
                            edit(ngTranslateCString("Depth Scale").data(),"depthScale",setting.depthScale,.01f,.01f,10,"%.3f");
                            edit(ngTranslateCString("Rotation").data(),"rotation",setting.rotation,.01f,
                                -std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),"%.2f°",true);
                            ImGui::Separator();
                            if(ImGui::MenuItem(ngTranslateCString("Delete").data())) {
                                remove=uuid;ImGui::EndPopup();ImGui::PopID();break;
                            }
                            ImGui::EndPopup();
                        }
                        const compat::RetainedString label=bone?compat::RetainedString(bone->getName()):
                            missingBoneLabel(uuid);
                        const compat::SourceCString selectableLabel{compat::RetainedStringSlice(label)};
                        compat::detail::ngSourceCStringPostconditions(label.view(),selectableLabel.data());
                        ImGui::Selectable(selectableLabel.data(),false,ImGuiSelectableFlags_AllowOverlap,{0,17});
                        if(ImGui::BeginDragDropTarget()) {
                            if(auto* payload=ImGui::AcceptDragDropPayload("_DEPTHBONEITEM")) {
                                std::uint64_t draggedUuid;
                                std::memcpy(&draggedUuid,compat::ngRequireObject(payload->Data),sizeof(draggedUuid));
                                reorder(root,target,draggedUuid,uuid);
                            }
                            ImGui::EndDragDropTarget();
                        }
                        if(ImGui::IsItemClicked(ImGuiMouseButton_Right))ImGui::OpenPopup("###DepthBoneSourceSettings");
                        ImGui::SameLine(0,0);
                        if(ImGui::BeginChild("###DepthBoneSourceDepth",{0,17},0,
                            ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_AlwaysAutoResize)) {
                            ngDummy({-168,1});ImGui::SameLine(0,0);
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0,1});
                            ImGui::SetNextItemWidth(56);
                            edit("###offset","depthOffset",setting.depthOffset,.01f,-10,10,"o %.2f");
                            ImGui::SameLine(0,0);ImGui::SetNextItemWidth(56);
                            edit("###scale","depthScale",setting.depthScale,.01f,.01f,10,"s %.2f");
                            ImGui::SameLine(0,0);ImGui::SetNextItemWidth(56);
                            edit("###rotation","rotation",setting.rotation,.01f,-std::numeric_limits<float>::max(),
                                std::numeric_limits<float>::max(),"r %.1f°",true);
                            ImGui::PopStyleVar();
                        }
                        ImGui::EndChild();
                        if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                            auto payloadUuid=uuid;
                            ImGui::SetDragDropPayload("_DEPTHBONEITEM",&payloadUuid,sizeof(payloadUuid),ImGuiCond_Always);
                            const compat::RetainedString payloadLabel=bone?compat::RetainedString(bone->getName()):
                                missingBoneLabel(uuid);
                            ngText(payloadLabel.view());ImGui::EndDragDropSource();
                        }
                        ImGui::PopID();
                    }
                    if(preview)ngRunGridDepthBoneCommand(Id::PreviewDepthBoneInfluence,context(target),root,target,preview);
                    if(remove)if(auto bone=findBone(root,remove))
                        ngRunGridDepthBoneCommand(Id::RemoveDepthBoneSource,context(target),root,target,bone);
                }
                ImGui::EndListBox();
            }
            if(ImGui::BeginDragDropTarget()) {
                if(auto* payload=ImGui::AcceptDragDropPayload("_PUPPETNTREE")) {
                    Node* raw;
                    std::memcpy(&raw,compat::ngRequireObject(payload->Data),sizeof(raw));
                    if(auto bone=std::dynamic_pointer_cast<ExDepthBone>(raw?raw->shared_from_this():nullptr))
                        ngRunGridDepthBoneCommand(Id::AddDepthBoneSource,context(target),root,target,bone);
                }
                ImGui::EndDragDropTarget();
            }
            if(auto armed=project::incArmedParameter()) {
                if(ngButtonColored("\ue693"))ngRunGridDepthBoneCommand(Id::PreviewDepthBoneDeform,context(target,armed),root,target);
                ngTooltip(compat::RetainedStringSlice(translateText("Preview Depth Bone Deform")));ImGui::SameLine();
                if(ngButtonColored("\ue668"))ngRunGridDepthBoneCommand(Id::ApplyDepthBoneDeform,context(target,armed),root,target);
                ngTooltip(compat::RetainedStringSlice(translateText("Apply Depth Bone Deform")));
            }
            auto* binding=root->getOrCreateBinding(target,kind);
            int maximum=static_cast<int>(binding->influenceRule.maxInfluences);
            float radius=binding->influenceRule.radiusScale,minimum=binding->influenceRule.minimumRadius;
            bool changed=false;ImGui::Spacing();
            ImGui::TextColored(CategoryTextColor,ngTranslateCString("Influence Rule").data());ImGui::PushItemWidth(120);
            changed=ImGui::DragInt(ngTranslateCString("Max Influencing Bones").data(),&maximum,.1f,1,16)||changed;
            changed=ImGui::DragFloat(ngTranslateCString("Influence Radius Scale").data(),&radius,.01f,.01f,100,"%.3f")||changed;
            changed=ImGui::DragFloat(ngTranslateCString("Minimum Influence Radius").data(),&minimum,.1f,0,100000,"%.2f")||changed;
            ImGui::PopItemWidth();
            if(changed) {
                maximum=std::max(1,maximum);
                const auto ruleContext=context(target);
                ngRunGridDepthBoneCommand(Id::SetDepthBoneInfluenceRule,ruleContext,root,target,{},
                    format("{\"maxInfluences\":%s,\"radiusScale\":%s,\"minimumRadius\":%s}",maximum,radius,minimum));
            }
        }
    }
    ngEndCategory();
}
}
