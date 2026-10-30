#include "parameters_internal.hpp"
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/controller.hpp>
#include <nijigenerate/widgets/dragdrop.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/drag_float_fields.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/windows/rename.hpp>
namespace nijigenerate::panels::parameters {
namespace {
thread_local std::shared_ptr<ParamDragDropData> dragOwner;
bool finite(nicxlive::core::math::Vec3 v) {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
}
void updatePoint(ParameterRef p) {
    auto point = compat::ngRequireObject(p)->findClosestKeypoint();
    commands::binding::cParamPoint = {point.x, point.y};
}
}
compat::Vec2u incParamPoint() {
    if(auto p=project::incArmedParameter())updatePoint(p);
    return commands::binding::cParamPoint;
}
void ngParameterView(bool armed,bool showCategory,bool fixedWidth,std::size_t index,ParameterRef param,
    compat::RetainedString* grab,bool canGroup,nicxlive::core::math::Vec3 color) {
    using namespace widgets;namespace pb=commands::parameter::base;
    // This first source test uses `is`; later armed tests use Object equality.
    if(project::incArmedParameter()==param)updatePoint(param);
    ImGui::PushID(param.get());
    struct Pop {~Pop(){ImGui::PopID();}} pop;
    bool open=true;
    if(showCategory)open=finite(color)?ngBeginCategory(
        detail::parameterNameCString(compat::ngRequireObject(param)->getName()).data(),{color.x,color.y,color.z,1}):
        ngBeginCategory(detail::parameterNameCString(compat::ngRequireObject(param)->getName()).data());
    if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        if(!detail::dragDropData){
            dragOwner=std::allocate_shared<ParamDragDropData>(
                nicxlive::core::d_runtime::SourceAllocator<ParamDragDropData>{});
            detail::dragDropData=dragOwner.get();
        }
        detail::dragDropData->param=param;
        ImGui::SetDragDropPayload("_PARAMETER",&detail::dragDropData,sizeof(detail::dragDropData),ImGuiCond_Always);
        ngText(compat::ngRequireObject(detail::dragDropData->param)->getName());ImGui::EndDragDropSource();
    }
    if(canGroup) {
        ngBeginDragDropFake();
        auto peek=ImGui::AcceptDragDropPayload("_PARAMETER",ImGuiDragDropFlags_AcceptPeekOnly|ImGuiDragDropFlags_SourceAllowNullID);
        if(peek&&peek->Data&&!nicxlive::core::ngResourceObjectEquals(
                compat::ngRequireObject(*static_cast<ParamDragDropData**>(peek->Data))->param,param)) {
            if(ImGui::BeginDragDropTarget()) {
                if(auto payload=ImGui::AcceptDragDropPayload("_PARAMETER")) {
                    auto other=compat::ngRequireObject(*compat::ngRequireObject(
                        static_cast<ParamDragDropData**>(payload->Data)))->param;
                    auto group=pb::incCreateParamGroup(static_cast<std::int32_t>(index));
                    pb::incMoveParameter(param,group);pb::incMoveParameter(other,group);
                }
                ImGui::EndDragDropTarget();
            }
        }
        ngEndDragDropFake();
    }
    if(open) {
        if(finite(color))detail::pushColorScheme(color);
        float required=compat::ngRequireObject(param)->isVec2?144.f:52.f;auto available=ngAvailableSpace();
        float width=fixedWidth?156.f:available.x-24;
        float height=fixedWidth?(compat::ngRequireObject(param)->isVec2?132.f:52.f):required-24;
        bool visible=true;
        if(showCategory)visible=ImGui::BeginChild("###PARAM",{width,required});
        if(visible||armed) {
            if(ImGui::BeginPopup("###ControlPopup")) {
                if(nicxlive::core::ngResourceObjectEquals(project::incArmedParameter(),param)) {
                    incKeypointActions(param,detail::retainedSlice(compat::ngRequireObject(param)->bindings),{});
                }
                ImGui::EndPopup();
            }
            using namespace core::translation;
            std::string formatted;
            if(compat::ngRequireObject(param)->isVec2) {
                const std::array<TypedFormatArgument,2> args{
                    ngFormatArgument(compat::ngRequireObject(param)->value.x),
                    ngFormatArgument(compat::ngRequireObject(param)->value.y)};
                formatted=ngFormatTyped("%.2f %.2f",args);
            } else {
                const std::array<TypedFormatArgument,1> args{
                    ngFormatArgument(compat::ngRequireObject(param)->value.x)};
                formatted=ngFormatTyped("%.2f",args);
            }
            const auto text=detail::allocateParameterStorage([&] {
                return compat::RetainedString(std::move(formatted));
            });
            ngText(compat::RetainedStringSlice(text));
            const bool forceSnap=nicxlive::core::ngResourceObjectEquals(project::incArmedParameter(),param);
            const compat::RetainedStringSlice grabName(*compat::ngRequireObject(grab));
            if(ngController(compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("###CONTROLLER")),
                param,{width,height},forceSnap,grabName)) {
                if(nicxlive::core::ngResourceObjectEquals(project::incArmedParameter(),param)) {
                    viewport::model::deform::ngViewportNodeDeformNotifyParamValueChanged();
                    commands::binding::paramPointChanged(param);
                }
                if(ImGui::IsMouseDown(ImGuiMouseButton_Left)){if(grab->empty())*grab=compat::RetainedString(compat::ngRequireObject(param)->getName());}
                else *grab=compat::RetainedString::emptyLiteral();
            }
            if(ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                if(nicxlive::core::ngResourceObjectEquals(project::incArmedParameter(),param))
                    viewport::model::deform::ngViewportNodeDeformNotifyParamValueChanged();
                commands::binding::refreshBindingList(param);ImGui::OpenPopup("###ControlPopup");
            }
        }
        if(showCategory)ImGui::EndChild();
        ImGui::SameLine(0,0);
        if(showCategory) {
            visible=ImGui::BeginChild("###SETTING",{24,required},false);
            ngParameterViewEditButtons(armed,false,index,param,visible);ImGui::EndChild();
            if(nicxlive::core::ngResourceObjectEquals(project::incArmedParameter(),param))incBindingList(param);
        }
        if(finite(color))detail::popColorScheme();
    }
    if(showCategory)ngEndCategory();
}
bool incParameterGropuMenuContents(std::shared_ptr<ext::ExParameterGroup> group) {
    using core::translation::ngTranslateCString;auto ctx=detail::context(group);bool result=false;
    if(ImGui::MenuItem(ngTranslateCString("Rename").data()))
        windows::base::incPushWindow(windows::base::ngCreateWindowDeferred<windows::rename::RenameWindow>([&] {
            return std::shared_ptr<nicxlive::core::common::NullableString>(
                group, &compat::ngRequireObject(group)->name);
        }));
    if(ImGui::BeginMenu(ngTranslateCString("Colors").data())) {
        auto flags=ImGuiColorEditFlags_NoLabel|ImGuiColorEditFlags_NoTooltip;
        if(ImGui::ColorButton("NONE",{0,0,0,0},flags|ImGuiColorEditFlags_AlphaPreview,{24,24})) {
            if(!group)compat::ngFailNullAccess(true);
            group->color={NAN,NAN,NAN};
        }
        const char* names[]{"RED","GREEN","BLUE","PURPLE","CYAN","YELLOW","WHITE"};
        const ImVec4 swatches[]{{1,0,0,1},{0,1,0,1},{0,0,1,1},{1,0,1,1},{0,1,1,1},{1,1,0,1},{1,1,1,1}};
        for(std::size_t i=0;i<7;++i) {
            ImGui::SameLine(0,4);auto c=swatches[i];
            if(ImGui::ColorButton(names[i],c,flags,{24,24})) {
                if(!group)compat::ngFailNullAccess(true);
                group->color={c.x==1?.25f:.15f,c.y==1?.25f:.15f,c.z==1?.25f:.15f};
            }
        }
        ImGui::Spacing();
        const auto customColorLabel=ngTranslateCString("Custom Color");
        auto& groupColor=compat::ngRequireObject(group)->color;
        widgets::ngColorPicker3Fields(customColorLabel.data(),groupColor.x,groupColor.y,groupColor.z,
            ImGuiColorEditFlags_InputRGB|ImGuiColorEditFlags_DisplayHSV);ImGui::EndMenu();
    }
    if(ImGui::MenuItem(ngTranslateCString("Delete").data())) {
        detail::queue([ctx]{detail::run(commands::parameter::group::commands,
            commands::parameter::group::GroupCommand::DeleteParamGroup,ctx);});result=true;
    }
    return result;
}
void incParameterMenuContents(nicxlive::core::common::SharedArray<ParameterRef> parameters) {
    incParameterMenuContents(detail::retainedSlice(parameters));
}
void incParameterMenuContents(compat::Slice<ParameterRef> parameters) {
    namespace p=commands::parameter::param;using Id=p::ParamCommand;
    auto ctx=std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
    ctx->puppet(project::incActivePuppet());
    ctx->parameters(parameters);
    auto add=[&](const char* label,bool two,int minimum) {
        if(!ImGui::MenuItem(core::translation::ngTranslateCString(label).data(),"",false,true))return;
        detail::queue([ctx,two,minimum] {
            detail::run(p::commands,two?Id::Add2DParameter:Id::Add1DParameter,ctx,[&](auto base) {
                if(two){auto command=std::dynamic_pointer_cast<p::Add2DParameterCommand>(base);command->min=minimum;command->max=1;}
                else {auto command=std::dynamic_pointer_cast<p::Add1DParameterCommand>(base);command->min=minimum;command->max=1;}
            });
        });
    };
    add("Add 1D Parameter (0..1)",false,0);add("Add 1D Parameter (-1..1)",false,-1);
    add("Add 2D Parameter (0..1)",true,0);add("Add 2D Parameter (-1..+1)",true,-1);
    detail::item("Add Mouth Shape",p::commands,Id::AddMouthParameter,ctx);
}
}
