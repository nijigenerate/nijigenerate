#include "parameters_internal.hpp"
#include <nijigenerate/windows/parameditor.hpp>
#include <nijigenerate/windows/paramsplit.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
namespace nijigenerate::panels::parameters {
void ngParameterViewEditButtons(bool armed,bool horizontal,std::size_t index,ParameterRef param,bool visible) {
    using namespace widgets;using core::translation::ngTranslateCString;
    namespace p=commands::parameter::paramedit;using Id=p::ParameditCommand;
    namespace a=commands::parameter::animedit;
    if(!visible&&!armed)return;
    auto ctx=detail::context(param);
    if(project::incEditMode()==project::EditMode::ModelEdit) {
        detail::setTransparency(1,1);
        if(ImGui::BeginPopup("###EditParam")) {
            if(ImGui::MenuItem(ngTranslateCString("Edit").data(),"",false,true))
                windows::base::incPushWindowList(windows::base::ngCreateWindow<windows::parameditor::ParamEditorWindow>(param));
            if(ImGui::MenuItem(ngTranslateCString("Split").data(),"",false,true))
                windows::base::incPushWindowList(windows::base::ngCreateWindow<windows::paramsplit::ParamSplitWindow>(index,param));
            if(!compat::ngRequireObject(param)->isVec2)detail::item("To 2D",p::commands,Id::ConvertTo2DParam,ctx);
            if(compat::ngRequireObject(param)->isVec2){detail::item("Flip X",p::commands,Id::FlipX,ctx);detail::item("Flip Y",p::commands,Id::FlipY,ctx);}
            else detail::item("Flip",p::commands,Id::Flip1D,ctx);
            if(ImGui::BeginMenu(ngTranslateCString("Mirror").data())) {
                detail::item("Horizontally",p::commands,Id::MirrorHorizontally,ctx);
                detail::item("Vertically",p::commands,Id::MirrorVertically,ctx);ImGui::EndMenu();
            }
            if(ImGui::BeginMenu(ngTranslateCString("Mirrored Autofill").data())) {
                detail::rawItem("\ue5c4",p::commands,Id::MirroredAutoFillDir1,ctx);
                detail::rawItem("\ue5c8",p::commands,Id::MirroredAutoFillDir2,ctx);
                if(compat::ngRequireObject(param)->isVec2){detail::rawItem("\ue5d8",p::commands,Id::MirroredAutoFillDir3,ctx);
                    detail::rawItem("\ue5db",p::commands,Id::MirroredAutoFillDir4,ctx);}
                ImGui::EndMenu();
            }
            ImGui::NewLine();ImGui::Separator();
            detail::item("Copy",p::commands,Id::CopyParameter,ctx);
            detail::item("Paste",p::commands,Id::PasteParameter,ctx);
            detail::item("Paste and Horizontal Flip",p::commands,Id::PasteParameterWithFlip,ctx);
            detail::item("Duplicate",p::commands,Id::DuplicateParameter,ctx);
            detail::item("Duplicate and Horizontal Flip",p::commands,Id::DuplicateParameterWithFlip,ctx);
            detail::item("Delete",p::commands,Id::DeleteParameter,ctx);
            ImGui::NewLine();ImGui::Separator();
            auto link=[&](ParameterRef target,int from,int to) {
                detail::queue([ctx,target,from,to] {
                    detail::run(p::commands,Id::LinkTo,ctx,[&](auto base) {
                        auto command=std::dynamic_pointer_cast<p::LinkToCommand>(base);
                        if(!command)throw compat::ThrowableException(
                            compat::RetainedString("LinkTo command type mismatch"));
                        command->toParam=target;command->fromAxis=from;command->toAxis=to;
                    });
                });
            };
            auto list=[&](int from) {
                auto parameters=compat::ngRequireObject(project::incActivePuppet())->parameters;
                for(auto target:parameters) {
                    if(nicxlive::core::ngResourceObjectEquals(param,target))continue;
                    if(compat::ngRequireObject(target)->isVec2) {
                        if(ImGui::BeginMenu(detail::parameterNameCString(target->getName()).data())) {
                            if(ImGui::MenuItem(ngTranslateCString("X").data()))link(target,from,0);
                            if(ImGui::MenuItem(ngTranslateCString("Y").data()))link(target,from,1);
                            ImGui::EndMenu();
                        }
                    }else if(ImGui::MenuItem(detail::parameterNameCString(target->getName()).data(),nullptr,false,true))
                        link(target,from,0);
                }
            };
            if(compat::ngRequireObject(param)->isVec2) {
                if(ImGui::BeginMenu(ngTranslateCString("Link X to...").data())){list(0);ImGui::EndMenu();}
                if(ImGui::BeginMenu(ngTranslateCString("Link Y to ..").data())){list(1);ImGui::EndMenu();}
            }else if(ImGui::BeginMenu(ngTranslateCString("Link parameter to...").data())){list(0);ImGui::EndMenu();}
            ImGui::NewLine();ImGui::Separator();detail::item("Set Starting Position",p::commands,Id::SetStartingKeyFrame,ctx);
            ImGui::EndPopup();
        }
        detail::resetTransparency();
        if(ngButtonColored("\ue8b8",{24,24}))ImGui::OpenPopup("###EditParam");
        if(horizontal)ImGui::SameLine();
        const bool isArmed=nicxlive::core::ngResourceObjectEquals(project::incArmedParameter(),param);
        if(ngButtonColored(isArmed?"\ue3fa":"\ue836",{24,24},isArmed?ImVec4(1,0,0,1):colorUndefined))
            detail::run(p::commands,Id::ToggleParameterArm,ctx);
        ngTooltip(core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Arm Parameter")));
    }
    if(project::incEditMode()==project::EditMode::AnimEdit) {
        if(horizontal)ImGui::SameLine();
        ImGui::BeginDisabled(!project::incAnimationGet());
        if(ngButtonColored("\ue990",{24,24}))detail::run(a::commands,a::AnimeditCommand::AddAnimationKeyFrame,ctx);
        ngTooltip(core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Add Keyframe")));
        if(ngButtonColored("\ue14d",{24,24}))detail::run(a::commands,a::AnimeditCommand::CopyAnimationKeyFrame,ctx);
        ngTooltip(core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Copy Keyframe")));
        if(ngButtonColored("\ue14f",{24,24}))detail::run(a::commands,a::AnimeditCommand::PasteAnimationKeyFrame,ctx);
        ngTooltip(core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Paste Keyframe")));ImGui::EndDisabled();
    }
}
}
