#pragma once

#include "common.hpp"
#include "detail/registry_state.hpp"
#include "node.hpp"
#include "puppet.hpp"
#include "drawable.hpp"
#include "part.hpp"
#include "composite.hpp"
#include "simplephysics.hpp"
#include "meshgroup.hpp"
#include "camera.hpp"
#include "pathdeform.hpp"
#include "griddeform.hpp"
#include "depthbone.hpp"
#include <nijigenerate/panels/package.hpp>

namespace nijigenerate::panels::inspector {
template<ModelEditSubMode mode, class T>
    requires (std::derived_from<T, Node> || std::derived_from<T, nicxlive::core::Puppet>)
void ngRegisterInspector() {
    using Factory = detail::NodeInspectorFactory;
    detail::nodeInspectors.append(Factory([]() -> std::shared_ptr<TypedInspector<Node>> {
        using namespace nicxlive::core::nodes;
        if constexpr (std::derived_from<T, nicxlive::core::Puppet>)
            return ngCreateInspector<puppet::PuppetInspector>(Selection<Node>{}, mode);
        else if constexpr (std::derived_from<T, ext::nodes::ExDepthRigRoot> ||
                           std::derived_from<T, ext::nodes::ExDepthBone>)
            return ngCreateInspector<depthbone::NodeInspector<mode, T>>(Selection<Node>{}, mode);
        else if constexpr (std::derived_from<T, ext::nodes::ExCamera>)
            return ngCreateInspector<camera::NodeInspector<mode, T>>(Selection<Node>{}, mode);
        else if constexpr (std::derived_from<T, Composite>)
            return ngCreateInspector<composite::NodeInspector<mode, T>>(Selection<Node>{}, mode);
        else if constexpr (std::derived_from<T, Part>)
            return ngCreateInspector<part::NodeInspector<mode, T>>(Selection<Node>{}, mode);
        else if constexpr (std::derived_from<T, SimplePhysicsDriver>)
            return ngCreateInspector<simplephysics::NodeInspector<mode, T>>(Selection<Node>{}, mode);
        else if constexpr (std::derived_from<T, MeshGroup>)
            return ngCreateInspector<meshgroup::NodeInspector<mode, T>>(Selection<Node>{}, mode);
        else if constexpr (std::derived_from<T, PathDeformer>)
            return ngCreateInspector<pathdeform::NodeInspector<mode, T>>(Selection<Node>{}, mode);
        else if constexpr (std::derived_from<T, GridDeformer>)
            return ngCreateInspector<griddeform::NodeInspector<mode, T>>(Selection<Node>{}, mode);
        else if constexpr (std::derived_from<T, Drawable>)
            return ngCreateInspector<drawable::NodeInspector<mode, T>>(Selection<Node>{}, mode);
        else
            return ngCreateInspector<node::NodeInspector<mode, T>>(Selection<Node>{}, mode);
    }));
}

std::shared_ptr<InspectorHolder<Node>> ngNodeInspector(Selection<Node> targets);

class InspectorPanel : public Panel, public compat::SignalReceiver,
        public std::enable_shared_from_this<InspectorPanel> {
    std::shared_ptr<nicxlive::core::Puppet> activePuppet;
    std::shared_ptr<project::Project> activeProject;
protected:
    void prepareSourceClassInfo() override {
        Panel::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(InspectorPanel),
                compat::RetainedString::fromStaticLiteral("nijigenerate.panels.inspector.base.InspectorPanel"));
            return true;
        }();
        (void)registered;
    }
    virtual void onChange(std::shared_ptr<Node> target, nicxlive::core::nodes::NotifyReason reason);
    virtual void onSelectionChanged(compat::Slice<std::shared_ptr<Node>> nodes);
    void onUpdate() override;
public:
    std::shared_ptr<InspectorHolder<Node>> activeNodeInspectors;
    InspectorPanel();
    template<class I> std::shared_ptr<I> getInspector() {
        return activeNodeInspectors ? activeNodeInspectors->template getInspector<I>() : nullptr;
    }
};
void ngInitializeInspectorPanelModule();
} // namespace nijigenerate::panels::inspector

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::panels::inspector::InspectorPanel> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.panels.inspector.base.InspectorPanel)";
    inline static constexpr char unqualifiedArgument[] = "(InspectorPanel)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.panels.inspector.base.InspectorPanel";
    inline static constexpr char unqualifiedElement[] = "InspectorPanel";
};
template<> struct SourceObjectTypeNames<::nijigenerate::panels::inspector::InspectorPanel> {
    inline static constexpr char className[] = "nijigenerate.panels.inspector.base.InspectorPanel";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.panels.inspector.base.InspectorPanel)";
};
}
