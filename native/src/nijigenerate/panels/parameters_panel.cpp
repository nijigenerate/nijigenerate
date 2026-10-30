// Independent parameters.d panel body; the registry is supplied by the application.
#include "parameters_panel.hpp"
#include "parameters_internal.hpp"
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/io/imageexport_case.hpp>
#include <bit>
namespace nijigenerate::panels::parameters {
ParametersPanel::ParametersPanel():Panel(compat::RetainedString::fromStaticLiteral("Parameters"),
    core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Parameters")),true) {}
void ParametersPanel::onUpdate() {
    const auto receiver=shared_from_this();
    using namespace widgets;using core::translation::ngTranslateRetained;
    namespace pb=commands::parameter::base;
    incRunPendingParameterUiCommand();
    if(project::incEditMode()==project::EditMode::VertexEdit) {
        ngLabelOver(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("In vertex edit mode...")),{0,0},true);return;
    }
    auto parameters=compat::ngRequireObject(project::incActivePuppet())->parameters;
    auto puppet=std::dynamic_pointer_cast<ext::ExPuppet>(project::incActivePuppet());
    auto groups=puppet?puppet->groups:compat::Slice<std::shared_ptr<ext::ExParameterGroup>>{};
    if(ImGui::BeginPopup("###AddParameter")){incParameterMenuContents(parameters);ImGui::EndPopup();}
    if(ImGui::BeginChild("###FILTER",{0,32})) {
        compat::RetainedStringSlice text(filter);
        const bool changed = ngInputText("Filter",text);
        filter = text.retainedString();
        if(changed)filter=io::imageexport::detail::lowerExtension(text);
        ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Filter, search for specific parameters")));
    }
    ImGui::EndChild();
    if(ImGui::BeginChild("ParametersList",{0,-36})) {
        auto matches=[&](const auto& p) {
            if(compat::ngRequireObject(p)->indexableName.find(filter.view())!=std::string::npos)return true;
            const auto armed=project::incArmedParameter();
            return nicxlive::core::ngResourceObjectEquals(armed,p);
        };
        auto display=[&](auto target,bool hideChildren) {
            for(std::size_t i=0;i<target.size();++i) {
                auto& param=target[i];auto ex=std::dynamic_pointer_cast<ext::ExParameter>(param);
                if(hideChildren&&ex&&ex->parent)continue;
                auto group=std::dynamic_pointer_cast<ext::ExParameterGroup>(param);
                bool found=filter.empty()||matches(param);
                if(group) {
                    const auto childRange=group->children;
                    for(std::size_t ix=0;ix<childRange.size();++ix) {
                        auto& child=childRange[ix];
                        if(matches(child))found=true;
                    }
                }
                if(!found)continue;
                if(!group){incParameterView(i,param,&grabParam,true,compat::ngRequireObject(project::incActivePuppet())->parameters);continue;}
                ImGui::PushID(std::bit_cast<std::int32_t>(group->getUUID()));
                auto c=group->color;bool colored=std::isfinite(c.x)&&std::isfinite(c.y)&&std::isfinite(c.z);
                bool open;
                if(colored) {
                    const auto label=detail::parameterNameCString(group->getName());
                    const auto currentColor=group->color;
                    open=ngBeginCategory(label.data(),{currentColor.x,currentColor.y,currentColor.z,1});
                } else open=ngBeginCategory(detail::parameterNameCString(group->getName()).data());
                if(ImGui::IsItemClicked(ImGuiMouseButton_Right))ImGui::OpenPopup("###CategorySettings");
                if(ImGui::BeginPopup("###CategorySettings")) {
                    bool deleted=incParameterGropuMenuContents(group);ImGui::EndPopup();
                    if(deleted){ngEndCategory();ImGui::PopID();continue;}
                }
                if(ImGui::BeginDragDropTarget()) {
                    if(auto payload=ImGui::AcceptDragDropPayload("_PARAMETER"))
                        pb::incMoveParameter(compat::ngRequireObject(*compat::ngRequireObject(
                            static_cast<ParamDragDropData**>(payload->Data)))->param,group);
                    ImGui::EndDragDropTarget();
                }
                if(open) {
                    auto children=group->children;
                    for(std::size_t ix=0;ix<children.size();++ix) {
                        auto& child=children[ix];
                        if(matches(child))incParameterView(ix,child,&grabParam,false,group->children,group->color);
                    }
                }
                ngEndCategory();ImGui::PopID();
            }
        };
        display(groups,false);display(parameters,true);
    }
    ImGui::EndChild();
    if(ImGui::BeginDragDropTarget()) {
        if(auto payload=ImGui::AcceptDragDropPayload("_PARAMETER"))
            pb::incMoveParameter(compat::ngRequireObject(*compat::ngRequireObject(
                static_cast<ParamDragDropData**>(payload->Data)))->param,{});
        ImGui::EndDragDropTarget();
    }
    auto available=ngAvailableSpace();ngDummy({available.x-32,32});ImGui::SameLine(0,0);
    if(ngButtonColored("\ue145",{32,32}))ImGui::OpenPopup("###AddParameter");
    ngTooltip(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Add Parameter")));
}
void ngInitializeParametersPanel() { panels::ngInitializePanelModule<ParametersPanel>(); }
}
