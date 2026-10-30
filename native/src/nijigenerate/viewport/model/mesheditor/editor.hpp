#pragma once
#include <nijigenerate/viewport/common/mesheditor/package.hpp>
#include "deformable.hpp"
#include "drawable.hpp"
#include "node.hpp"
#include <compat/object_metadata.hpp>

namespace nijigenerate::viewport::model::mesheditor::editor {
using namespace ::nijigenerate::viewport::common::mesheditor;
using namespace ::nijigenerate::viewport::common::mesheditor::operations;

namespace detail {
template<class T, project::EditMode... modes> struct ImportedMeshEditor;

template<class T>
requires (std::derived_from<T, nicxlive::core::nodes::Node> &&
    !std::derived_from<T, nicxlive::core::nodes::Deformable>)
struct ImportedMeshEditor<T> {
    using Type = common::mesheditor::operations::node::NodeEditorImpl<T>;
};

template<class T, project::EditMode mode>
requires (std::derived_from<T, nicxlive::core::nodes::Node> &&
    !std::derived_from<T, nicxlive::core::nodes::Deformable>)
struct ImportedMeshEditor<T, mode> {
    using Type = common::mesheditor::operations::node::NodeEditorImpl<T, mode>;
};

template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
struct ImportedMeshEditor<T, mode> {
    using Type = drawable::IncMeshEditorOneFor<T, mode>;
};

template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit &&
    std::derived_from<T, nicxlive::core::nodes::Deformable> &&
    !std::derived_from<T, nicxlive::core::nodes::Drawable>)
struct ImportedMeshEditor<T, mode> {
    using Type = deformable::IncMeshEditorOneFor<T, mode>;
};
}

// Public imports select the actual producer specialization, with no new class.
template<class T, project::EditMode... modes>
using IncMeshEditorOneFor = typename detail::ImportedMeshEditor<T, modes...>::Type;

class DeformMeshEditor : public common::mesheditor::IncMeshEditor {
    void adjustToolModeForSelection();
public:
    DeformMeshEditor() : IncMeshEditor(true) {}
    void addTarget(NodeRef target) override;
    void setTargets(Nodes targets) override;
private:
    void prepareSourceClassInfo() override {
        IncMeshEditor::prepareSourceClassInfo();
        registerSourceClassInfo<DeformMeshEditor>(
            "nijigenerate.viewport.model.mesheditor.editor.DeformMeshEditor");
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::model::mesheditor::editor::DeformMeshEditor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.model.mesheditor.editor.DeformMeshEditor)";
    inline static constexpr char unqualifiedArgument[] = "(DeformMeshEditor)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.model.mesheditor.editor.DeformMeshEditor";
    inline static constexpr char unqualifiedElement[] = "DeformMeshEditor";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::model::mesheditor::editor::DeformMeshEditor> {
    inline static constexpr char className[] = "nijigenerate.viewport.model.mesheditor.editor.DeformMeshEditor";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.model.mesheditor.editor.DeformMeshEditor)";
};
} // namespace nicxlive::core::d_runtime
