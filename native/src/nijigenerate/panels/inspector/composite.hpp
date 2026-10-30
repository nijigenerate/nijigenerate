#pragma once
#include "common.hpp"
#include <nodes/composite.hpp>
#include <nijigenerate/core/typed_format_vec3.hpp>
#include <nijigenerate/core/typed_format_blend_mode.hpp>
#include <nijigenerate/commands/inspector/apply_node.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <functional>
#include <variant>
namespace nijigenerate::commands { class Context; class CommandResult; }
namespace nijigenerate::panels::inspector::composite {
using Composite=nicxlive::core::nodes::Composite;
using BlendMode=nicxlive::core::nodes::BlendMode;
std::string ngCompositeBlendName(BlendMode);
using CompositePropertyValue=std::variant<std::reference_wrapper<const nicxlive::core::math::Vec3>,
    std::reference_wrapper<const BlendMode>,std::reference_wrapper<const float>>;
std::shared_ptr<commands::CommandResult> ngRunCompositePropertyCommand(
    commands::inspector::apply_node::InspectorNodeApplyCommand, std::shared_ptr<commands::Context>, CompositePropertyValue);
template<ModelEditSubMode mode,class T>
    requires ((mode==ModelEditSubMode::Layout || mode==ModelEditSubMode::Deform) && std::derived_from<T,Composite>)
class NodeInspector;
}

namespace nicxlive::core::d_runtime {
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires ((Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout ||
        Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Deform) &&
        std::derived_from<T, ::nicxlive::core::nodes::Composite>)
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::composite::NodeInspector<Mode, T>> {
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
            std::string name = qualified ? "nijigenerate.panels.inspector.composite." : "";
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
        std::derived_from<T, ::nicxlive::core::nodes::Composite>)
struct SourceTypeSpellings<::nijigenerate::panels::inspector::composite::NodeInspector<Mode, T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::composite::NodeInspector<Mode, T>>;
        const auto qualified = Names::element(true);
        const auto unqualified = Names::element(false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};
}

namespace nijigenerate::panels::inspector::composite {
enum class Attribute {Tint,ScreenTint,BlendingMode,Opacity,Threshold};
template<class T,Attribute attribute> struct Descriptor {
    using Target=T;
    using Value=std::conditional_t<attribute==Attribute::Tint || attribute==Attribute::ScreenTint,
        nicxlive::core::math::Vec3,std::conditional_t<attribute==Attribute::BlendingMode,BlendMode,float>>;
    static constexpr auto id=[] {
        if constexpr(attribute==Attribute::Tint) return PropertyId("tint");
        else if constexpr(attribute==Attribute::ScreenTint) return PropertyId("screenTint");
        else if constexpr(attribute==Attribute::BlendingMode) return PropertyId("blendingMode");
        else if constexpr(attribute==Attribute::Opacity) return PropertyId("opacity");
        else return PropertyId("threshold");
    }();
    template<class Owner> static Value read(Owner&,const std::shared_ptr<T>& node) {
        if constexpr(attribute==Attribute::Tint) return compat::ngRequireObject(node)->tint;
        else if constexpr(attribute==Attribute::ScreenTint) return compat::ngRequireObject(node)->screenTint;
        else if constexpr(attribute==Attribute::BlendingMode) return compat::ngRequireObject(node)->blendMode;
        else if constexpr(attribute==Attribute::Opacity) return compat::ngRequireObject(node)->opacity;
        else return compat::ngRequireObject(node)->threshold();
    }
    template<class Owner> static void write(Owner&,const std::shared_ptr<T>& node,Value value) {
        if constexpr(attribute==Attribute::Tint) compat::ngRequireObject(node)->tint=value;
        else if constexpr(attribute==Attribute::ScreenTint) compat::ngRequireObject(node)->screenTint=value;
        else if constexpr(attribute==Attribute::BlendingMode) compat::ngRequireObject(node)->blendMode=value;
        else if constexpr(attribute==Attribute::Opacity) compat::ngRequireObject(node)->opacity=value;
        else compat::ngRequireObject(node)->setThreshold(value);
    }
    static std::string display(Value value) {
        if constexpr(attribute==Attribute::BlendingMode) return ngCompositeBlendName(value);
        else {
            const std::array<core::translation::TypedFormatArgument,1> args{core::translation::ngFormatArgument(value)};
            return core::translation::ngFormatTyped("%s",args);
        }
    }
};
template<class T> requires std::derived_from<T,Composite>
class NodeInspector<ModelEditSubMode::Layout,T> : public BaseInspector<ModelEditSubMode::Layout,T>,
    public MultiEdit<NodeInspector<ModelEditSubMode::Layout,T>> {
    using Self=NodeInspector;
    using Base=BaseInspector<ModelEditSubMode::Layout,T>;
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
    InspectorProperty<Self,Descriptor<T,Attribute::Tint>> tint;
    InspectorProperty<Self,Descriptor<T,Attribute::ScreenTint>> screenTint;
    InspectorProperty<Self,Descriptor<T,Attribute::BlendingMode>> blendingMode;
    InspectorProperty<Self,Descriptor<T,Attribute::Opacity>> opacity;
    InspectorProperty<Self,Descriptor<T,Attribute::Threshold>> threshold;
    void capture(Selection<Node> nodes) override {
        auto self=std::static_pointer_cast<Self>(this->shared_from_this()); Base::capture(std::move(nodes));
        tint.get(self)->capture(); screenTint.get(self)->capture(); blendingMode.get(self)->capture();
        opacity.get(self)->capture(); threshold.get(self)->capture();
    }
    void run() override;
};
template<class T,PropertyId idValue,PropertyId parameter> struct DeformDescriptor {
    using Target=T; using Value=float;
    static constexpr auto id=idValue;
    template<class Owner> static float read(Owner& owner,const std::shared_ptr<T>& node) {
        return incInspectorDeformGetValue(node,owner.currParam,parameter.view(),owner.currCursor);
    }
    template<class Owner> static void writeBatch(Owner& owner,Selection<T> nodes,float value) {
        incInspectorDeformSetValue(Selection<Node>(nodes),owner.currParam,parameter.view(),owner.currCursor,value);
    }
    static std::string display(float value) {
        const std::array<core::translation::TypedFormatArgument,1> args{core::translation::ngFormatArgument(value)};
        return core::translation::ngFormatTyped("%s",args);
    }
};
template<class T> requires std::derived_from<T,Composite>
class NodeInspector<ModelEditSubMode::Deform,T> : public BaseInspector<ModelEditSubMode::Deform,T>,
    public MultiEdit<NodeInspector<ModelEditSubMode::Deform,T>> {
    using Self=NodeInspector;
    using Base=BaseInspector<ModelEditSubMode::Deform,T>;
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
    template<PropertyId name,PropertyId param> using Property=InspectorProperty<Self,DeformDescriptor<T,name,param>,true>;
    Property<"opacity","opacity"> opacity;
    Property<"tintR","tint.r"> tintR; Property<"tintG","tint.g"> tintG; Property<"tintB","tint.b"> tintB;
    Property<"screenTintR","screenTint.r"> screenTintR; Property<"screenTintG","screenTint.g"> screenTintG;
    Property<"screenTintB","screenTint.b"> screenTintB;
    void capture(Selection<Node> nodes) override {
        auto self=std::static_pointer_cast<Self>(this->shared_from_this()); Base::capture(std::move(nodes));
        opacity.get(self)->capture(); tintR.get(self)->capture(); tintG.get(self)->capture(); tintB.get(self)->capture();
        screenTintR.get(self)->capture(); screenTintG.get(self)->capture(); screenTintB.get(self)->capture();
    }
    void run(std::shared_ptr<Parameter> parameter,Cursor cursor) override;
};
}
#include "composite_run.hpp"
