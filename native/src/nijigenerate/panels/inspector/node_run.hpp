#pragma once
#include "node.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/inspector/apply_node_commands.hpp>
#include <nijigenerate/commands/inspector/apply_node_table.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <nijigenerate/core/translation.hpp>
#include <resource.hpp>
#include <compat/source_allocator.hpp>
#include <limits>
#include <numbers>
namespace nijigenerate::panels::inspector::node {
namespace detail {
// Retain the selected Windows binary64 coefficients. Other supported targets
// use the source real coefficient/product before the final float narrowing.
#if defined(_WIN32)
using InspectorReal = double;
inline constexpr InspectorReal degreesFactor = 0x1.ca5dc1a63c1f8p+5;
inline constexpr InspectorReal radiansFactor = 0x1.1df46a2529d39p-6;
#else
using InspectorReal = long double;
inline constexpr InspectorReal degreesFactor = InspectorReal{180} / std::numbers::pi_v<InspectorReal>;
inline constexpr InspectorReal radiansFactor = std::numbers::pi_v<InspectorReal> / InspectorReal{180};
#endif
inline float degrees(float value) {return static_cast<float>(static_cast<InspectorReal>(value)*degreesFactor);}
inline float radians(float value) {return static_cast<float>(static_cast<InspectorReal>(value)*radiansFactor);}
}
template<class T> requires eligible<ModelEditSubMode::Layout,T>
void NodeInspector<ModelEditSubMode::Layout,T>::run() {
    using namespace widgets; using core::translation::ngTranslateCString;
    const auto sourceText=[]<std::size_t N>(const char (&text)[N]) {
        return compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(text));
    };
    using Id=commands::inspector::apply_node::InspectorNodeApplyCommand;
    auto self=std::static_pointer_cast<NodeInspector>(this->shared_from_this());
    if(this->targets.empty()) return;
    const auto node=this->targets[0];
    const auto root=compat::ngRequireObject(project::incActivePuppet())->root;
    if(nicxlive::core::ngResourceObjectEquals(node,root)) return;
    auto context=[&] {auto ctx=std::allocate_shared<commands::Context>(
        nicxlive::core::d_runtime::SourceAllocator<commands::Context>{});
        ctx->inspectors(commands::Context::Inspectors{self});
        ctx->nodes(static_cast<commands::Context::Nodes>(Selection<Node>(this->targets))); return ctx;};
    const auto dispatch=[]<PropertyId prop>(Id id,std::shared_ptr<commands::Context> context,auto& value) {
        namespace source=commands::inspector::apply_node;
        using Inspector=NodeInspector<ModelEditSubMode::Layout,Node>;
#define NG_DISPATCH_NODE_PROPERTY(Property,Name,Member,Type) \
        if constexpr(prop.view()==Property) { \
            using Access=source::InspectorAttributeAccess<Inspector,&Inspector::Member>; \
            using Actual=typename source::DefApply<#Name,Inspector,Access,"Node",Type>::Apply; \
            (void)commands::ngDispatchRegisteredCommand<Actual>(source::commands,id,context, \
                {"InspectorNodeApplyCommand." #Name,"InspectorNodeApplyCommand",#Name "Command"},value); \
        }
        NG_DISPATCH_NODE_PROPERTY("translationX",TranslationX,translationX,"float");
        NG_DISPATCH_NODE_PROPERTY("translationY",TranslationY,translationY,"float");
        NG_DISPATCH_NODE_PROPERTY("translationZ",TranslationZ,translationZ,"float");
        NG_DISPATCH_NODE_PROPERTY("rotationX",RotationX,rotationX,"float");
        NG_DISPATCH_NODE_PROPERTY("rotationY",RotationY,rotationY,"float");
        NG_DISPATCH_NODE_PROPERTY("rotationZ",RotationZ,rotationZ,"float");
        NG_DISPATCH_NODE_PROPERTY("scaleX",ScaleX,scaleX,"float");
        NG_DISPATCH_NODE_PROPERTY("scaleY",ScaleY,scaleY,"float");
        NG_DISPATCH_NODE_PROPERTY("lockToRoot",LockToRoot,lockToRoot,"bool");
        NG_DISPATCH_NODE_PROPERTY("pinToMesh",PinToMesh,pinToMesh,"bool");
        NG_DISPATCH_NODE_PROPERTY("pixelSnap",PixelSnap,pixelSnap,"bool");
        NG_DISPATCH_NODE_PROPERTY("zSort",ZSort,zSort,"float");
#undef NG_DISPATCH_NODE_PROPERTY
    };
    auto edit=[&]<PropertyId prop>(auto& property,Id id,auto draw) {
        auto state=[&] {return property.get(self);};
        if(this->template shared<Descriptor<T,prop>>(state,[&]{return draw(state);})) {
            const auto ctx=context();
            const auto current=state();
            dispatch.template operator()<prop>(id,ctx,compat::ngRequireObject(current)->value);
        }
    };
    if(ngBeginCategory(ngTranslateCString("Transform").data())) {
        auto avail=ImGui::GetContentRegionAvail();
        const float maximum=std::numeric_limits<float>::max();
        ImGui::TextColored(CategoryTextColor,ngTranslateCString("Translation").data());
        ImGui::PushItemWidth((avail.x-4.f)/3.f);
        ImGui::PushID(0);
        edit.template operator()<"translationX">(translationX,Id::TranslationX,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("translation_x"),&current->value,1.f,-maximum,maximum,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        }); ImGui::PopID();
        ImGui::SameLine(0,4);
        ImGui::PushID(1);
        edit.template operator()<"translationY">(translationY,Id::TranslationY,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("translation_y"),&current->value,1.f,-maximum,maximum,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        }); ImGui::PopID();
        ImGui::SameLine(0,4);
        ImGui::PushID(2);
        edit.template operator()<"translationZ">(translationZ,Id::TranslationZ,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("translation_z"),&current->value,1.f,-maximum,maximum,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        }); ImGui::PopID();
        ImGui::Spacing(); ImGui::Spacing(); ImGui::BeginGroup();
        edit.template operator()<"lockToRoot">(lockToRoot,Id::LockToRoot,[&](auto state) {
            const auto label=ngTranslateCString("Lock to Root Node");
            const auto current=ngInspectorWritableState(state());
            return ngCheckbox(label.data(),&current->value);});
        edit.template operator()<"pinToMesh">(pinToMesh,Id::PinToMesh,[&](auto state) {
            const auto label=ngTranslateCString("Pin origin to parent mesh.");
            const auto current=ngInspectorWritableState(state());
            return ngCheckbox(label.data(),&current->value);});
        ImGui::EndGroup();
        ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Makes so that the translation of this node is based off the root node, making it stay in place even if its parent moves."))));
        ImGui::Spacing(); ImGui::Spacing(); ImGui::PopItemWidth(); ImGui::Spacing();
        ImGui::TextColored(CategoryTextColor,ngTranslateCString("Rotation").data());
        ImGui::PushItemWidth((avail.x-4.f)/3.f);
        ImGui::PushID(3);
        {auto state=[&] {return rotationX.get(self);};
        float angle=detail::degrees(compat::ngRequireObject(state())->value);
        if(this->template shared<Descriptor<T,"rotationX">>(state,[&]{
            return ngDragFloat(sourceText("rotation_x"),&angle,1.f/100.f,-maximum,maximum,
                sourceText("%.2f°"),ImGuiSliderFlags_NoRoundToFormat);
        })) {
            ngInspectorWritableState(state())->value=detail::radians(angle);
            const auto ctx=context();
            const auto current=state();
            dispatch.template operator()<"rotationX">(Id::RotationX,ctx,compat::ngRequireObject(current)->value);
        }}
        ImGui::PopID();
        ImGui::SameLine(0,4);
        ImGui::PushID(4);
        {auto state=[&] {return rotationY.get(self);};
        float angle=detail::degrees(compat::ngRequireObject(state())->value);
        if(this->template shared<Descriptor<T,"rotationY">>(state,[&]{
            return ngDragFloat(sourceText("rotation_y"),&angle,1.f/100.f,-maximum,maximum,
                sourceText("%.2f°"),ImGuiSliderFlags_NoRoundToFormat);
        })) {
            ngInspectorWritableState(state())->value=detail::radians(angle);
            const auto ctx=context();
            const auto current=state();
            dispatch.template operator()<"rotationY">(Id::RotationY,ctx,compat::ngRequireObject(current)->value);
        }}
        ImGui::PopID();
        ImGui::SameLine(0,4);
        ImGui::PushID(5);
        {auto state=[&] {return rotationZ.get(self);};
        float angle=detail::degrees(compat::ngRequireObject(state())->value);
        if(this->template shared<Descriptor<T,"rotationZ">>(state,[&]{
            return ngDragFloat(sourceText("rotation_z"),&angle,1.f/100.f,-maximum,maximum,
                sourceText("%.2f°"),ImGuiSliderFlags_NoRoundToFormat);
        })) {
            ngInspectorWritableState(state())->value=detail::radians(angle);
            const auto ctx=context();
            const auto current=state();
            dispatch.template operator()<"rotationZ">(Id::RotationZ,ctx,compat::ngRequireObject(current)->value);
        }}
        ImGui::PopID();
        ImGui::PopItemWidth(); avail.x+=ImGui::GetFontSize(); ImGui::Spacing();
        ImGui::TextColored(CategoryTextColor,ngTranslateCString("Scale").data());
        ImGui::PushItemWidth((avail.x-14.f)/2.f);
        ImGui::PushID(6);
        edit.template operator()<"scaleX">(scaleX,Id::ScaleX,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("scale_x"),&current->value,1.f/100.f,-maximum,maximum,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        }); ImGui::PopID();
        ImGui::SameLine(0,4);
        ImGui::PushID(7);
        edit.template operator()<"scaleY">(scaleY,Id::ScaleY,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ngDragFloat(sourceText("scale_y"),&current->value,1.f/100.f,-maximum,maximum,
                sourceText("%.2f"),ImGuiSliderFlags_NoRoundToFormat);
        }); ImGui::PopID();
        ImGui::PopItemWidth(); ImGui::Spacing(); ImGui::Spacing();
        edit.template operator()<"pixelSnap">(pixelSnap,Id::PixelSnap,[&](auto state) {
            const auto label=ngTranslateCString("Snap to Pixel");
            const auto current=ngInspectorWritableState(state());
            return ngCheckbox(label.data(),&current->value);});
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::TextColored(CategoryTextColor,ngTranslateCString("Sorting").data());
        const auto zSortTargets=this->targets;
        compat::Slice<float> zSortValues;
        if(!zSortTargets.empty()) zSortValues.resize(zSortTargets.size());
        for(std::size_t index=0;index<zSortTargets.size();++index) {
            const auto target=zSortTargets[index];
            zSortValues[index]=compat::ngRequireObject(target)->relZSort();
        }
        edit.template operator()<"zSort">(zSort,Id::ZSort,[&](auto state) {
            const auto current=ngInspectorWritableState(state());
            return ImGui::InputFloat("###ZSort",&current->value,0.01f,0.05f,"%0.2f");});
    } ngEndCategory();
}
template<class T> requires eligible<ModelEditSubMode::Deform,T>
void NodeInspector<ModelEditSubMode::Deform,T>::run(std::shared_ptr<Parameter> parameter,Cursor cursor) {
    using namespace widgets; using core::translation::ngTranslateCString;
    auto self=std::static_pointer_cast<NodeInspector>(this->shared_from_this());
    if(this->targets.empty()) return;
    const auto node=this->targets[0];
    this->updateDeform(parameter,cursor);
    const auto root=compat::ngRequireObject(project::incActivePuppet())->root;
    if(nicxlive::core::ngResourceObjectEquals(node,root)) return;
    if(ngBeginCategory(ngTranslateCString("Transform").data())) {
        auto avail=ImGui::GetContentRegionAvail();
        ImGui::TextColored(CategoryTextColor,ngTranslateCString("Translation").data());
        ImGui::PushItemWidth((avail.x-4.f)/3.f);
        ImGui::PushID(0);
        this->template deform<DeformDescriptor<T,"translationX","transform.t.x">>(
            [&]{return translationX.get(self);},[&](auto name,float* value) {
            return ngInspectorDeformFloatDragVal(name,value,1.f);
        }); ImGui::PopID();
        ImGui::SameLine(0,4);
        ImGui::PushID(1);
        this->template deform<DeformDescriptor<T,"translationY","transform.t.y">>(
            [&]{return translationY.get(self);},[&](auto name,float* value) {
            return ngInspectorDeformFloatDragVal(name,value,1.f);
        }); ImGui::PopID();
        ImGui::SameLine(0,4);
        ImGui::PushID(2);
        this->template deform<DeformDescriptor<T,"translationZ","transform.t.z">>(
            [&]{return translationZ.get(self);},[&](auto name,float* value) {
            return ngInspectorDeformFloatDragVal(name,value,1.f);
        }); ImGui::PopID();
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PopItemWidth();
        ImGui::Spacing();
        ImGui::TextColored(CategoryTextColor,ngTranslateCString("Rotation").data());
        ImGui::PushItemWidth((avail.x-4.f)/3.f);
        ImGui::PushID(3);
        this->template deform<DeformDescriptor<T,"rotationX","transform.r.x">>(
            [&]{return rotationX.get(self);},[&](auto name,float* value) {
            return ngInspectorDeformFloatDragVal(name,value,0.05f,true);
        }); ImGui::PopID();
        ImGui::SameLine(0,4);
        ImGui::PushID(4);
        this->template deform<DeformDescriptor<T,"rotationY","transform.r.y">>(
            [&]{return rotationY.get(self);},[&](auto name,float* value) {
            return ngInspectorDeformFloatDragVal(name,value,0.05f,true);
        }); ImGui::PopID();
        ImGui::SameLine(0,4);
        ImGui::PushID(5);
        this->template deform<DeformDescriptor<T,"rotationZ","transform.r.z">>(
            [&]{return rotationZ.get(self);},[&](auto name,float* value) {
            return ngInspectorDeformFloatDragVal(name,value,0.05f,true);
        }); ImGui::PopID();
        ImGui::PopItemWidth();
        avail.x+=ImGui::GetFontSize(); ImGui::Spacing();
        ImGui::TextColored(CategoryTextColor,ngTranslateCString("Scale").data());
        ImGui::PushItemWidth((avail.x-14.f)/2.f);
        ImGui::PushID(6);
        this->template deform<DeformDescriptor<T,"scaleX","transform.s.x">>(
            [&]{return scaleX.get(self);},[&](auto name,float* value) {
            return ngInspectorDeformFloatDragVal(name,value,0.1f);
        }); ImGui::PopID();
        ImGui::SameLine(0,4);
        ImGui::PushID(7);
        this->template deform<DeformDescriptor<T,"scaleY","transform.s.y">>(
            [&]{return scaleY.get(self);},[&](auto name,float* value) {
            return ngInspectorDeformFloatDragVal(name,value,0.1f);
        }); ImGui::PopID();
        ImGui::PopItemWidth();
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::TextColored(CategoryTextColor,ngTranslateCString("Sorting").data());
        this->template deform<DeformDescriptor<T,"zSort","zSort">>(
            [&]{return zSort.get(self);},[&](auto name,float* value) {
            return ngInspectorDeformInputFloat(name,value,0.01f,0.05f);});
    } ngEndCategory();
}
}
