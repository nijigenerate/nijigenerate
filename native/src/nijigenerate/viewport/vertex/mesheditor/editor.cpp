#include "editor.hpp"
#include "drawable.hpp"
#include "deformable.hpp"
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/viewport/common/mesheditor/operations/node.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <compat/source_allocator.hpp>
#include <utility>

namespace nijigenerate::viewport::vertex::mesheditor::editor {
using namespace nicxlive::core::nodes;
namespace {
template<class T, class... Args> std::shared_ptr<T> allocateVertexSubeditor(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(args)...);
}
}
VertexMeshEditor::VertexMeshEditor() : IncMeshEditor(false) {
    initializeSource();
}
void VertexMeshEditor::initializeSource() {
    deformOnly = false;
    actionScope = core::actionstack::ngOpenActionStackScope(core::actionstack::ActionStackScopeUnit::VertexEdit);
}
VertexMeshEditor::~VertexMeshEditor() { closeScope(); }
void VertexMeshEditor::closeScope() {
    const auto keepAlive = weak_from_this().lock();
    if (actionScope) {
        const auto scope = actionScope;
        scope->close();
        actionScope.reset();
    }
}
void VertexMeshEditor::adjustToolModeForSelection() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    bool anyGrid = false, anyPath = false, anyOther = false;
    const auto keys = editors.keys();
    for (const auto node : keys) {
        if (std::dynamic_pointer_cast<GridDeformer>(node)) anyGrid = true;
        else if (std::dynamic_pointer_cast<PathDeformer>(node)) anyPath = true;
        else anyOther = true;
    }
    const auto current = getToolMode();
    if (anyGrid) {
        if (!anyPath && !anyOther) {
            if (current != VertexToolMode::Grid) setToolMode(VertexToolMode::Grid);
        } else if (current == VertexToolMode::Grid) {
            if (anyPath && !anyOther) {
                if (current != VertexToolMode::BezierDeform) setToolMode(VertexToolMode::BezierDeform);
            } else setToolMode(VertexToolMode::Points);
        }
    } else if (anyPath && !anyOther) {
        if (current != VertexToolMode::BezierDeform) setToolMode(VertexToolMode::BezierDeform);
    }
}
void VertexMeshEditor::addTarget(NodeRef target) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (editors.contains(target)) return;
    EditorRef subEditor;
    if (auto targetDrawable = std::dynamic_pointer_cast<Drawable>(target)) {
        subEditor = allocateVertexSubeditor<drawable::IncMeshEditorOneFor<Drawable, project::EditMode::VertexEdit>>();
        common::mesh::GridAxes axes;
        if (common::mesh::isGrid(targetDrawable->Drawable::getMesh().vertices, axes)) {
            compat::ngRequireObject(subEditor)->toolMode = VertexToolMode::Grid;
            toolMode = VertexToolMode::Grid;
        }
    } else if (std::dynamic_pointer_cast<Deformable>(target)) {
        subEditor = allocateVertexSubeditor<deformable::IncMeshEditorOneFor<Deformable, project::EditMode::VertexEdit>>();
        if (std::dynamic_pointer_cast<PathDeformer>(target)) compat::ngRequireObject(subEditor)->toolMode = VertexToolMode::BezierDeform;
        else if (std::dynamic_pointer_cast<GridDeformer>(target)) compat::ngRequireObject(subEditor)->toolMode = VertexToolMode::Grid;
    }
    compat::ngRequireObject(subEditor)->setTarget(target);
    compat::ngRequireObject(subEditor)->mirrorHoriz = mirrorHoriz;
    compat::ngRequireObject(subEditor)->mirrorVert = mirrorVert;
    compat::ngRequireObject(subEditor)->previewTriangulate = previewTriangulate;
    editors[target] = subEditor;
    adjustToolModeForSelection();
}
void VertexMeshEditor::setTargets(Nodes targets) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    decltype(editors) newEditors;
    for (const auto target : targets) {
        if (editors.contains(target)) {
            const auto found = editors.find(target);
            if (found == editors.end()) throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/vertex/mesheditor/editor.d"), 126);
            const auto existing = found->second;
            newEditors[target] = existing;
        } else {
            EditorRef subEditor;
            if (std::dynamic_pointer_cast<Drawable>(target)) {
                subEditor = allocateVertexSubeditor<drawable::IncMeshEditorOneFor<Drawable, project::EditMode::VertexEdit>>();
            } else if (std::dynamic_pointer_cast<Deformable>(target)) {
                subEditor = allocateVertexSubeditor<deformable::IncMeshEditorOneFor<Deformable, project::EditMode::VertexEdit>>();
                if (std::dynamic_pointer_cast<GridDeformer>(target)) compat::ngRequireObject(subEditor)->toolMode = VertexToolMode::Grid;
                if (std::dynamic_pointer_cast<PathDeformer>(target)) compat::ngRequireObject(subEditor)->toolMode = VertexToolMode::BezierDeform;
            } else subEditor = allocateVertexSubeditor<common::mesheditor::operations::node::NodeEditor>(deformOnly);
            compat::ngRequireObject(subEditor)->setTarget(target);
            compat::ngRequireObject(subEditor)->mirrorHoriz = mirrorHoriz;
            compat::ngRequireObject(subEditor)->mirrorVert = mirrorVert;
            compat::ngRequireObject(subEditor)->previewTriangulate = previewTriangulate;
            newEditors[target] = subEditor;
        }
    }
    const auto previous = editors;
    for (const auto& entry : previous) {
        const auto node = entry.first;
        const auto editor = entry.second;
        if (!newEditors.contains(node)) {
            compat::ngRequireObject(editor)->abortToolMode();
        }
    }
    editors = newEditors;
    adjustToolModeForSelection();
}
}
