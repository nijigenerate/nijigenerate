#pragma once
// Independent panels/inspector/node.d, BSD-2-Clause.
#include "common.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/ext/nodes/excamera.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nodes/composite.hpp>
#include <nodes/mesh_group.hpp>
#include <nodes/drawable.hpp>
#include <nodes/simple_physics_driver.hpp>
#include <nodes/path_deformer.hpp>
#include <nodes/grid_deformer.hpp>
#include <nijigenerate/commands/inspector/apply_node.hpp>
#include <variant>
#include <functional>
namespace nijigenerate::commands { class Context; class CommandResult; }
namespace nijigenerate::panels::inspector::node {
using PropertyValue=std::variant<std::reference_wrapper<const float>,std::reference_wrapper<const bool>>;
std::shared_ptr<commands::CommandResult> ngRunNodeInspectorPropertyCommand(
    commands::inspector::apply_node::InspectorNodeApplyCommand,std::shared_ptr<commands::Context>,PropertyValue);
template<ModelEditSubMode mode,class T> inline constexpr bool eligible = std::derived_from<T,Node>
    && !std::derived_from<T,nicxlive::core::nodes::Composite>
    && !std::derived_from<T,nicxlive::core::nodes::MeshGroup>
    && !std::derived_from<T,nicxlive::core::nodes::Drawable>
    && !std::derived_from<T,nicxlive::core::nodes::SimplePhysicsDriver>
    && !std::derived_from<T,ext::nodes::ExCamera>
    && !std::derived_from<T,nicxlive::core::nodes::PathDeformer>
    && !std::derived_from<T,nicxlive::core::nodes::GridDeformer>
    && (mode!=ModelEditSubMode::Layout || (!std::derived_from<T,ext::nodes::ExDepthRigRoot>
        && !std::derived_from<T,ext::nodes::ExDepthBone>));
template<class T,PropertyId prop> struct Descriptor {
    using Target=T;
    using Value=std::conditional_t<prop.view()=="lockToRoot" || prop.view()=="pinToMesh" || prop.view()=="pixelSnap",bool,float>;
    static constexpr auto id=prop;
    template<class Owner> static Value read(Owner&,const std::shared_ptr<T>& value) {
        const auto node=compat::ngRequireObject(value);
        if constexpr(prop.view()=="translationX") return node->localTransform.translation.x;
        else if constexpr(prop.view()=="translationY") return node->localTransform.translation.y;
        else if constexpr(prop.view()=="translationZ") return node->localTransform.translation.z;
        else if constexpr(prop.view()=="rotationX") return node->localTransform.rotation.x;
        else if constexpr(prop.view()=="rotationY") return node->localTransform.rotation.y;
        else if constexpr(prop.view()=="rotationZ") return node->localTransform.rotation.z;
        else if constexpr(prop.view()=="scaleX") return node->localTransform.scale.x;
        else if constexpr(prop.view()=="scaleY") return node->localTransform.scale.y;
        else if constexpr(prop.view()=="lockToRoot") return node->lockToRootValue();
        else if constexpr(prop.view()=="pinToMesh") return node->pinToMesh;
        else if constexpr(prop.view()=="pixelSnap") return node->localTransform.pixelSnap;
        else if constexpr(prop.view()=="zSort") return node->relZSort();
    }
    template<class Owner> static void write(Owner&,const std::shared_ptr<T>& target,Value value) {
        const auto node=compat::ngRequireObject(target);
        if constexpr(prop.view()=="translationX") node->localTransform.translation.x=value;
        else if constexpr(prop.view()=="translationY") node->localTransform.translation.y=value;
        else if constexpr(prop.view()=="translationZ") node->localTransform.translation.z=value;
        else if constexpr(prop.view()=="rotationX") node->localTransform.rotation.x=value;
        else if constexpr(prop.view()=="rotationY") node->localTransform.rotation.y=value;
        else if constexpr(prop.view()=="rotationZ") node->localTransform.rotation.z=value;
        else if constexpr(prop.view()=="scaleX") node->localTransform.scale.x=value;
        else if constexpr(prop.view()=="scaleY") node->localTransform.scale.y=value;
        else if constexpr(prop.view()=="lockToRoot") node->setLockToRoot(value);
        else if constexpr(prop.view()=="pinToMesh") node->setPinToMesh(value);
        else if constexpr(prop.view()=="pixelSnap") node->localTransform.pixelSnap=value;
        else if constexpr(prop.view()=="zSort") node->zSort(value);
    }
    static std::string display(Value value) {
        const std::array<core::translation::TypedFormatArgument,1> args{core::translation::ngFormatArgument(value)};
        return core::translation::ngFormatTyped("%s",args);
    }
};
template<class T,PropertyId name,PropertyId param> struct DeformDescriptor {
    using Target=T; using Value=float; static constexpr auto id=name;
    template<class Owner> static float read(Owner& owner,const std::shared_ptr<T>& node) {
        return incInspectorDeformGetValue(node,owner.currParam,
            compat::RetainedString::fromBorrowedView(param.view()),owner.currCursor);
    }
    template<class Owner> static void writeBatch(Owner& owner,Selection<T> nodes,float value) {
        incInspectorDeformSetValue(Selection<Node>(nodes),owner.currParam,
            compat::RetainedString::fromBorrowedView(param.view()),owner.currCursor,value);
    }
    static std::string display(float value) {
        const std::array<core::translation::TypedFormatArgument,1> args{core::translation::ngFormatArgument(value)};
        return core::translation::ngFormatTyped("%s",args);
    }
};
template<ModelEditSubMode mode,class T=Node> requires eligible<mode,T> class NodeInspector;
}

namespace nicxlive::core::d_runtime {
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires (::nijigenerate::panels::inspector::node::eligible<Mode, T> &&
        (Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout ||
            Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Deform))
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::node::NodeInspector<Mode, T>> {
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
            std::string name = qualified ? "nijigenerate.panels.inspector.node." : "";
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
    requires (::nijigenerate::panels::inspector::node::eligible<Mode, T> &&
        (Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout ||
            Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Deform))
struct SourceTypeSpellings<::nijigenerate::panels::inspector::node::NodeInspector<Mode, T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::node::NodeInspector<Mode, T>>;
        const auto qualified = Names::element(true);
        const auto unqualified = Names::element(false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};
}

namespace nijigenerate::panels::inspector::node {
template<class T> requires eligible<ModelEditSubMode::Layout,T>
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
    typename MultiEdit<Self>::template Attribute<Descriptor<T,"translationX">> translationX =
        MultiEdit<Self>::template attribute<Descriptor<T,"translationX">>();
    InspectorProperty<Self,Descriptor<T,"translationY">> translationY;
    InspectorProperty<Self,Descriptor<T,"translationZ">> translationZ;
    InspectorProperty<Self,Descriptor<T,"rotationX">> rotationX;
    InspectorProperty<Self,Descriptor<T,"rotationY">> rotationY;
    InspectorProperty<Self,Descriptor<T,"rotationZ">> rotationZ;
    InspectorProperty<Self,Descriptor<T,"scaleX">> scaleX;
    InspectorProperty<Self,Descriptor<T,"scaleY">> scaleY;
    InspectorProperty<Self,Descriptor<T,"lockToRoot">> lockToRoot;
    InspectorProperty<Self,Descriptor<T,"pinToMesh">> pinToMesh;
    InspectorProperty<Self,Descriptor<T,"pixelSnap">> pixelSnap;
    InspectorProperty<Self,Descriptor<T,"zSort">> zSort;
    using SharedValue_translationX = typename decltype(translationX)::State;
    using SharedValue_translationY = typename decltype(translationY)::State;
    using SharedValue_translationZ = typename decltype(translationZ)::State;
    using SharedValue_rotationX = typename decltype(rotationX)::State;
    using SharedValue_rotationY = typename decltype(rotationY)::State;
    using SharedValue_rotationZ = typename decltype(rotationZ)::State;
    using SharedValue_scaleX = typename decltype(scaleX)::State;
    using SharedValue_scaleY = typename decltype(scaleY)::State;
    using SharedValue_lockToRoot = typename decltype(lockToRoot)::State;
    using SharedValue_pinToMesh = typename decltype(pinToMesh)::State;
    using SharedValue_pixelSnap = typename decltype(pixelSnap)::State;
    using SharedValue_zSort = typename decltype(zSort)::State;
    void capture(Selection<Node> nodes) override {
        auto self=std::static_pointer_cast<Self>(this->shared_from_this()); Base::capture(nodes);
        translationX.get(self)->capture();
        translationY.get(self)->capture();
        translationZ.get(self)->capture();
        rotationX.get(self)->capture();
        rotationY.get(self)->capture();
        rotationZ.get(self)->capture();
        scaleX.get(self)->capture();
        scaleY.get(self)->capture();
        lockToRoot.get(self)->capture();
        pinToMesh.get(self)->capture();
        pixelSnap.get(self)->capture();
        zSort.get(self)->capture();
    }
    void run() override;
};
template<class T> requires eligible<ModelEditSubMode::Deform,T>
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
    typename MultiEdit<Self>::template Deformation<DeformDescriptor<T,"translationX","transform.t.x">> translationX =
        MultiEdit<Self>::template deformation<DeformDescriptor<T,"translationX","transform.t.x">>();
    InspectorProperty<Self,DeformDescriptor<T,"translationY","transform.t.y">,true> translationY;
    InspectorProperty<Self,DeformDescriptor<T,"translationZ","transform.t.z">,true> translationZ;
    InspectorProperty<Self,DeformDescriptor<T,"rotationX","transform.r.x">,true> rotationX;
    InspectorProperty<Self,DeformDescriptor<T,"rotationY","transform.r.y">,true> rotationY;
    InspectorProperty<Self,DeformDescriptor<T,"rotationZ","transform.r.z">,true> rotationZ;
    InspectorProperty<Self,DeformDescriptor<T,"scaleX","transform.s.x">,true> scaleX;
    InspectorProperty<Self,DeformDescriptor<T,"scaleY","transform.s.y">,true> scaleY;
    InspectorProperty<Self,DeformDescriptor<T,"zSort","zSort">,true> zSort;
    using SharedValue_translationX = typename decltype(translationX)::State;
    using SharedValue_translationY = typename decltype(translationY)::State;
    using SharedValue_translationZ = typename decltype(translationZ)::State;
    using SharedValue_rotationX = typename decltype(rotationX)::State;
    using SharedValue_rotationY = typename decltype(rotationY)::State;
    using SharedValue_rotationZ = typename decltype(rotationZ)::State;
    using SharedValue_scaleX = typename decltype(scaleX)::State;
    using SharedValue_scaleY = typename decltype(scaleY)::State;
    using SharedValue_zSort = typename decltype(zSort)::State;
    void capture(Selection<Node> nodes) override {
        auto self=std::static_pointer_cast<Self>(this->shared_from_this()); Base::capture(nodes);
        translationX.get(self)->capture();
        translationY.get(self)->capture();
        translationZ.get(self)->capture();
        rotationX.get(self)->capture();
        rotationY.get(self)->capture();
        rotationZ.get(self)->capture();
        scaleX.get(self)->capture();
        scaleY.get(self)->capture();
        zSort.get(self)->capture();
    }
    void run(std::shared_ptr<Parameter>,Cursor) override;
};
}
#include "node_run.hpp"
