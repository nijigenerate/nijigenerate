#include "parameters_internal.hpp"
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/utils.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
namespace nijigenerate::panels::parameters {
bool incKeypointActions(ParameterRef param,BindingList source,BindingList target) {
    namespace b=commands::binding;using Id=b::BindingCommand;using core::translation::ngTranslateCString;
    auto ctx=std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
    if(!source.isNull())ctx->bindings(source);
    if(!target.isNull())ctx->activeBindings(target);
    detail::bindingContext(ctx,param);
    if(detail::item("Unset",b::commands,Id::UnsetKeyFrame,ctx))return true;
    if(detail::item("Set to current",b::commands,Id::SetKeyFrame,ctx))return true;
    if(detail::item("Reset",b::commands,Id::ResetKeyFrame,ctx))return true;
    if(detail::item("Invert",b::commands,Id::InvertKeyFrame,ctx))return true;
    if(ImGui::BeginMenu(ngTranslateCString("Mirror").data())) {
        if(detail::item("Horizontally",b::commands,Id::MirrorKeyFrameHorizontally,ctx)) {ImGui::EndMenu();return true;}
        if(detail::item("Vertically",b::commands,Id::MirrorKeyFrameVertically,ctx)) {ImGui::EndMenu();return true;}
        ImGui::EndMenu();
    }
    if(detail::item("Flip Deform",b::commands,Id::FlipDeform,ctx))return true;
    if(detail::item("Symmetrize Deform",b::commands,Id::SymmetrizeDeform,ctx))return true;
    if(compat::ngRequireObject(param)->isVec2) {
        if(ImGui::BeginMenu(ngTranslateCString("Set from mirror").data())) {
            if(detail::item("Horizontally",b::commands,Id::SetFromHorizontalMirror,ctx)){ImGui::EndMenu();return true;}
            if(detail::item("Vertically",b::commands,Id::SetFromVerticalMirror,ctx)){ImGui::EndMenu();return true;}
            if(detail::item("Diagonally",b::commands,Id::SetFromDiagonalMirror,ctx)){ImGui::EndMenu();return true;}
            ImGui::EndMenu();
        }
    } else if(detail::item("Set from mirror",b::commands,Id::SetFrom1DMirror,ctx))return true;
    if(detail::item("Copy",b::commands,Id::CopyBinding,ctx))return true;
    return detail::item("Paste",b::commands,Id::PasteBinding,ctx);
}
void incBindingMenuContents(ParameterRef param,commands::binding::SelectedBindings selected) {
    namespace b=commands::binding;using Id=b::BindingCommand;using core::translation::ngTranslateCString;
    auto ctx=std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
    ctx->activeBindings(detail::values(selected));
    detail::bindingContext(ctx,param);
    detail::item("Remove",b::commands,Id::RemoveBinding,ctx);
    if(incKeypointActions(param,{},detail::values(selected)))return;
    if(ImGui::BeginMenu(ngTranslateCString("Interpolation Mode").data())) {
        using Mode=nicxlive::core::common::InterpolateMode;
        const auto item=[&](const char* name,Mode mode) {
            if(ImGui::MenuItem(ngTranslateCString(name).data(),"",false,true))detail::queue([ctx,mode] {
                detail::run(b::commands,Id::SetInterpolation,ctx,[&](auto base) {
                    auto command=std::dynamic_pointer_cast<b::SetInterpolationCommand>(base);
                    if(!command)throw compat::ThrowableException(
                        compat::RetainedString("SetInterpolation command type mismatch"));
                    command->mode=mode;
                });
            });
        };
        item("Nearest",Mode::Nearest);item("Linear",Mode::Linear);item("Cubic",Mode::Cubic);ImGui::EndMenu();
    }
    const bool compatible=!b::cCompatibleNodes.empty();
    if(ImGui::BeginMenu(ngTranslateCString("Copy to").data(),compatible)) {
        auto nodes=b::cCompatibleNodes;
        for(auto resource:nodes)if(auto node=std::dynamic_pointer_cast<b::Node>(resource.getResource()))
            if(ImGui::MenuItem(detail::parameterNameCString(node->getName()).data(),"",false,true))
            detail::queue([param,node]{b::copySelectionToNode(param,node);});
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu(ngTranslateCString("Swap with").data(),compatible)) {
        auto nodes=b::cCompatibleNodes;
        for(auto resource:nodes)if(auto node=std::dynamic_pointer_cast<b::Node>(resource.getResource()))
            if(ImGui::MenuItem(detail::parameterNameCString(node->getName()).data(),"",false,true))
            detail::queue([param,node]{b::swapSelectionWithNode(param,node);});
        ImGui::EndMenu();
    }
}
void incBindingList(ParameterRef param) {
    namespace b=commands::binding;using namespace widgets;using core::translation::ngTranslateCString;
    if(ngBeginCategory(ngTranslateCString("Bindings").data(),IncCategoryFlags::None,[](float,float) {
        if(b::selectedOnly)ImGui::TextUnformatted("\ue152");else ImGui::TextDisabled("%s","\ue152");
        if(ImGui::IsItemClicked())b::selectedOnly=!b::selectedOnly;
        ngTooltip(core::translation::ngTranslateRetained(b::selectedOnly
            ? compat::RetainedString::fromStaticLiteral("Show all nodes")
            : compat::RetainedString::fromStaticLiteral("Show only selected nodes")));ImGui::SameLine();
    })) {
        b::refreshBindingList(param,b::selectedOnly);
        auto& io=ImGui::GetIO();const auto inactive=ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::BeginChild("BindingList",{0,256},false);
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,{4,1});ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing,14);
        auto nodes=commands::parameter::cAllBoundNodes;
        for(auto resource:nodes) {
            auto all=b::cParamBindingEntriesAll.at(resource);
            const auto present=b::cParamBindingEntries.find(resource)!=b::cParamBindingEntries.end();
            bool nodeSelected=true,someSelected=false;
            for(auto binding:all) {
                if(!b::cSelectedBindings.contains(b::BindingTarget(compat::ngRequireObject(binding)->getTarget())))nodeSelected=false;
                else someSelected=true;
            }
            auto flags=ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_OpenOnArrow;
            if(nodeSelected)flags|=ImGuiTreeNodeFlags_Selected;
            if(!present)ImGui::PushStyleColor(ImGuiCol_Text,inactive);
            const auto boundResource=resource.getResource();
            auto node=std::dynamic_pointer_cast<b::Node>(boundResource);
            nicxlive::core::common::NullableString name;
            if(node) {
                const auto icon=utils::incTypeIdToIcon(node->typeId());
                const auto nodeName=node->getName();
                name=detail::allocateParameterStorage([&] {
                    return nicxlive::core::common::NullableString(compat::RetainedString(
                        std::string(icon)+" "+std::string(nodeName.view())));
                });
            } else name=compat::ngRequireObject(boundResource)->getName();
            const auto uuid=compat::ngRequireObject(boundResource)->getUUID();
            if(ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::uintptr_t>(uuid)),flags,
                    detail::parameterNameCString(name).data())) {
                if(!present)ImGui::PopStyleColor();
                if(ImGui::BeginPopup("###BindingPopup")){incBindingMenuContents(param,b::cSelectedBindings);ImGui::EndPopup();}
                if(ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    if(!someSelected){b::cSelectedBindings.clear();for(auto binding:all)b::cSelectedBindings[b::BindingTarget(compat::ngRequireObject(binding)->getTarget())]=binding;}
                    b::cCompatibleNodes=b::getCompatibleNodes();ImGui::OpenPopup("###BindingPopup");
                }
                if(ImGui::IsItemClicked(ImGuiMouseButton_Left)&&!ImGui::IsItemToggledOpen()) {
                    if(project::incNodeInSelection(node))project::incFocusCamera(node);else project::incSelectNode(node);
                    if(!io.KeyCtrl){b::cSelectedBindings.clear();nodeSelected=false;}
                    for(auto binding:all){auto key=b::BindingTarget(compat::ngRequireObject(binding)->getTarget());if(nodeSelected)b::cSelectedBindings.erase(key);else b::cSelectedBindings[key]=binding;}
                }
                for(auto binding:all) {
                    bool selected=b::cSelectedBindings.contains(b::BindingTarget(compat::ngRequireObject(binding)->getTarget()));
                    auto leaf=ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if(selected)leaf|=ImGuiTreeNodeFlags_Selected;
                    const bool set=b::ngBindingIsSetAt(binding,b::cParamPoint);
                    if(!set)ImGui::PushStyleColor(ImGuiCol_Text,inactive);
                    auto label=compat::ngRequireObject(binding)->getNameDescriptor();
                    if(auto value=std::dynamic_pointer_cast<nicxlive::core::param::ValueParameterBinding>(binding);value&&set) {
                        const std::array<core::translation::TypedFormatArgument,2> args{
                            core::translation::ngFormatArgument(label),core::translation::ngFormatArgument(value->getValue({b::cParamPoint.x,b::cParamPoint.y}))};
                        auto formatted=core::translation::ngFormatTyped("%s (%.02f)",args);
                        label=detail::allocateParameterStorage([&] {
                            return nicxlive::core::common::NullableString(
                                compat::RetainedString(std::move(formatted)));
                        });
                    }
                    // Keep the separately evaluated D string descriptor alive through the UI call.
                    auto bindingId=compat::ngRequireObject(binding)->getNameDescriptor();
                    const auto bindingIdCString=detail::parameterNameCString(bindingId);
                    ImGui::TreeNodeEx(bindingIdCString.data(),leaf,detail::parameterNameCString(label).data());
                    if(!set)ImGui::PopStyleColor();
                    if(ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        if(!selected){b::cSelectedBindings.clear();b::cSelectedBindings[b::BindingTarget(compat::ngRequireObject(binding)->getTarget())]=binding;}
                        b::cCompatibleNodes=b::getCompatibleNodes();ImGui::OpenPopup("###BindingPopup");
                    }
                    if(ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        if(!io.KeyCtrl){b::cSelectedBindings.clear();selected=false;}
                        if(selected)b::cSelectedBindings.erase(b::BindingTarget(compat::ngRequireObject(binding)->getTarget()));
                        else b::cSelectedBindings[b::BindingTarget(compat::ngRequireObject(binding)->getTarget())]=binding;
                    }
                }
                ImGui::TreePop();
            } else if(!present)ImGui::PopStyleColor();
        }
        ImGui::PopStyleVar(2);ImGui::EndChild();
    }
    ngEndCategory();
}
}
