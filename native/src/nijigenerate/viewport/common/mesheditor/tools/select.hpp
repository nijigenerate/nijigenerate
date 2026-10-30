#pragma once
#include "base.hpp"
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>

namespace nijigenerate::viewport::common::mesheditor::tools::select {

class NodeSelect : public base::Tool, public base::Draggable {
protected:
    void prepareSourceClassInfo() override {
        base::Tool::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(NodeSelect),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.tools.select.NodeSelect"));
            return true;
        }();
        (void)registered;
    }
public:
    using EditorRef = base::EditorRef;
    using Vec2 = base::Vec2;
    using VertexToolMode = base::VertexToolMode;
    bool isDragging = false;
    enum class SelectActionID : std::int32_t {
        None = 0,
        SelectArea = 1,
        ToggleSelect = 2,
        SelectOne = 3,
        MaybeSelectOne = 4,
        SelectMaybeSelectOne = 5,
        StartDrag = 6,
        End = 7,
    };

    void setToolMode(VertexToolMode mode, EditorRef editor) override;
    virtual nicxlive::core::math::Vec3 calculateMousePosIntersection(EditorRef editor, Vec2 mouse);
    std::int32_t peek(ImGuiIO* io, EditorRef editor) override;
    std::int32_t unify(compat::Slice<std::int32_t>) override { return 0; }
    bool update(ImGuiIO*, EditorRef, std::int32_t, bool& changed) override {
        changed = false;
        return false;
    }
    bool onDragStart(Vec2, EditorRef editor) override {
        const auto keepAlive = shared_from_this();
        editor = compat::ngRequireObject(editor);
        if (!editor->isSelecting) {
            isDragging = true;
            editor->getDeformAction();
            return true;
        }
        return false;
    }
    bool onDragEnd(Vec2, EditorRef editor) override {
        const auto keepAlive = shared_from_this();
        isDragging = false;
        editor = compat::ngRequireObject(editor);
        if (editor->isSelecting) {
            editor->updateSelectionPreview(&ImGui::GetIO());
            if (editor->mutateSelection) {
                const auto proposed = editor->newSelected;
                if (!editor->invertSelection) {
                    for (auto vertex : proposed) {
                        if (find(editor->selected, vertex) == editor->selected.size())
                            editor->selected.append(vertex);
                    }
                } else {
                    for (auto vertex : proposed) {
                        const auto index = find(editor->selected, vertex);
                        if (index != editor->selected.size()) {
                            for (std::size_t i = index + 1; i < editor->selected.size(); ++i)
                                editor->selected[i - 1] = editor->selected[i];
                            editor->selected = editor->selected.slice(0, editor->selected.size() - 1);
                        }
                    }
                }
                editor->updateMirrorSelected();
                editor->newSelected.resize(0);
            } else {
                editor->selected = editor->newSelected;
                editor->newSelected = {};
                editor->updateMirrorSelected();
            }
            editor->isSelecting = false;
        }
        editor->pushDeformAction();
        return true;
    }
    bool onDragUpdate(Vec2 mouse, EditorRef editor) override;
    void draw(base::CameraRef camera, EditorRef editor) override;
    void finalizeToolMode(EditorRef) override {}

private:
    static std::size_t find(const base::Editor::Indices& selection, std::uint64_t index) {
        for (std::size_t i = 0; i < selection.size(); ++i) if (selection[i] == index) return i;
        return selection.size();
    }
};

} // namespace nijigenerate::viewport::common::mesheditor::tools::select

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<
    ::nijigenerate::viewport::common::mesheditor::tools::select::NodeSelect::SelectActionID> {
    using Enum = ::nijigenerate::viewport::common::mesheditor::tools::select::NodeSelect::SelectActionID;
    inline static constexpr std::string_view name = "SelectActionID";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::None, "None"},
        FormatEnumMember<Enum>{Enum::SelectArea, "SelectArea"},
        FormatEnumMember<Enum>{Enum::ToggleSelect, "ToggleSelect"},
        FormatEnumMember<Enum>{Enum::SelectOne, "SelectOne"},
        FormatEnumMember<Enum>{Enum::MaybeSelectOne, "MaybeSelectOne"},
        FormatEnumMember<Enum>{Enum::SelectMaybeSelectOne, "SelectMaybeSelectOne"},
        FormatEnumMember<Enum>{Enum::StartDrag, "StartDrag"},
        FormatEnumMember<Enum>{Enum::End, "End"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::select::NodeSelect::SelectActionID> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.select.NodeSelect.SelectActionID)";
    inline static constexpr char unqualifiedArgument[] = "(SelectActionID)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.select.NodeSelect.SelectActionID";
    inline static constexpr char unqualifiedElement[] = "SelectActionID";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::tools::select::NodeSelect> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.common.mesheditor.tools.select.NodeSelect)";
    inline static constexpr char unqualifiedArgument[] = "(NodeSelect)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.common.mesheditor.tools.select.NodeSelect";
    inline static constexpr char unqualifiedElement[] = "NodeSelect";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::select::NodeSelect> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.tools.select.NodeSelect";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.tools.select.NodeSelect)";
};
}
