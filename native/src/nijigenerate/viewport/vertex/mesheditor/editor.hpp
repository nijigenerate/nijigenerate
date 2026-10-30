#pragma once
#include <nijigenerate/viewport/common/mesheditor/package.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/enums.hpp>
#include "deformable.hpp"
#include "drawable.hpp"
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::viewport::vertex::mesheditor::editor {
using namespace ::nijigenerate::viewport::common::mesheditor;
using namespace ::nijigenerate::viewport::common::mesheditor::operations;
using ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode;

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
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
struct ImportedMeshEditor<T, mode> {
    using Type = drawable::IncMeshEditorOneFor<T, mode>;
};

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Deformable> &&
    !std::derived_from<T, nicxlive::core::nodes::Drawable>)
struct ImportedMeshEditor<T, mode> {
    using Type = deformable::IncMeshEditorOneFor<T, mode>;
};
}

template<class T, project::EditMode... modes>
using IncMeshEditorOneFor = typename detail::ImportedMeshEditor<T, modes...>::Type;

class VertexMeshEditor : public common::mesheditor::IncMeshEditor {
    std::shared_ptr<core::actionstack::ActionStackScope> actionScope;
    struct SourceAllocationTag {};
    explicit VertexMeshEditor(SourceAllocationTag) : IncMeshEditor(false) {}
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource();
    void adjustToolModeForSelection();
public:
    VertexMeshEditor();
    static std::shared_ptr<VertexMeshEditor> createSource() {
        auto result = std::allocate_shared<VertexMeshEditor>(
            nicxlive::core::d_runtime::SourceAllocator<VertexMeshEditor>{}, SourceAllocationTag{});
        result->initializeSource();
        return result;
    }
    VertexMeshEditor(const VertexMeshEditor&) = delete;
    VertexMeshEditor& operator=(const VertexMeshEditor&) = delete;
    ~VertexMeshEditor() override;
    virtual void closeScope();
    void addTarget(NodeRef target) override;
    void setTargets(Nodes targets) override;
private:
    void prepareSourceClassInfo() override {
        IncMeshEditor::prepareSourceClassInfo();
        registerSourceClassInfo<VertexMeshEditor>(
            "nijigenerate.viewport.vertex.mesheditor.editor.VertexMeshEditor");
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::mesheditor::editor::VertexMeshEditor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.mesheditor.editor.VertexMeshEditor)";
    inline static constexpr char unqualifiedArgument[] = "(VertexMeshEditor)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.mesheditor.editor.VertexMeshEditor";
    inline static constexpr char unqualifiedElement[] = "VertexMeshEditor";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::vertex::mesheditor::editor::VertexMeshEditor> {
    inline static constexpr char className[] = "nijigenerate.viewport.vertex.mesheditor.editor.VertexMeshEditor";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.vertex.mesheditor.editor.VertexMeshEditor)";
};
} // namespace nicxlive::core::d_runtime
