#pragma once
#include "common.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nodes/mesh_group.hpp>
namespace nijigenerate::commands { class Context; class CommandResult; }
namespace nijigenerate::commands::inspector::apply_node {
std::shared_ptr<CommandResult> ngRunMeshGroupPropertyCommand(bool translate, std::shared_ptr<Context>, bool);
}
namespace nijigenerate::panels::inspector::meshgroup {
template<ModelEditSubMode targetMode, class T>
    requires(targetMode==ModelEditSubMode::Layout && std::derived_from<T,nicxlive::core::nodes::MeshGroup>)
class NodeInspector;
}

namespace nicxlive::core::d_runtime {
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires (Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout &&
        std::derived_from<T, nodes::MeshGroup>)
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::meshgroup::NodeInspector<Mode, T>> {
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
            std::string name = qualified ? "nijigenerate.panels.inspector.meshgroup." : "";
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
        std::derived_from<T, nodes::MeshGroup>)
struct SourceTypeSpellings<::nijigenerate::panels::inspector::meshgroup::NodeInspector<Mode, T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::meshgroup::NodeInspector<Mode, T>>;
        const auto qualified = Names::element(true);
        const auto unqualified = Names::element(false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};
}

namespace nijigenerate::panels::inspector::meshgroup {
template<class T, bool translate> struct Descriptor {
    using Target=T;
    using Value=bool;
    static constexpr auto id=[] { if constexpr(translate) return PropertyId("translateChildren"); else return PropertyId("dynamic"); }();
    template<class Owner> static bool read(Owner&,const std::shared_ptr<T>& node) {
        if constexpr(translate) return compat::ngRequireObject(node)->getTranslateChildren();
        else return compat::ngRequireObject(node)->dynamic;
    }
    template<class Owner> static void write(Owner&,const std::shared_ptr<T>& node,bool value) {
        if constexpr(translate) compat::ngRequireObject(node)->setTranslateChildren(value);
        else compat::ngRequireObject(node)->switchMode(value);
    }
    static std::string display(bool value) { return value ? "true" : "false"; }
};
template<ModelEditSubMode targetMode, class T>
    requires(targetMode==ModelEditSubMode::Layout && std::derived_from<T,nicxlive::core::nodes::MeshGroup>)
class NodeInspector : public BaseInspector<targetMode,T>, public MultiEdit<NodeInspector<targetMode,T>> {
    using Self=NodeInspector;
    using Base=BaseInspector<targetMode,T>;
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        ::nijigenerate::panels::inspector::detail::prepareInspectorClassRecord<Self>();
    }
public:
    InspectorProperty<Self,Descriptor<T,false>> dynamic;
    InspectorProperty<Self,Descriptor<T,true>> translateChildren;
    void capture(Selection<Node> nodes) override {
        auto owner=std::static_pointer_cast<Self>(this->shared_from_this());
        Base::capture(std::move(nodes));
        dynamic.get(owner)->capture(); translateChildren.get(owner)->capture();
    }
    void run() override;
};
}
#include "meshgroup_run.hpp"
