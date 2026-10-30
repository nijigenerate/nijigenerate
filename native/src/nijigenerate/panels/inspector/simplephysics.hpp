#pragma once
// Independent panels/inspector/simplephysics.d, BSD-2-Clause.
#include "common.hpp"
#include <nodes/simple_physics_driver.hpp>
#include <nijigenerate/commands/inspector/apply_node.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <variant>
#include <functional>
namespace nijigenerate::commands { class Context; class CommandResult; }
namespace nijigenerate::panels::inspector {
// The C++ carrier is a class, but these D enum operands are string descriptors.
template<> struct InspectorComparisonSnapshot<nicxlive::core::nodes::SourcePhysicsModel> : std::true_type {};
template<> struct InspectorComparisonSnapshot<nicxlive::core::nodes::SourceParamMapMode> : std::true_type {};
}
namespace nijigenerate::panels::inspector::simplephysics {
using SimplePhysics = nicxlive::core::nodes::SimplePhysicsDriver;
using PhysicsModel = nicxlive::core::nodes::SourcePhysicsModel;
using ParamMapMode = nicxlive::core::nodes::SourceParamMapMode;
std::string ngPhysicsModelName(PhysicsModel);
std::string ngPhysicsMapModeName(ParamMapMode);
using PropertyValue = std::variant<std::reference_wrapper<const PhysicsModel>,
    std::reference_wrapper<const ParamMapMode>,std::reference_wrapper<const bool>,
    std::reference_wrapper<const float>>;
std::shared_ptr<commands::CommandResult> ngRunSimplePhysicsPropertyCommand(
    commands::inspector::apply_node::InspectorNodeApplyCommand,std::shared_ptr<commands::Context>,PropertyValue);
void ngRunSimplePhysicsParameterCommand(std::shared_ptr<commands::Context>,
    const std::shared_ptr<Parameter>&,bool clear);
template<ModelEditSubMode mode,class T> requires std::derived_from<T,SimplePhysics> class NodeInspector;
}

namespace nicxlive::core::d_runtime {
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires ((Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout ||
        Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Deform) &&
        std::derived_from<T, ::nicxlive::core::nodes::SimplePhysicsDriver>)
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::simplephysics::NodeInspector<Mode, T>> {
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
            std::string name = qualified ? "nijigenerate.panels.inspector.simplephysics." : "";
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
        std::derived_from<T, ::nicxlive::core::nodes::SimplePhysicsDriver>)
struct SourceTypeSpellings<::nijigenerate::panels::inspector::simplephysics::NodeInspector<Mode, T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::simplephysics::NodeInspector<Mode, T>>;
        const auto qualified = Names::element(true);
        const auto unqualified = Names::element(false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};
}

namespace nijigenerate::panels::inspector::simplephysics {
template<class T,PropertyId prop> struct Descriptor {
    using Target=T;
    using Value=std::conditional_t<prop.view()=="modelType",PhysicsModel,
        std::conditional_t<prop.view()=="mapMode",ParamMapMode,
        std::conditional_t<prop.view()=="localOnly",bool,float>>>;
    static constexpr auto id=prop;
    template<class Owner> static Value read(Owner&,const std::shared_ptr<T>& node) {
        if constexpr(prop.view()=="modelType") return compat::ngRequireObject(node)->getModelType();
        else if constexpr(prop.view()=="mapMode") return compat::ngRequireObject(node)->mapMode;
        else if constexpr(prop.view()=="localOnly") return compat::ngRequireObject(node)->localOnly;
        else if constexpr(prop.view()=="gravity") return compat::ngRequireObject(node)->gravity;
        else if constexpr(prop.view()=="length") return compat::ngRequireObject(node)->length;
        else if constexpr(prop.view()=="frequency") return compat::ngRequireObject(node)->frequency;
        else if constexpr(prop.view()=="angleDamping") return compat::ngRequireObject(node)->angleDamping;
        else if constexpr(prop.view()=="lengthDamping") return compat::ngRequireObject(node)->lengthDamping;
        else if constexpr(prop.view()=="outputScaleX") return compat::ngRequireObject(node)->outputScale[0];
        else if constexpr(prop.view()=="outputScaleY") return compat::ngRequireObject(node)->outputScale[1];
    }
    template<class Owner> static void write(Owner&,const std::shared_ptr<T>& node,Value value) {
        if constexpr(prop.view()=="modelType") compat::ngRequireObject(node)->setModelType(value);
        else {
            if(!node) compat::ngFailNullAccess(true);
            if constexpr(prop.view()=="mapMode") node->mapMode=value;
            else if constexpr(prop.view()=="localOnly") node->localOnly=value;
            else if constexpr(prop.view()=="gravity") node->gravity=value;
            else if constexpr(prop.view()=="length") node->length=value;
            else if constexpr(prop.view()=="frequency") node->frequency=value;
            else if constexpr(prop.view()=="angleDamping") node->angleDamping=value;
            else if constexpr(prop.view()=="lengthDamping") node->lengthDamping=value;
            else if constexpr(prop.view()=="outputScaleX") node->outputScale[0]=value;
            else if constexpr(prop.view()=="outputScaleY") node->outputScale[1]=value;
        }
    }
    static std::string display(Value value) {
        if constexpr(std::same_as<Value,PhysicsModel>) return ngPhysicsModelName(value);
        else if constexpr(std::same_as<Value,ParamMapMode>) return ngPhysicsMapModeName(value);
        else { const std::array<core::translation::TypedFormatArgument,1> args{core::translation::ngFormatArgument(value)};
            return core::translation::ngFormatTyped("%s",args); }
    }
};
template<class T> requires std::derived_from<T,SimplePhysics>
class NodeInspector<ModelEditSubMode::Layout,T> : public BaseInspector<ModelEditSubMode::Layout,T>,
    public MultiEdit<NodeInspector<ModelEditSubMode::Layout,T>> {
    using Self=NodeInspector; using Base=BaseInspector<ModelEditSubMode::Layout,T>;
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        ::nijigenerate::panels::inspector::detail::prepareInspectorClassRecord<Self>();
    }
public:
    NodeInspector()=default;
    NodeInspector(const NodeInspector&)=delete;
    NodeInspector(NodeInspector&&)=delete;
    NodeInspector& operator=(const NodeInspector&)=delete;
    NodeInspector& operator=(NodeInspector&&)=delete;
    InspectorProperty<Self,Descriptor<T,"modelType">> modelType;
    InspectorProperty<Self,Descriptor<T,"mapMode">> mapMode;
    InspectorProperty<Self,Descriptor<T,"localOnly">> localOnly;
    InspectorProperty<Self,Descriptor<T,"gravity">> gravity;
    InspectorProperty<Self,Descriptor<T,"length">> length;
    InspectorProperty<Self,Descriptor<T,"frequency">> frequency;
    InspectorProperty<Self,Descriptor<T,"angleDamping">> angleDamping;
    InspectorProperty<Self,Descriptor<T,"lengthDamping">> lengthDamping;
    InspectorProperty<Self,Descriptor<T,"outputScaleX">> outputScaleX;
    InspectorProperty<Self,Descriptor<T,"outputScaleY">> outputScaleY;
    void capture(Selection<Node> nodes) override {
        auto self=std::static_pointer_cast<Self>(this->shared_from_this()); Base::capture(nodes);
        localOnly.get(self)->capture();
        modelType.get(self)->capture();
        mapMode.get(self)->capture();
        gravity.get(self)->capture();
        length.get(self)->capture();
        frequency.get(self)->capture();
        angleDamping.get(self)->capture();
        lengthDamping.get(self)->capture();
        outputScaleX.get(self)->capture();
        outputScaleY.get(self)->capture();
    }
    void run() override;
};
template<class T,PropertyId name,PropertyId parameter> struct DeformDescriptor {
    using Target=T; using Value=float; static constexpr auto id=name;
    template<class Owner> static float read(Owner& owner,const std::shared_ptr<T>& node) {
        return incInspectorDeformGetValue(node,owner.currParam,
            compat::RetainedString::fromStaticView(parameter.view()),owner.currCursor);
    }
    template<class Owner> static void writeBatch(Owner& owner,Selection<T> nodes,float value) {
        incInspectorDeformSetValue(Selection<Node>(nodes),owner.currParam,
            compat::RetainedString::fromStaticView(parameter.view()),owner.currCursor,value);
    }
    static std::string display(float value) { const std::array<core::translation::TypedFormatArgument,1> args{
        core::translation::ngFormatArgument(value)}; return core::translation::ngFormatTyped("%s",args); }
};
template<class T> requires std::derived_from<T,SimplePhysics>
class NodeInspector<ModelEditSubMode::Deform,T> : public BaseInspector<ModelEditSubMode::Deform,T>,
    public MultiEdit<NodeInspector<ModelEditSubMode::Deform,T>> {
    using Self=NodeInspector; using Base=BaseInspector<ModelEditSubMode::Deform,T>;
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        ::nijigenerate::panels::inspector::detail::prepareInspectorClassRecord<Self>();
    }
public:
    NodeInspector()=default;
    NodeInspector(const NodeInspector&)=delete;
    NodeInspector(NodeInspector&&)=delete;
    NodeInspector& operator=(const NodeInspector&)=delete;
    NodeInspector& operator=(NodeInspector&&)=delete;
    InspectorProperty<Self,DeformDescriptor<T,"gravity","gravity">,true> gravity;
    InspectorProperty<Self,DeformDescriptor<T,"length","length">,true> length;
    InspectorProperty<Self,DeformDescriptor<T,"frequency","frequency">,true> frequency;
    InspectorProperty<Self,DeformDescriptor<T,"angleDamping","angleDamping">,true> angleDamping;
    InspectorProperty<Self,DeformDescriptor<T,"lengthDamping","lengthDamping">,true> lengthDamping;
    InspectorProperty<Self,DeformDescriptor<T,"outputScaleX","outputScale.x">,true> outputScaleX;
    InspectorProperty<Self,DeformDescriptor<T,"outputScaleY","outputScale.y">,true> outputScaleY;
    void capture(Selection<Node> nodes) override {
        auto self=std::static_pointer_cast<Self>(this->shared_from_this()); Base::capture(nodes);
        gravity.get(self)->capture();
        length.get(self)->capture();
        frequency.get(self)->capture();
        angleDamping.get(self)->capture();
        lengthDamping.get(self)->capture();
        outputScaleX.get(self)->capture();
        outputScaleY.get(self)->capture();
    }
    void run(std::shared_ptr<Parameter>,Cursor) override;
};
}
#include "simplephysics_run.hpp"
