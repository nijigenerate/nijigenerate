#pragma once
#include "common.hpp"
#include <nodes/drawable.hpp>
#include <nodes/part.hpp>
#include <nodes/mesh_group.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
namespace nijigenerate::commands { class Context; class CommandResult; }
namespace nijigenerate::commands::inspector::apply_node {
std::shared_ptr<CommandResult> ngRunDrawableOffsetCommand(bool y, std::shared_ptr<Context>, float);
}
namespace nijigenerate::panels::inspector::drawable {
template<ModelEditSubMode targetMode, class T>
    requires (targetMode == ModelEditSubMode::Layout && std::derived_from<T, nicxlive::core::nodes::Drawable> &&
        !std::derived_from<T, nicxlive::core::nodes::Part> && !std::derived_from<T, nicxlive::core::nodes::MeshGroup>)
class NodeInspector;
}

namespace nicxlive::core::d_runtime {
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires (Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout &&
        std::derived_from<T, nodes::Drawable> && !std::derived_from<T, nodes::Part> &&
        !std::derived_from<T, nodes::MeshGroup>)
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::drawable::NodeInspector<Mode, T>> {
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
            std::string name = qualified ? "nijigenerate.panels.inspector.drawable." : "";
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
        std::derived_from<T, nodes::Drawable> && !std::derived_from<T, nodes::Part> &&
        !std::derived_from<T, nodes::MeshGroup>)
struct SourceTypeSpellings<::nijigenerate::panels::inspector::drawable::NodeInspector<Mode, T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::drawable::NodeInspector<Mode, T>>;
        const auto qualified = Names::element(true);
        const auto unqualified = Names::element(false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};
}

namespace nijigenerate::panels::inspector::drawable {
template<class T, bool y> struct OffsetDescriptor {
    using Target = T;
    using Value = float;
    static constexpr auto id = [] { if constexpr (y) return PropertyId("offsetY"); else return PropertyId("offsetX"); }();
    template<class Owner> static float read(Owner&, std::shared_ptr<T> node) {
        if constexpr (y) return compat::ngRequireObject(node)->getMesh().origin.y;
        else return compat::ngRequireObject(node)->getMesh().origin.x;
    }
    template<class Owner> static void write(Owner&, std::shared_ptr<T> node, float value) {
        if constexpr (y) compat::ngRequireObject(node)->getMesh().origin.y = value;
        else compat::ngRequireObject(node)->getMesh().origin.x = value;
    }
    static std::string display(float value) {
        const std::array<core::translation::TypedFormatArgument, 1> args{core::translation::ngFormatArgument(value)};
        return core::translation::ngFormatTyped("%s", args);
    }
};
template<ModelEditSubMode targetMode, class T>
    requires (targetMode == ModelEditSubMode::Layout && std::derived_from<T, nicxlive::core::nodes::Drawable> &&
        !std::derived_from<T, nicxlive::core::nodes::Part> && !std::derived_from<T, nicxlive::core::nodes::MeshGroup>)
class NodeInspector : public BaseInspector<targetMode,T>, public MultiEdit<NodeInspector<targetMode,T>> {
    using Self = NodeInspector;
    using Base = BaseInspector<targetMode,T>;
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
    InspectorProperty<Self,OffsetDescriptor<T,false>> offsetX;
    InspectorProperty<Self,OffsetDescriptor<T,true>> offsetY;
    void capture(Selection<Node> nodes) override {
        auto owner = std::static_pointer_cast<Self>(this->retainSelf());
        Base::capture(std::move(nodes));
        offsetX.get(owner)->capture(); offsetY.get(owner)->capture();
    }
    void run() override;
};
}
#include "drawable_run.hpp"
