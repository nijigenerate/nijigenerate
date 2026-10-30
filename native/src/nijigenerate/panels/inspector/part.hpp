#pragma once
// Independent panels/inspector/part.d, BSD-2-Clause.
#include "common.hpp"
#include <nodes/part.hpp>
#include <nodes/composite.hpp>
#include <nodes/projectable.hpp>
#include <nijigenerate/core/typed_format_vec3.hpp>
#include <nijigenerate/core/typed_format_blend_mode.hpp>
#include <nijigenerate/commands/inspector/apply_node.hpp>
#include <functional>
#include <variant>
namespace nijigenerate::commands {class Context; class CommandResult;}
namespace nijigenerate::panels::inspector::part {
using Part=nicxlive::core::nodes::Part;
using Composite=nicxlive::core::nodes::Composite;
using Projectable=nicxlive::core::nodes::Projectable;
using Drawable=nicxlive::core::nodes::Drawable;
using Texture=nicxlive::core::Texture;
using BlendMode=nicxlive::core::nodes::BlendMode;
using Vec3=nicxlive::core::math::Vec3;
using TextureUsage=nicxlive::core::nodes::TextureUsage;
void incInspectorTextureSlot(std::shared_ptr<Part>,TextureUsage,compat::RetainedStringSlice,ImVec2);
nicxlive::core::common::SharedArray<std::ptrdiff_t> incRegisterWeldedPoints(std::shared_ptr<Drawable>,std::shared_ptr<Drawable>,float weight=0.5f);
using PropertyValue=std::variant<std::reference_wrapper<const Vec3>,
    std::reference_wrapper<const BlendMode>,std::reference_wrapper<const float>,
    std::reference_wrapper<const bool>>;
std::shared_ptr<commands::CommandResult> ngRunPartPropertyCommand(
    commands::inspector::apply_node::InspectorNodeApplyCommand,std::shared_ptr<commands::Context>,PropertyValue);
std::string ngPartBlendName(BlendMode);
template<ModelEditSubMode mode,class T>
requires(std::derived_from<T,Part> && !std::derived_from<T,Composite>) class NodeInspector;
}

namespace nicxlive::core::d_runtime {
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires ((Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout ||
        Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Deform) &&
        std::derived_from<T, ::nicxlive::core::nodes::Part> &&
        !std::derived_from<T, ::nicxlive::core::nodes::Composite>)
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::part::NodeInspector<Mode, T>> {
    static RetainedString className() { return declaration(true, true, true); }
    static RetainedString nestedQualifiedScope() { return declaration(true, true, false); }
    static RetainedString nestedUnqualifiedScope() { return declaration(false, true, false); }
    static RetainedString element(bool qualified) { return declaration(qualified, false, false); }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::panels::inspector::detail::inspectorClassArgument(element(true));
    }
private:
    static RetainedString declaration(bool qualified, bool classLeaf, bool classInfo) {
        const auto argument = ngSourceTypeSpelling<T>(classInfo ? SourceTypeSpelling::QualifiedElement :
            SourceTypeSpelling::UnqualifiedElement);
        return ::nijigenerate::panels::inspector::detail::inspectorNameBytes([&] {
            std::string name = qualified ? "nijigenerate.panels.inspector.part." : "";
            name += "NodeInspector!(ModelEditSubMode.";
            name += Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout ? "Layout, " : "Deform, ";
            name.append(argument.view());
            name += ')';
            if (classLeaf) name += ".NodeInspector";
            return name;
        });
    }
};

template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires ((Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout ||
        Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Deform) &&
        std::derived_from<T, ::nicxlive::core::nodes::Part> &&
        !std::derived_from<T, ::nicxlive::core::nodes::Composite>)
struct SourceTypeSpellings<::nijigenerate::panels::inspector::part::NodeInspector<Mode, T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::part::NodeInspector<Mode, T>>;
        const auto qualified = Names::element(true);
        const auto unqualified = Names::element(false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};
}

namespace nijigenerate::panels::inspector::part {
template<class T,PropertyId prop> struct Descriptor {
    using Target=T;
    using Value=std::conditional_t<prop.view()=="tint" || prop.view()=="screenTint",Vec3,
        std::conditional_t<prop.view()=="blendingMode",BlendMode,
        std::conditional_t<prop.view()=="autoResizedMesh",bool,float>>>;
    static constexpr auto id=prop;
    template<class Owner> static Value read(Owner&,const std::shared_ptr<T>& node) {
        if constexpr(prop.view()=="tint") return compat::ngRequireObject(node)->tint;
        else if constexpr(prop.view()=="screenTint") return compat::ngRequireObject(node)->screenTint;
        else if constexpr(prop.view()=="emissionStrength") return compat::ngRequireObject(node)->emissionStrength;
        else if constexpr(prop.view()=="blendingMode") return compat::ngRequireObject(node)->blendMode;
        else if constexpr(prop.view()=="opacity") return compat::ngRequireObject(node)->opacity;
        else if constexpr(prop.view()=="maskAlphaThreshold") return compat::ngRequireObject(node)->maskAlphaThreshold;
        else if constexpr(prop.view()=="autoResizedMesh") { auto projectable=std::dynamic_pointer_cast<Projectable>(node); return projectable ? projectable->autoResizedMesh : false; }
    }
    template<class Owner> static void write(Owner&,const std::shared_ptr<T>& node,Value value) {
        if constexpr(prop.view()=="tint") compat::ngRequireObject(node)->tint=value;
        else if constexpr(prop.view()=="screenTint") compat::ngRequireObject(node)->screenTint=value;
        else if constexpr(prop.view()=="emissionStrength") compat::ngRequireObject(node)->emissionStrength=value;
        else if constexpr(prop.view()=="blendingMode") compat::ngRequireObject(node)->blendMode=value;
        else if constexpr(prop.view()=="opacity") compat::ngRequireObject(node)->opacity=value;
        else if constexpr(prop.view()=="maskAlphaThreshold") compat::ngRequireObject(node)->maskAlphaThreshold=value;
        else if constexpr(prop.view()=="autoResizedMesh") { if(auto projectable=std::dynamic_pointer_cast<Projectable>(node)) projectable->autoResizedMesh=value; }
    }
    static std::string display(Value value) {
        if constexpr(std::same_as<Value,BlendMode>) return ngPartBlendName(value);
        else { const std::array<core::translation::TypedFormatArgument,1> args{core::translation::ngFormatArgument(value)};
            return core::translation::ngFormatTyped("%s",args); }
    }
};
template<class T> requires(std::derived_from<T,Part> && !std::derived_from<T,Composite>)
class NodeInspector<ModelEditSubMode::Layout,T> : public BaseInspector<ModelEditSubMode::Layout,T>,
    public MultiEdit<NodeInspector<ModelEditSubMode::Layout,T>> {
    using Self=NodeInspector; using Base=BaseInspector<ModelEditSubMode::Layout,T>;
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        ::nijigenerate::panels::inspector::detail::prepareInspectorClassRecord<Self>();
    }
public:
    NodeInspector() = default;
    NodeInspector(const NodeInspector&) = delete;
    NodeInspector& operator=(const NodeInspector&) = delete;
    NodeInspector(NodeInspector&&) = delete;
    NodeInspector& operator=(NodeInspector&&) = delete;
    InspectorProperty<Self,Descriptor<T,"tint">> tint;
    InspectorProperty<Self,Descriptor<T,"screenTint">> screenTint;
    InspectorProperty<Self,Descriptor<T,"emissionStrength">> emissionStrength;
    InspectorProperty<Self,Descriptor<T,"blendingMode">> blendingMode;
    InspectorProperty<Self,Descriptor<T,"opacity">> opacity;
    InspectorProperty<Self,Descriptor<T,"maskAlphaThreshold">> maskAlphaThreshold;
    InspectorProperty<Self,Descriptor<T,"autoResizedMesh">> autoResizedMesh;
    void capture(Selection<Node> nodes) override {
        auto self=std::static_pointer_cast<Self>(this->shared_from_this()); Base::capture(std::move(nodes));
        tint.get(self)->capture();
        screenTint.get(self)->capture();
        emissionStrength.get(self)->capture();
        blendingMode.get(self)->capture();
        opacity.get(self)->capture();
        maskAlphaThreshold.get(self)->capture();
        autoResizedMesh.get(self)->capture();
    }
    void run() override;
};
template<class T,PropertyId prop,PropertyId key> struct DeformDescriptor {
    using Target=T; using Value=float; static constexpr auto id=prop;
    template<class Owner> static float read(Owner& owner,const std::shared_ptr<T>& node) {
        return incInspectorDeformGetValue(node,owner.currParam,key.view(),owner.currCursor);
    }
    template<class Owner> static void writeBatch(Owner& owner,Selection<T> nodes,float value) {
        incInspectorDeformSetValue(Selection<Node>(nodes),owner.currParam,key.view(),owner.currCursor,value);
    }
    static std::string display(float value) {const std::array<core::translation::TypedFormatArgument,1> args{
        core::translation::ngFormatArgument(value)};return core::translation::ngFormatTyped("%s",args);}
};
template<class T> requires(std::derived_from<T,Part> && !std::derived_from<T,Composite>)
class NodeInspector<ModelEditSubMode::Deform,T> : public BaseInspector<ModelEditSubMode::Deform,T>,
    public MultiEdit<NodeInspector<ModelEditSubMode::Deform,T>> {
    using Self=NodeInspector; using Base=BaseInspector<ModelEditSubMode::Deform,T>;
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        ::nijigenerate::panels::inspector::detail::prepareInspectorClassRecord<Self>();
    }
public:
    NodeInspector() = default;
    NodeInspector(const NodeInspector&) = delete;
    NodeInspector& operator=(const NodeInspector&) = delete;
    NodeInspector(NodeInspector&&) = delete;
    NodeInspector& operator=(NodeInspector&&) = delete;
    InspectorProperty<Self,DeformDescriptor<T,"emissionStrength","emissionStrength">,true> emissionStrength;
    InspectorProperty<Self,DeformDescriptor<T,"opacity","opacity">,true> opacity;
    InspectorProperty<Self,DeformDescriptor<T,"alphaThreshold","alphaThreshold">,true> alphaThreshold;
    InspectorProperty<Self,DeformDescriptor<T,"tintR","tint.r">,true> tintR;
    InspectorProperty<Self,DeformDescriptor<T,"tintG","tint.g">,true> tintG;
    InspectorProperty<Self,DeformDescriptor<T,"tintB","tint.b">,true> tintB;
    InspectorProperty<Self,DeformDescriptor<T,"screenTintR","screenTint.r">,true> screenTintR;
    InspectorProperty<Self,DeformDescriptor<T,"screenTintG","screenTint.g">,true> screenTintG;
    InspectorProperty<Self,DeformDescriptor<T,"screenTintB","screenTint.b">,true> screenTintB;
    void capture(Selection<Node> nodes) override {
        auto self=std::static_pointer_cast<Self>(this->shared_from_this()); Base::capture(std::move(nodes));
        emissionStrength.get(self)->capture();
        opacity.get(self)->capture();
        alphaThreshold.get(self)->capture();
        tintR.get(self)->capture();
        tintG.get(self)->capture();
        tintB.get(self)->capture();
        screenTintR.get(self)->capture();
        screenTintG.get(self)->capture();
        screenTintB.get(self)->capture();
    }
    void run(std::shared_ptr<Parameter>,Cursor) override;
};
}
#include "part_run.hpp"
