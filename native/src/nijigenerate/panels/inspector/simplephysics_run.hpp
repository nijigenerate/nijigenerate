#pragma once
#include "simplephysics.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/panels/parameters.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <compat/source_allocator.hpp>
namespace nijigenerate::panels::inspector::simplephysics {
template<class T> requires std::derived_from<T,SimplePhysics>
void NodeInspector<ModelEditSubMode::Layout,T>::run() {
    using namespace widgets; using core::translation::ngTranslateCString;
    const auto sourceText=[]<std::size_t N>(const char (&text)[N]) {
        return compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(text));
    };
    const auto translated=[&]<std::size_t N>(const char (&text)[N]) {
        return compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral(text)));
    };
    using Id=commands::inspector::apply_node::InspectorNodeApplyCommand;
    if(this->targets.empty()) return;
    auto self=std::static_pointer_cast<NodeInspector>(this->shared_from_this());
    auto node=this->targets[0];
    auto context=[&] {auto ctx=std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
        ctx->inspectors(commands::Context::Inspectors{self});
        ctx->nodes(static_cast<commands::Context::Nodes>(Selection<Node>(this->targets))); return ctx;};
    auto parameterContext=[&] {auto ctx=std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{}); ctx->nodes({node}); return ctx;};
    if(ngBeginCategory(ngTranslateCString("SimplePhysics").data())) {
        ImGui::Spacing();
        if(this->targets.size()==1) {
            ImGui::PushID("TargetParam");
            if(ImGui::BeginPopup("TPARAM")) {
                if(compat::ngRequireObject(node)->getParam()) {
                    if(ImGui::MenuItem(ngTranslateCString("Unmap").data()))
                        ngRunSimplePhysicsParameterCommand(parameterContext(),{},true);
                } else ngDummyLabel(translated("Unassigned"),ImVec2(128,16));
                ImGui::EndPopup();
            }
            ngText(translated("Parameter"));
            auto paramName=translated("(unassigned)").retainedString();
            if(compat::ngRequireObject(node)->getParam())
                paramName=compat::RetainedString(compat::ngRequireObject(
                    compat::ngRequireObject(node)->getParam())->getName());
            const compat::SourceCString nameBuffer{compat::RetainedStringSlice(paramName)};
            ImGui::InputText("###TARGET_PARAM",const_cast<char*>(nameBuffer.data()),paramName.size(),ImGuiInputTextFlags_ReadOnly);
            ImGui::OpenPopupOnItemClick("TPARAM",ImGuiPopupFlags_MouseButtonRight);
            if(ImGui::BeginDragDropTarget()) {
                if(const auto* payload=ImGui::AcceptDragDropPayload("_PARAMETER")) {
                    auto* data=*compat::ngRequireObject(static_cast<panels::parameters::ParamDragDropData* const*>(payload->Data));
                    auto ctx=parameterContext();
                    const auto& selected=compat::ngRequireObject(data)->param;
                    ngRunSimplePhysicsParameterCommand(ctx,selected,false);
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::PopID();
        }
        const auto edit=[&]<PropertyId prop>(auto& property,Id id,auto fn) {
            const auto state=[&] { return property.get(self); };
            if(this->template shared<Descriptor<T,prop>>(state,[&]{return fn(state);})) {
                auto ctx=context();
                const auto current=state();
                auto value=std::cref(compat::ngRequireObject(current)->value);
                ngRunSimplePhysicsPropertyCommand(id,ctx,value);
            }
        };
        ngText(translated("Type"));
        edit.template operator()<"modelType">(modelType,Id::SimplePhysicsModelType,[&](auto state) {
            bool changed=false;
            const auto preview=ngTranslateCString(ngPhysicsModelName(compat::ngRequireObject(state())->value));
            if(ImGui::BeginCombo("###PhysType",preview.data())) {
                for(auto item:{PhysicsModel::Pendulum,PhysicsModel::SpringPendulum}) {
                    const auto label=ngTranslateCString(ngPhysicsModelName(item));
                    const bool selected=compat::ngRequireObject(state())->value==item;
                    if(ImGui::Selectable(label.data(),selected)) {
                        ngInspectorWritableState(state())->value=item; changed=true;
                    }
                }
                ImGui::EndCombo();
            } return changed;
        });
        ImGui::Spacing(); ngText(translated("Mapping mode"));
        edit.template operator()<"mapMode">(mapMode,Id::SimplePhysicsMapMode,[&](auto state) {
            bool changed=false;
            const auto preview=ngTranslateCString(ngPhysicsMapModeName(compat::ngRequireObject(state())->value));
            if(ImGui::BeginCombo("###PhysMapMode",preview.data())) {
                for(auto item:{ParamMapMode::AngleLength,ParamMapMode::XY,ParamMapMode::LengthAngle,ParamMapMode::YX}) {
                    const auto label=ngTranslateCString(ngPhysicsMapModeName(item));
                    const bool selected=compat::ngRequireObject(state())->value==item;
                    if(ImGui::Selectable(label.data(),selected)) {
                        ngInspectorWritableState(state())->value=item; changed=true;
                    }
                }
                ImGui::EndCombo();
            } return changed;
        });
        ImGui::Spacing(); ImGui::PushID("SimplePhysics"); ImGui::PushID(-1);
        edit.template operator()<"localOnly">(localOnly,Id::SimplePhysicsLocalOnly,[&](auto state) {
            const auto label=ngTranslateCString("Local Transform Lock");
            const auto current=ngInspectorWritableState(state());
            return ngCheckbox(label.data(),&current->value);
        });
        ngTooltip(translated("Whether the physics system only listens to the movement of the physics node itself"));
        ImGui::Spacing(); ImGui::Spacing(); ImGui::PopID();
        const float maximum=std::numeric_limits<float>::max();
        ImGui::PushID(0);
        ngText(translated("Gravity scale"));
        edit.template operator()<"gravity">(gravity,Id::SimplePhysicsGravity,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("gravity"),&current->value,0.01f,-maximum,maximum,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PopID();
        ImGui::PushID(1);
        ngText(translated("Length"));
        edit.template operator()<"length">(length,Id::SimplePhysicsLength,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("length"),&current->value,0.01f,0,maximum,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PopID();
        ImGui::PushID(2);
        ngText(translated("Resonant frequency"));
        edit.template operator()<"frequency">(frequency,Id::SimplePhysicsFrequency,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("frequency"),&current->value,0.01f,0.01f,30,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PopID();
        ImGui::PushID(3);
        ngText(translated("Damping"));
        edit.template operator()<"angleDamping">(angleDamping,Id::SimplePhysicsAngleDamping,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("damping_angle"),&current->value,0.01f,0,5,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        });
        ImGui::PopID();
        ImGui::PushID(4);
        edit.template operator()<"lengthDamping">(lengthDamping,Id::SimplePhysicsLengthDamping,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("damping_length"),&current->value,0.01f,0,5,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PopID();
        ImGui::PushID(5);
        ngText(translated("Output scale"));
        edit.template operator()<"outputScaleX">(outputScaleX,Id::SimplePhysicsOutputScaleX,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("output_scale.x"),&current->value,0.01f,0,maximum,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        });
        ImGui::PopID();
        ImGui::PushID(6);
        edit.template operator()<"outputScaleY">(outputScaleY,Id::SimplePhysicsOutputScaleY,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("output_scale.y"),&current->value,0.01f,0,maximum,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PopID();
        ImGui::Spacing(); ImGui::Spacing(); ImGui::PopID();
    }
    ngEndCategory();
}
template<class T> requires std::derived_from<T,SimplePhysics>
void NodeInspector<ModelEditSubMode::Deform,T>::run(std::shared_ptr<Parameter> parameter,Cursor cursor) {
    using namespace widgets; using core::translation::ngTranslateCString;
    const auto sourceText=[]<std::size_t N>(const char (&text)[N]) {
        return compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(text));
    };
    const auto translated=[&]<std::size_t N>(const char (&text)[N]) {
        return compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral(text)));
    };
    if(this->targets.empty()) return;
    [[maybe_unused]] auto node=this->targets[0];
    auto self=std::static_pointer_cast<NodeInspector>(this->shared_from_this());
    this->updateDeform(parameter,cursor);
    if(ngBeginCategory(ngTranslateCString("Simple Physics").data())) {
        ImGui::PushID("SimplePhysics");
        const float maximum=std::numeric_limits<float>::max();
        ImGui::PushID(0);
        ngText(translated("Gravity scale"));
        this->template deform<DeformDescriptor<T,"gravity","gravity">>([&] { return gravity.get(self); },[&](auto name,float* value) {
            return ngInspectorDeformDragFloat(name,value,0.01f,-maximum,maximum,sourceText("%.2f").data());
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PopID();
        ImGui::PushID(1);
        ngText(translated("Length"));
        this->template deform<DeformDescriptor<T,"length","length">>([&] { return length.get(self); },[&](auto name,float* value) {
            return ngInspectorDeformDragFloat(name,value,0.01f,-maximum,maximum,sourceText("%.2f").data());
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PopID();
        ImGui::PushID(2);
        ngText(translated("Resonant frequency"));
        this->template deform<DeformDescriptor<T,"frequency","frequency">>([&] { return frequency.get(self); },[&](auto name,float* value) {
            return ngInspectorDeformDragFloat(name,value,0.01f,-maximum,maximum,sourceText("%.2f").data());
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PopID();
        ImGui::PushID(3);
        ngText(translated("Damping"));
        this->template deform<DeformDescriptor<T,"angleDamping","angleDamping">>([&] { return angleDamping.get(self); },[&](auto name,float* value) {
            return ngInspectorDeformDragFloat(name,value,0.01f,-maximum,maximum,sourceText("%.2f").data());
        });
        ImGui::PopID();
        ImGui::PushID(4);
        this->template deform<DeformDescriptor<T,"lengthDamping","lengthDamping">>([&] { return lengthDamping.get(self); },[&](auto name,float* value) {
            return ngInspectorDeformDragFloat(name,value,0.01f,-maximum,maximum,sourceText("%.2f").data());
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PopID();
        ImGui::PushID(5);
        ngText(translated("Output scale"));
        this->template deform<DeformDescriptor<T,"outputScaleX","outputScale.x">>([&] { return outputScaleX.get(self); },[&](auto name,float* value) {
            return ngInspectorDeformDragFloat(name,value,0.01f,-maximum,maximum,sourceText("%.2f").data());
        });
        ImGui::PopID();
        ImGui::PushID(6);
        this->template deform<DeformDescriptor<T,"outputScaleY","outputScale.y">>([&] { return outputScaleY.get(self); },[&](auto name,float* value) {
            return ngInspectorDeformDragFloat(name,value,0.01f,-maximum,maximum,sourceText("%.2f").data());
        });
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PopID();
        ImGui::Spacing(); ImGui::Spacing(); ImGui::PopID();
    }
    ngEndCategory();
}
}
