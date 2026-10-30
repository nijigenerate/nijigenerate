#include "deform.hpp"
#include "depthboneoverlay.hpp"
#include "mesheditor/editor.hpp"
#include <nijigenerate/viewport/main_viewport.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/actions/depthbone_publishers.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/source_allocator.hpp>
#include <param/binding_impl.hpp>

namespace nijigenerate::viewport::model::deform {
using namespace nicxlive::core;
using namespace nicxlive::core::nodes;
using namespace nicxlive::core::param;
using namespace ext::nodes;
using NodeRef = std::shared_ptr<Node>;
using Nodes = compat::Slice<NodeRef>;
using ParameterRef = std::shared_ptr<Parameter>;
using EditorRef = std::shared_ptr<common::mesheditor::IncMeshEditor>;

void DeformationViewport::draw(std::shared_ptr<math::Camera> camera) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (editor) {
        const auto current = editor;
        current->draw(camera);
    }
    if (project::ngShowDepthBones) {
        std::shared_ptr<ExDepthBone> selected;
        const auto nodes = project::incSelectedNodes();
        for (const auto node : nodes) {
            if (auto bone = std::dynamic_pointer_cast<ExDepthBone>(node)) { selected = bone; break; }
        }
        for (const auto root : findDepthRoots())
            depthboneoverlay::drawDepthBones(root, findDepthRoot(selected) == root ? selected : nullptr);
    }
}
std::shared_ptr<ExDepthRigRoot> DeformationViewport::findDepthRoot(std::shared_ptr<ExDepthBone> bone) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    NodeRef cursor = bone;
    while (cursor) {
        if (auto root = std::dynamic_pointer_cast<ExDepthRigRoot>(cursor)) return root;
        cursor = cursor->parentPtr();
    }
    return nullptr;
}
compat::Slice<std::shared_ptr<ExDepthRigRoot>> DeformationViewport::findDepthRoots() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    compat::Slice<std::shared_ptr<ExDepthRigRoot>> roots;
    const auto puppet = project::incActivePuppet();
    if (!puppet || !puppet->root) return roots;
    const auto visit = [&](auto&& self, NodeRef node) -> void {
        if (!node) return;
        if (auto root = std::dynamic_pointer_cast<ExDepthRigRoot>(node)) roots.append(root);
        const auto children = node->childrenList();
        for (const auto child : children) self(self, child);
    };
    visit(visit, puppet->root);
    return roots;
}
void DeformationViewport::drawTools() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (editor) {
        const auto current = editor;
        current->viewportTools();
    }
}
void DeformationViewport::drawOptions() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (editor) {
        const auto current = editor;
        current->displayToolOptions();
    }
}
void DeformationViewport::update(ImGuiIO* io, std::shared_ptr<math::Camera> camera) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!editor) return;
    bool changed;
    {
        const auto current = editor;
        changed = current->update(io, camera);
    }
    if (changed) {
        const auto nodes = project::incSelectedNodes();
        for (const auto node : nodes) {
            if (auto deformable = std::dynamic_pointer_cast<Deformable>(node)) {
                std::shared_ptr<DeformationParameterBinding> binding;
                {
                    const auto currentParameter = compat::ngRequireObject(parameter);
                    binding = std::dynamic_pointer_cast<DeformationParameterBinding>(
                        currentParameter->getOrAddBinding(deformable,
                            compat::RetainedString::fromStaticLiteral("deform")));
                }
                const auto keypoint = [this] {
                    const auto currentParameter = compat::ngRequireObject(parameter);
                    return currentParameter->findClosestKeypoint();
                }();
                const auto updateReceiver = compat::ngRequireObject(binding);
                const auto offsets = [this, &deformable] {
                    const auto current = compat::ngRequireObject(editor);
                    const auto nodeEditor = compat::ngRequireObject(current->getEditorFor(deformable));
                    return nodeEditor->getOffsets();
                }();
                updateReceiver->update(keypoint, offsets);
                actions::depthboneinvalidation::ngNotifyDepthBoneBindingValueChanged(binding,
                    {static_cast<std::uint32_t>(keypoint.x), static_cast<std::uint32_t>(keypoint.y)});
            }
        }
    }
}
void DeformationViewport::selectionChanged(Nodes nodes) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    depthboneoverlay::depthBoneEffectivePivotSelectionChanged(nodes);
    editor = nullptr;
    paramValueChanged();
}
void DeformationViewport::withdraw() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    depthboneoverlay::depthBoneEffectivePivotSelectionChanged({});
}
void DeformationViewport::armedParameterChanged(ParameterRef value) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    parameter = value;
    paramValueChanged();
}
void DeformationViewport::paramValueChanged() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (parameter) {
        const auto targets = project::incSelectedNodes();
        const auto argument = parameter;
        const auto keypoint = [this] {
            const auto currentParameter = compat::ngRequireObject(parameter);
            return currentParameter->findClosestKeypoint();
        }();
        syncEditor(argument, keypoint, targets);
    } else {
        editor = nullptr;
    }
}
void DeformationViewport::syncEditor(ParameterRef value, Nodes targets) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    syncEditor(value, value ? value->findClosestKeypoint() : Vec2u{0, 0}, targets);
}
void DeformationViewport::syncEditor(ParameterRef value, Vec2u keyPoint, Nodes targets) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    parameter = value;
    if (!value) {
        editor = nullptr;
        return;
    }
    if (targets.empty()) return;
    if (!editor) {
        editor = std::allocate_shared<mesheditor::editor::DeformMeshEditor>(
            nicxlive::core::d_runtime::SourceAllocator<mesheditor::editor::DeformMeshEditor>{});
    } else {
        const auto oldTargets = [this] {
            const auto current = compat::ngRequireObject(editor);
            return current->getTargets();
        }();
        for (const auto node : oldTargets) {
            const auto current = [this, &node] {
                const auto currentEditor = compat::ngRequireObject(editor);
                return currentEditor->getEditorFor(node);
            }();
            if (current) {
                current->pushDeformAction();
                current->forceResetAction();
            }
        }
    }
    {
        const auto current = compat::ngRequireObject(editor);
        current->setTargets(targets);
    }
    {
        const auto current = compat::ngRequireObject(editor);
        current->resetMesh();
    }
    const auto newTargets = [this] {
        const auto current = compat::ngRequireObject(editor);
        return current->getTargets();
    }();
    for (const auto node : newTargets) {
        const auto current = [this, &node] {
            const auto currentEditor = compat::ngRequireObject(editor);
            return currentEditor->getEditorFor(node);
        }();
        std::shared_ptr<DeformationParameterBinding> binding;
        if (auto deformable = std::dynamic_pointer_cast<Deformable>(node))
            binding = std::dynamic_pointer_cast<DeformationParameterBinding>(value->getBinding(deformable,
                compat::RetainedString::fromStaticLiteral("deform")));
        if (current) {
            if (binding) {
                // The original local value invokes Deformation's copying
                // postblit before its offsets are passed to the editor.
                const auto deformation = binding->getValue(keyPoint);
                current->applyOffsets(deformation.vertexOffsets);
            }
            current->adjustPathTransform();
        }
    }
}
void DeformationViewport::menu() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (editor) {
        const auto current = editor;
        current->popupMenu();
    }
}
std::shared_ptr<DeformationViewport> ngDeformationViewport() {
    if (auto model = std::dynamic_pointer_cast<base::DelegationViewport>(
            compat::ngRequireObject(base::ngViewport())->subView()))
        return std::dynamic_pointer_cast<DeformationViewport>(model->subView());
    return nullptr;
}
void ngViewportNodeDeformNotifyParamValueChanged() {
    if (auto view = ngDeformationViewport()) view->paramValueChanged();
}
EditorRef ngViewportModelDeformGetEditor() {
    if (auto view = ngDeformationViewport()) return view->editor;
    return nullptr;
}
EditorRef ngViewportModelDeformSyncEditor(ParameterRef parameter, Vec2u keyPoint, Nodes targets) {
    if (auto view = ngDeformationViewport()) {
        view->syncEditor(parameter, keyPoint, targets);
        return view->editor;
    }
    return nullptr;
}
}
