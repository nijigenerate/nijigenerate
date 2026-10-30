#pragma once

#include "common.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nodes/path_deformer.hpp>
#include <limits>
#include <memory>
#include <type_traits>
#include "griddeform.hpp"
#include <nijigenerate/commands/inspector/apply_node.hpp>
#include <variant>
#include <functional>

namespace nijigenerate::commands { class Context; }
namespace nijigenerate::panels::inspector::pathdeform {

using PathDeformer = nicxlive::core::nodes::PathDeformer;
using CurveType = PathDeformer::CurveType;
template<ModelEditSubMode mode, class T = PathDeformer> requires std::derived_from<T, PathDeformer>
class NodeInspector;
}

namespace nicxlive::core::d_runtime {
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires ((Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout ||
        Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Deform) &&
        std::derived_from<T, ::nicxlive::core::nodes::PathDeformer>)
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::pathdeform::NodeInspector<Mode, T>> {
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
            std::string name = qualified ? "nijigenerate.panels.inspector.pathdeform." : "";
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
        std::derived_from<T, ::nicxlive::core::nodes::PathDeformer>)
struct SourceTypeSpellings<::nijigenerate::panels::inspector::pathdeform::NodeInspector<Mode, T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::pathdeform::NodeInspector<Mode, T>>;
        const auto qualified = Names::element(true);
        const auto unqualified = Names::element(false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};
}

namespace nijigenerate::panels::inspector::pathdeform {
using PropertyValue=std::variant<std::reference_wrapper<const bool>,std::reference_wrapper<const float>,
    std::reference_wrapper<const CurveType>>;
void ngRunPathPropertyCommand(commands::inspector::apply_node::InspectorNodeApplyCommand,
    std::shared_ptr<Inspector>,Selection<Node>,PropertyValue);
void ngRunPathPropertyCommand(commands::inspector::apply_node::InspectorNodeApplyCommand,
    std::shared_ptr<commands::Context>,PropertyValue);

namespace detail {
enum class Attribute {
    Dynamic, PhysicsEnabled, CurveType, Gravity, RestoreConstant, Damping, InputScale, PropagateScale
};

// The native runtime stores the real connected driver inside its adapter.
// Legacy and spring drivers intentionally fail the pendulum-only source cast.
inline std::shared_ptr<nicxlive::core::nodes::ConnectedPendulumDriver> pendulum(PathDeformer& node) {
    const auto adapter = std::dynamic_pointer_cast<PathDeformer::ConnectedDriverAdapter>(node.getDriverSource());
    auto* driver = adapter ? dynamic_cast<nicxlive::core::nodes::ConnectedPendulumDriver*>(adapter->impl()) : nullptr;
    return driver ? std::shared_ptr<nicxlive::core::nodes::ConnectedPendulumDriver>(adapter, driver) : nullptr;
}

template<class T, Attribute attribute> struct Descriptor {
    using Target = T;
    using Value = std::conditional_t<attribute == Attribute::CurveType, CurveType,
        std::conditional_t<attribute == Attribute::Dynamic || attribute == Attribute::PhysicsEnabled, bool, float>>;
    static constexpr auto id = [] {
        if constexpr (attribute == Attribute::Dynamic) return PropertyId("dynamic");
        else if constexpr (attribute == Attribute::PhysicsEnabled) return PropertyId("physicsEnabled");
        else if constexpr (attribute == Attribute::CurveType) return PropertyId("curveType");
        else if constexpr (attribute == Attribute::Gravity) return PropertyId("gravity");
        else if constexpr (attribute == Attribute::RestoreConstant) return PropertyId("restoreConstant");
        else if constexpr (attribute == Attribute::Damping) return PropertyId("damping");
        else if constexpr (attribute == Attribute::InputScale) return PropertyId("inputScale");
        else return PropertyId("propagateScale");
    }();

    template<class Owner> static Value read(Owner&, const std::shared_ptr<T>& node) {
        if constexpr (attribute == Attribute::Dynamic) return compat::ngRequireObject(node)->dynamicDeformation;
        else if constexpr (attribute == Attribute::PhysicsEnabled) return compat::ngRequireObject(node)->physicsEnabled();
        else if constexpr (attribute == Attribute::CurveType) return compat::ngRequireObject(node)->curveType;
        else {
            auto driver = pendulum(*compat::ngRequireObject(node));
            if (!driver) return std::numeric_limits<float>::quiet_NaN();
            if constexpr (attribute == Attribute::Gravity) return driver->gravity();
            else if constexpr (attribute == Attribute::RestoreConstant) return driver->restoreConstant();
            else if constexpr (attribute == Attribute::Damping) return driver->damping();
            else if constexpr (attribute == Attribute::InputScale) return driver->inputScale();
            else return driver->propagateScale();
        }
    }

    template<class Owner> static void write(Owner&, const std::shared_ptr<T>& node, Value value) {
        if constexpr (attribute == Attribute::Dynamic) compat::ngRequireObject(node)->switchDynamic(value);
        else if constexpr (attribute == Attribute::PhysicsEnabled) compat::ngRequireObject(node)->setPhysicsEnabled(value);
        else if constexpr (attribute == Attribute::CurveType) {
            compat::ngRequireObject(node)->curveType = value;
            const auto receiver = compat::ngRequireObject(node);
            const auto vertices = compat::ngRequireObject(node)->getVertices();
            receiver->rebuffer(vertices);
        } else {
            if (auto driver = pendulum(*compat::ngRequireObject(node))) {
                if constexpr (attribute == Attribute::Gravity) driver->gravity(value);
                else if constexpr (attribute == Attribute::RestoreConstant) driver->restoreConstant(value);
                else if constexpr (attribute == Attribute::Damping) driver->damping(value);
                else if constexpr (attribute == Attribute::InputScale) driver->inputScale(value);
                else driver->propagateScale(value);
            }
        }
    }

    static std::string display(Value value) requires std::is_same_v<Value, float> {
        const std::array<core::translation::TypedFormatArgument, 1> arguments{
            core::translation::ngFormatArgument(value)};
        return core::translation::ngFormatTyped("%s", arguments);
    }
};

// Both source static-if branches contain the same generated attribute state.
// Construction still runs through ngCreateInspector and the accepted base's
// post-allocation virtual capture; no C++ base-constructor dispatch is used.
template<ModelEditSubMode mode, class T, class Owner = NodeInspector<mode, T>>
    requires std::derived_from<T, PathDeformer>
class NodeInspectorState : public BaseInspector<mode, T>, public MultiEdit<Owner> {
    using Self = Owner;
    using Base = BaseInspector<mode, T>;
    template<Attribute attribute> using Property = typename MultiEdit<Self>::template Attribute<Descriptor<T, attribute>>;
    inline static thread_local griddeform::SessionMap sourceSettingsSessions;
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        ::nijigenerate::panels::inspector::detail::prepareInspectorClassRecord<Self>();
    }
public:
    NodeInspectorState()=default;
    NodeInspectorState(const NodeInspectorState&)=delete;
    NodeInspectorState(NodeInspectorState&&)=delete;
    NodeInspectorState& operator=(const NodeInspectorState&)=delete;
    NodeInspectorState& operator=(NodeInspectorState&&)=delete;
    Property<Attribute::Dynamic> dynamic;
    Property<Attribute::PhysicsEnabled> physicsEnabled;
    Property<Attribute::CurveType> curveType;
    Property<Attribute::Gravity> gravity;
    Property<Attribute::RestoreConstant> restoreConstant;
    Property<Attribute::Damping> damping;
    Property<Attribute::InputScale> inputScale;
    Property<Attribute::PropagateScale> propagateScale;
    using SharedValue_dynamic = typename decltype(dynamic)::State;
    using SharedValue_physicsEnabled = typename decltype(physicsEnabled)::State;
    using SharedValue_curveType = typename decltype(curveType)::State;
    using SharedValue_gravity = typename decltype(gravity)::State;
    using SharedValue_restoreConstant = typename decltype(restoreConstant)::State;
    using SharedValue_damping = typename decltype(damping)::State;
    using SharedValue_inputScale = typename decltype(inputScale)::State;
    using SharedValue_propagateScale = typename decltype(propagateScale)::State;

    void capture(Selection<Node> nodes) override {
        auto owner = std::static_pointer_cast<Self>(this->shared_from_this());
        Base::capture(std::move(nodes));
        physicsEnabled.get(owner)->capture();
        curveType.get(owner)->capture();
        gravity.get(owner)->capture();
        restoreConstant.get(owner)->capture();
        damping.get(owner)->capture();
        inputScale.get(owner)->capture();
        propagateScale.get(owner)->capture();
        // The source deliberately does not capture dynamic here.
    }

protected:
    static std::shared_ptr<ext::nodes::ExDepthBone> findDepthBone(
        std::shared_ptr<ext::nodes::ExDepthRigRoot> root, std::uint64_t uuid) {
        for (auto bone : compat::ngRequireObject(root)->depthBones())
            if (compat::ngRequireObject(bone)->getUUID() == uuid) return bone;
        return {};
    }

    // Both source branches use the genuine depthbone UI with this original's
    // distinct settings-session storage and Path target kind.
    void drawDepthBoneSources(std::shared_ptr<PathDeformer> node,
        std::shared_ptr<Parameter> parameter = {}, Cursor cursor = {}) {
        griddeform::ngDrawGridDepthBoneSources(std::move(node),std::move(parameter),cursor,
            sourceSettingsSessions,ext::nodes::ExDepthTargetKind::Path);
    }
};
} // namespace detail

template<class T> requires std::derived_from<T, PathDeformer>
class NodeInspector<ModelEditSubMode::Layout, T>
    : public detail::NodeInspectorState<ModelEditSubMode::Layout, T> {
public:
    // The actual Layout implementation requires Context, typed apply commands
    // and the depthbone UI. It must not be replaced by an empty run body.
    void run() override;
};

template<class T> requires std::derived_from<T, PathDeformer>
class NodeInspector<ModelEditSubMode::Deform, T>
    : public detail::NodeInspectorState<ModelEditSubMode::Deform, T> {
public:
    void run(std::shared_ptr<Parameter> parameter, Cursor cursor) override {
        const auto keepAlive = this->shared_from_this();
        if (this->targets.empty()) return;
        this->drawDepthBoneSources(compat::ngArrayAt(this->targets, 0), std::move(parameter), cursor);
    }
};

} // namespace nijigenerate::panels::inspector::pathdeform
#include "pathdeform_run.hpp"
