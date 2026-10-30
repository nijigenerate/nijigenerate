#include "base.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace nijigenerate::panels::inspector {
namespace {
using InspectorPtr = std::shared_ptr<TypedInspector<Node>>;
compat::Slice<InspectorPtr> allocateInspectors(std::size_t count) {
    if (!count) return {};
    try {
        return compat::Slice<InspectorPtr>(std::vector<InspectorPtr>(count));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

void initInspectors() {
    using namespace nicxlive::core::nodes;
    constexpr auto deform = ModelEditSubMode::Deform;
    constexpr auto layout = ModelEditSubMode::Layout;
    ngRegisterInspector<deform, Node>();
    ngRegisterInspector<deform, Part>();
    ngRegisterInspector<deform, Composite>();
    ngRegisterInspector<deform, SimplePhysicsDriver>();
    ngRegisterInspector<deform, PathDeformer>();
    ngRegisterInspector<deform, GridDeformer>();
    ngRegisterInspector<layout, Node>();
    ngRegisterInspector<layout, ext::nodes::ExCamera>();
    ngRegisterInspector<layout, Composite>();
    ngRegisterInspector<layout, Drawable>();
    ngRegisterInspector<layout, Part>();
    ngRegisterInspector<layout, SimplePhysicsDriver>();
    ngRegisterInspector<layout, MeshGroup>();
    ngRegisterInspector<layout, PathDeformer>();
    ngRegisterInspector<layout, GridDeformer>();
    ngRegisterInspector<layout, ext::nodes::ExDepthRigRoot>();
    ngRegisterInspector<layout, ext::nodes::ExDepthBone>();
    ngRegisterInspector<layout, nicxlive::core::Puppet>();
    detail::inspectorsInitialized = true;
}
compat::Slice<std::shared_ptr<TypedInspector<Node>>> instantiateInspectors() {
    const auto factories = detail::nodeInspectors;
    auto result = allocateInspectors(factories.size());
    for (std::size_t i = 0; i < factories.size(); ++i) {
        const auto factory = factories[i];
        result[i] = factory();
    }
    return result;
}
}

std::shared_ptr<InspectorHolder<Node>> ngNodeInspector(Selection<Node> targets) {
    if (!detail::inspectorsInitialized) initInspectors();
    const auto mode = project::ngModelEditSubMode();
    auto result = ngCreateInspector<InspectorHolder<Node>>(targets, mode);
    const auto factories = detail::nodeInspectors;
    // map over this array has length: std.array allocates before calling factories.
    auto inspectors = allocateInspectors(factories.size());
    for (std::size_t i = 0; i < factories.size(); ++i) {
        const auto factory = factories[i];
        inspectors[i] = factory();
    }
    result->setInspectors(inspectors);
    return result;
}

void InspectorPanel::onChange(std::shared_ptr<Node>, nicxlive::core::nodes::NotifyReason reason) {
    using nicxlive::core::nodes::NotifyReason;
    auto owner = shared_from_this();
    if ((reason == NotifyReason::StructureChanged || reason == NotifyReason::AttributeChanged) &&
            activeNodeInspectors) {
        const auto inspector = activeNodeInspectors;
        inspector->capture(activeNodeInspectors->getTargets());
    }
}

void InspectorPanel::onSelectionChanged(compat::Slice<std::shared_ptr<Node>> nodes) {
    auto owner = shared_from_this();
    const auto mode = project::ngModelEditSubMode();
    activeNodeInspectors = ngCreateInspector<InspectorHolder<Node>>(Selection<Node>(nodes), mode);
    const auto inspector = activeNodeInspectors;
    inspector->setInspectors(instantiateInspectors());
}

void InspectorPanel::onUpdate() {
    auto owner = shared_from_this();
    const auto subMode = project::ngModelEditSubMode();
    const bool projectChanged = [this] {
        const auto current = project::incActiveProject();
        return !nicxlive::core::common::ngClassObjectEquals(current, activeProject);
    }();
    if (projectChanged) {
        activeProject = project::incActiveProject();
        compat::ngRequireObject(activeProject)->SelectionChanged.connect<&InspectorPanel::onSelectionChanged>(*this);
    }
    const bool puppetChanged = [this] {
        const auto current = project::incActivePuppet();
        return !nicxlive::core::ngPuppetObjectEquals(current, activePuppet);
    }();
    if (puppetChanged) {
        activePuppet = project::incActivePuppet();
        if (activePuppet) {
            const auto rootNode = compat::ngRequireObject(activePuppet)->root;
            compat::ngRequireObject(rootNode)->addNotifyListener(
                Node::NotifyListener::fromMethod<&InspectorPanel::onChange>(owner));
        }
        activeNodeInspectors.reset();
    }
    if (!activeNodeInspectors) onSelectionChanged(project::incSelectedNodes());
    if (activeNodeInspectors) compat::ngRequireObject(activeNodeInspectors)->subMode(subMode);
    if (project::incEditMode() == project::EditMode::VertexEdit) {
        widgets::ngLabelOver(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("In vertex edit mode...")), ImVec2(0, 0), true);
        return;
    }
    const auto nodes = project::incSelectedNodes();
    if (!nodes.empty()) {
        if (project::incEditMode() == project::EditMode::ModelEdit) {
            const auto parameter = project::incArmedParameter();
            Cursor cursor{};
            if (parameter) {
                const auto nearest = parameter->findClosestKeypoint();
                cursor = {static_cast<std::uint32_t>(nearest.x), static_cast<std::uint32_t>(nearest.y)};
            }
            if (activeNodeInspectors) compat::ngRequireObject(activeNodeInspectors)->inspect(parameter, cursor);
        } else {
            incCommonNonEditHeader(nodes[0]);
        }
    } else {
        widgets::ngLabelOver(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("No nodes selected...")), ImVec2(0, 0), true);
    }
}

InspectorPanel::InspectorPanel()
    : Panel(compat::RetainedString::fromStaticLiteral("Inspector"),
        core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Inspector")), true) {
    supportsReadOnlyInteraction = true;
    activeModes = project::EditMode::ModelEdit;
    // Preserve the source constructor's unconditional append, including the
    // case where ngNodeInspector initialized the factory array beforehand.
    initInspectors();
}
void ngInitializeInspectorPanelModule() { ngInitializePanelModule<InspectorPanel>(); }
} // namespace nijigenerate::panels::inspector
