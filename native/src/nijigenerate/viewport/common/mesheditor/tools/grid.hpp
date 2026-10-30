#pragma once
#include "select.hpp"
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>
#include <nodes/grid_deformer.hpp>
#include <common/resource_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <functional>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::common::mesheditor::tools::grid {
namespace detail {
inline auto sourceEditorKeys(const base::Editors& editors) {
    try {
        return editors.keys();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
class GridTool : public select::NodeSelect {
protected:
    void prepareSourceClassInfo() override {
        select::NodeSelect::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(GridTool),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.tools.grid.GridTool"));
            return true;
        }();
        (void)registered;
    }
public:
    using MeshRef = base::Editor::MeshRef;
    enum class GridActionID : std::int32_t {
        Add = static_cast<std::int32_t>(SelectActionID::End),
        Remove,
        Create,
        TranslateFree,
        TranslateX,
        TranslateY,
        TranslateUp,
        TranslateDown,
        TranslateLeft,
        TranslateRight,
        End,
    };

private:
    struct VirtualMeshContext { MeshRef mesh; };
    using DeformableEditorRef = std::shared_ptr<operations::impl::IncMeshEditorOneDeformable>;
    using DeformableEditorPolicy = nicxlive::core::common::ObjectClassKeyPolicy<DeformableEditorRef>;
    nicxlive::core::common::ResourceKeyMap<DeformableEditorRef, VirtualMeshContext,
        DeformableEditorPolicy::Hash, DeformableEditorPolicy::Equal> deformableMeshes;
    MeshRef createEmptyMesh();
    MeshRef getMesh(EditorRef editor, bool& isVirtual);

public:
    virtual void resetVirtualMeshAsEmpty(EditorRef editor);
    virtual void refreshMeshView(EditorRef editor, MeshRef mesh);
    virtual void markChanged(EditorRef editor) {
        if (const auto deformable = std::dynamic_pointer_cast<operations::impl::IncMeshEditorOneDeformable>(editor)) {
            deformable->vertexMapDirty = true;
        }
    }
    virtual bool applyVirtualMeshToTarget(EditorRef editor);
    // D enum .init is its first member, which is SelectActionID.End, not zero.
    GridActionID currentAction = GridActionID::Add;
    std::int32_t numCut = 3;
    Vec2 dragOrigin{nan(), nan()};
    Vec2 dragEnd{nan(), nan()};
private:
    Vec2 pressOrigin{nan(), nan()};
    std::uint64_t pressVertex = std::numeric_limits<std::uint64_t>::max();
    bool hasPressOrigin = false;
public:
    std::int32_t dragTargetXIndex = 0;
    std::int32_t dragTargetYIndex = 0;
    inline static thread_local float selectRadius = 16.0f;
    // Actual bodies must initialize source out float value to NaN before reads.
    virtual bool isOnGrid(MeshRef mesh, std::int32_t axis, Vec2 mouse, float threshold, float& value);
    virtual bool isOnEdge(MeshRef mesh, std::int32_t axis, Vec2 mouse, float threshold, float& value);
    void setToolMode(VertexToolMode mode, EditorRef editor) override;
    void abortToolMode(EditorRef editor) override {
        const auto keepAlive = weak_from_this().lock();
        if (const auto deformable = std::dynamic_pointer_cast<operations::impl::IncMeshEditorOneDeformable>(editor)) {
            deformableMeshes.erase(deformable);
        }
        isDragging = false;
        currentAction = GridActionID::End;
    }
    bool onDragStart(Vec2 mouse, EditorRef editor) override;
    bool onDragEnd(Vec2 mouse, EditorRef editor) override;
    bool onDragUpdate(Vec2 mouse, EditorRef editor) override;
    virtual bool updateMeshEdit(ImGuiIO* io, EditorRef editor, bool& changed);
    bool update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) override {
        changed = false;
        const auto keepAlive = weak_from_this().lock();
        NodeSelect::update(io, editor, action, changed);
        if (!compat::ngRequireObject(editor)->deformOnly) updateMeshEdit(io, editor, changed);
        return changed;
    }
    void draw(base::CameraRef camera, EditorRef editor) override;
private:
    static float nan() { return std::numeric_limits<float>::quiet_NaN(); }
};

template<class T> requires std::derived_from<T, GridTool>
class ToolInfoImpl : public base::ToolInfoBase<T> {
protected:
    void prepareSourceClassInfo() override {
        base::ToolInfoBase<T>::prepareSourceClassInfo();
        base::detail::prepareSourceTemplateClassRecord<ToolInfoImpl<T>, T>(
            "nijigenerate.viewport.common.mesheditor.tools.grid.ToolInfoImpl!(", ").ToolInfoImpl");
    }
public:
    void setupToolMode(base::EditorRef editor, base::VertexToolMode mode) override {
        const auto keepAlive = this->weak_from_this().lock();
        compat::ngRequireObject(editor)->setToolMode(mode);
        compat::ngRequireObject(editor)->setPath(nullptr);
        compat::ngRequireObject(editor)->deforming = false;
        compat::ngRequireObject(editor)->refreshMesh();
    }
    bool viewportTools(bool deformOnly, base::VertexToolMode currentMode, base::Editors editors) override {
        if (deformOnly) return false;
        bool isDrawable = true;
        for (const auto& key : detail::sourceEditorKeys(editors)) {
            if (!std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(key)) {
                isDrawable = false;
                break;
            }
        }
        bool isGridDeformer = true;
        for (const auto& key : detail::sourceEditorKeys(editors)) {
            if (!std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(key)) {
                isGridDeformer = false;
                break;
            }
        }
        if (isDrawable || isGridDeformer)
            return base::ToolInfoBase<T>::viewportTools(deformOnly, currentMode, editors);
        return false;
    }
    bool canUse(bool deformOnly, compat::Slice<base::NodeRef> targets) override {
        if (deformOnly) return false;
        for (const auto& target : targets) {
            if (!std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(target) &&
                !std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(target)) return false;
        }
        return true;
    }
    base::VertexToolMode mode() override { return base::VertexToolMode::Grid; }
    compat::RetainedString icon() override { return compat::RetainedString::fromStaticLiteral("\xee\x8f\xac"); }
    compat::RetainedString description() override {
        return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Grid Vertex Tool"));
    }
};

} // namespace nijigenerate::viewport::common::mesheditor::tools::grid

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<
    ::nijigenerate::viewport::common::mesheditor::tools::grid::GridTool::GridActionID> {
    using Enum = ::nijigenerate::viewport::common::mesheditor::tools::grid::GridTool::GridActionID;
    inline static constexpr std::string_view name = "GridActionID";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Add, "Add"},
        FormatEnumMember<Enum>{Enum::Remove, "Remove"},
        FormatEnumMember<Enum>{Enum::Create, "Create"},
        FormatEnumMember<Enum>{Enum::TranslateFree, "TranslateFree"},
        FormatEnumMember<Enum>{Enum::TranslateX, "TranslateX"},
        FormatEnumMember<Enum>{Enum::TranslateY, "TranslateY"},
        FormatEnumMember<Enum>{Enum::TranslateUp, "TranslateUp"},
        FormatEnumMember<Enum>{Enum::TranslateDown, "TranslateDown"},
        FormatEnumMember<Enum>{Enum::TranslateLeft, "TranslateLeft"},
        FormatEnumMember<Enum>{Enum::TranslateRight, "TranslateRight"},
        FormatEnumMember<Enum>{Enum::End, "End"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::tools::grid::GridTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.common.mesheditor.tools.grid.GridTool)";
    inline static constexpr char unqualifiedArgument[] = "(GridTool)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.common.mesheditor.tools.grid.GridTool";
    inline static constexpr char unqualifiedElement[] = "GridTool";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::grid::GridTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.tools.grid.GridTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.tools.grid.GridTool)";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::grid::GridTool::GridActionID> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.grid.GridTool.GridActionID)";
    inline static constexpr char unqualifiedArgument[] = "(GridActionID)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.grid.GridTool.GridActionID";
    inline static constexpr char unqualifiedElement[] = "GridActionID";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::grid::GridTool::VirtualMeshContext> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.grid.GridTool.VirtualMeshContext)";
    inline static constexpr char unqualifiedArgument[] = "(VirtualMeshContext)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.grid.GridTool.VirtualMeshContext";
    inline static constexpr char unqualifiedElement[] = "VirtualMeshContext";
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::grid::GridTool>
struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::grid::ToolInfoImpl<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        return {source::sourceToolFamilyText(
                "(nijigenerate.viewport.common.mesheditor.tools.grid.ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText("(ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText(
                "nijigenerate.viewport.common.mesheditor.tools.grid.ToolInfoImpl!", argument, ""),
            source::sourceToolFamilyText("ToolInfoImpl!", argument, "")};
    }
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::grid::GridTool>
struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::grid::ToolInfoImpl<T>> {
    static RetainedString className() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument);
        return source::sourceToolFamilyText(
            "nijigenerate.viewport.common.mesheditor.tools.grid.ToolInfoImpl!", argument, ".ToolInfoImpl");
    }
    static RetainedString templateArgumentName() {
        return SourceTypeSpellings<
            ::nijigenerate::viewport::common::mesheditor::tools::grid::ToolInfoImpl<T>>::get().qualifiedArgument;
    }
};
}
