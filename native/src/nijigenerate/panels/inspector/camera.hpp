#pragma once
#include "common.hpp"
#include <nijigenerate/ext/nodes/excamera.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::commands { class Context; class CommandResult; }
namespace nijigenerate::commands::inspector::apply_node {
std::shared_ptr<CommandResult> ngRunCameraViewportCommand(std::shared_ptr<Context>, nicxlive::core::math::Vec2);
}

namespace nijigenerate::panels::inspector::camera {
template<ModelEditSubMode targetMode, class T>
    requires (targetMode == ModelEditSubMode::Layout && std::derived_from<T, ext::nodes::ExCamera>)
class NodeInspector;
}

namespace nicxlive::core::d_runtime {
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires (Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout &&
        std::derived_from<T, ::nijigenerate::ext::nodes::ExCamera>)
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::camera::NodeInspector<Mode, T>> {
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
            std::string name = qualified ? "nijigenerate.panels.inspector.camera." : "";
            name += "NodeInspector!(ModelEditSubMode.Layout, ";
            name.append(argument.view());
            name += ')';
            if (classLeaf) name += ".NodeInspector";
            return name;
        });
    }
};
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires (Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout &&
        std::derived_from<T, ::nijigenerate::ext::nodes::ExCamera>)
struct SourceTypeSpellings<::nijigenerate::panels::inspector::camera::NodeInspector<Mode, T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::camera::NodeInspector<Mode, T>>;
        const auto qualified = Names::element(true);
        const auto unqualified = Names::element(false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};
}

namespace nijigenerate::panels::inspector::camera {
template<class T> struct ViewportDescriptor {
    using Target = T;
    using Value = nicxlive::core::math::Vec2;
    static constexpr auto id = PropertyId("viewportOrigin");
    template<class Owner> static Value read(Owner&, const std::shared_ptr<T>& node) {
        const auto receiver = compat::ngRequireObject(node);
        return receiver->getViewport();
    }
    template<class Owner> static void write(Owner&, const std::shared_ptr<T>& node, Value value) {
        const auto receiver = compat::ngRequireObject(node);
        receiver->setViewport(value);
    }
    static std::string display(Value value) {
        const compat::Slice<float> items{value.x, value.y};
        const std::array<core::translation::TypedFormatArgument, 1> arguments{
            core::translation::ngFormatArgument(items)};
        return core::translation::ngFormatTyped("%s", arguments);
    }
};
template<ModelEditSubMode targetMode, class T>
    requires (targetMode == ModelEditSubMode::Layout && std::derived_from<T, ext::nodes::ExCamera>)
class NodeInspector : public BaseInspector<targetMode, T>, public MultiEdit<NodeInspector<targetMode, T>> {
    using Self = NodeInspector;
    using Base = BaseInspector<targetMode, T>;
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
    InspectorProperty<Self, ViewportDescriptor<T>> viewportOrigin;
    void capture(Selection<Node> targets) override {
        auto owner = std::static_pointer_cast<Self>(this->retainSelf());
        // Keep the argument descriptor alive through the property capture below.
        Base::capture(targets);
        viewportOrigin.get(owner)->capture();
    }
    void run() override;
};
}
#include "camera_run.hpp"
