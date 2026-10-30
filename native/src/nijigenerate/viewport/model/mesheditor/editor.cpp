#include "editor.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <compat/source_allocator.hpp>
#include "drawable.hpp"
#include "deformable.hpp"
#include <nijigenerate/viewport/common/mesheditor/operations/node.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <utility>

namespace nijigenerate::viewport::model::mesheditor::editor {
using namespace nicxlive::core::nodes;
namespace {
template<class T, class... Args> std::shared_ptr<T> allocateModelSubeditor(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(args)...);
}
}

void DeformMeshEditor::adjustToolModeForSelection() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    bool anyGrid = false, anyPath = false, anyOther = false;
    const auto selectedNodes = editors.keys();
    for (const auto node : selectedNodes) {
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

void DeformMeshEditor::addTarget(NodeRef target) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (editors.contains(target)) return;
    EditorRef subEditor;
    if (std::dynamic_pointer_cast<Drawable>(target)) {
        subEditor = allocateModelSubeditor<drawable::IncMeshEditorOneFor<Drawable, project::EditMode::ModelEdit>>();
    } else if (std::dynamic_pointer_cast<Deformable>(target)) {
        subEditor = deformable::ngCreateModelDeformableEditor(target);
        if (std::dynamic_pointer_cast<PathDeformer>(target)) subEditor->toolMode = VertexToolMode::BezierDeform;
        else if (std::dynamic_pointer_cast<GridDeformer>(target)) subEditor->toolMode = VertexToolMode::Grid;
    }
    compat::ngRequireObject(subEditor)->setTarget(target);
    subEditor->mirrorHoriz = mirrorHoriz;
    subEditor->mirrorVert = mirrorVert;
    subEditor->previewTriangulate = previewTriangulate;
    editors[target] = subEditor;
    adjustToolModeForSelection();
}

void DeformMeshEditor::setTargets(Nodes targets) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    common::mesheditor::tools::base::Editors newEditors;
    for (const auto target : targets) {
        if (editors.contains(target)) {
            const auto found = editors.find(target);
            if (found == editors.end()) throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/model/mesheditor/editor.d"), 106);
            const auto existing = found->second;
            newEditors[target] = existing;
        } else {
            EditorRef subEditor;
            if (std::dynamic_pointer_cast<Drawable>(target)) {
                subEditor = allocateModelSubeditor<drawable::IncMeshEditorOneFor<Drawable, project::EditMode::ModelEdit>>();
            } else if (std::dynamic_pointer_cast<Deformable>(target)) {
                subEditor = deformable::ngCreateModelDeformableEditor(target);
                if (std::dynamic_pointer_cast<PathDeformer>(target)) subEditor->toolMode = VertexToolMode::BezierDeform;
            } else {
                subEditor = allocateModelSubeditor<common::mesheditor::operations::node::NodeEditor>(deformOnly);
            }
            compat::ngRequireObject(subEditor)->setTarget(target);
            subEditor->mirrorHoriz = mirrorHoriz;
            subEditor->mirrorVert = mirrorVert;
            subEditor->previewTriangulate = previewTriangulate;
            newEditors[target] = subEditor;
        }
    }
    const auto previous = editors;
    for (const auto [node, editor] : previous) {
        if (!newEditors.contains(node)) compat::ngRequireObject(editor)->abortToolMode();
    }
    editors = std::move(newEditors);
}
}
