#pragma once
#include <compat/source_allocator.hpp>
#include "select.hpp"
#include <nijigenerate/viewport/common/spline.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nodes/drawable.hpp>
#include <nodes/deformable.hpp>
#include <concepts>
#include <array>
#include <cstdint>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::viewport::common::mesheditor::tools::pathdeform {
namespace detail {
inline auto sourceEditorValues(const base::Editors& editors) {
    try {
        return editors.values();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
class PathDeformTool : public select::NodeSelect {
protected:
    void prepareSourceClassInfo() override {
        select::NodeSelect::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(PathDeformTool),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.tools.pathdeform.PathDeformTool"));
            return true;
        }();
        (void)registered;
    }
public:
    PathDeformTool() = default;
    PathDeformTool(const PathDeformTool&) = delete;
    PathDeformTool& operator=(const PathDeformTool&) = delete;
    using PathRef = std::shared_ptr<spline::CatmullSpline>;
    PathRef path;
    std::uint32_t pathDragTarget = 0, lockedPoint = 0;
    enum class Mode : std::int32_t { Define, Transform };
    enum class PathDeformActionID : std::int32_t {
        SwitchMode = static_cast<std::int32_t>(SelectActionID::End), RemovePoint, AddPoint, TranslatePoint,
        StartTransform, StartShiftTransform, Transform, Rotate, SetRotateCenter, UnsetRotateCenter, Shift
    };
    Mode mode = Mode::Define, prevMode = Mode::Define;
    bool _isShiftMode = false, _isRotateMode = false;
    void setToolMode(VertexToolMode value, EditorRef editor) override {
        const auto keepAlive = shared_from_this();
        pathDragTarget = lockedPoint = UINT32_MAX;
        NodeSelect::setToolMode(value, editor);
    }
    virtual void setPath(PathRef value) {
        const auto keepAlive = weak_from_this().lock();
        if (!value || !spline::ngCatmullSplineObjectEquals(path, value)) {
            path = std::move(value);
            pathDragTarget = lockedPoint = UINT32_MAX;
        }
    }
    virtual Mode getMode() { return mode; }
    virtual void setMode(Mode value) { mode = value; }
    virtual bool getIsShiftMode() { return _isShiftMode; }
    virtual void setIsShiftMode(bool value) { _isShiftMode = value; }
    virtual bool getIsRotateMode() { return _isRotateMode; }
    virtual void setIsRotateMode(bool value) { _isRotateMode = value; }
    std::int32_t peek(ImGuiIO* io, EditorRef editor) override;
    std::int32_t unify(compat::Slice<std::int32_t> actions) override;
    bool update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) override;
    void draw(base::CameraRef camera, EditorRef editor) override;
    base::MeshEditorActionRef<base::DeformationAction> editorAction(
        base::NodeRef target, std::shared_ptr<base::DeformationAction> action) override;
    base::MeshEditorActionRef<actions::GroupAction> editorAction(
        base::NodeRef target, std::shared_ptr<actions::GroupAction> action) override;
    base::MeshEditorActionRef<base::DeformationAction> editorAction(
        base::DrawableRef target, std::shared_ptr<base::DeformationAction> action) override;
    base::MeshEditorActionRef<actions::GroupAction> editorAction(
        base::DrawableRef target, std::shared_ptr<actions::GroupAction> action) override;
};

template<class T> requires std::derived_from<T, PathDeformTool>
class ToolInfoImpl : public base::ToolInfoBase<T> {
protected:
    void prepareSourceClassInfo() override {
        base::ToolInfoBase<T>::prepareSourceClassInfo();
        base::detail::prepareSourceTemplateClassRecord<ToolInfoImpl<T>, T>(
            "nijigenerate.viewport.common.mesheditor.tools.pathdeform.ToolInfoImpl!(", ").ToolInfoImpl");
    }
public:
    void setupToolMode(base::EditorRef editor, base::VertexToolMode mode) override {
        const auto keepAlive = this->shared_from_this();
        compat::ngRequireObject(editor)->setToolMode(mode);
        compat::ngRequireObject(editor)->setPath(std::allocate_shared<spline::CatmullSpline>(
            nicxlive::core::d_runtime::SourceAllocator<spline::CatmullSpline>{}));
        compat::ngRequireObject(editor)->deforming = false;
        compat::ngRequireObject(editor)->refreshMesh();
    }
    // The source deliberately hides this tool while retaining mode registration.
    bool viewportTools(bool, base::VertexToolMode, base::Editors) override { return false; }
    bool canUse(bool deformOnly, compat::Slice<base::NodeRef> targets) override {
        if (!deformOnly) return false;
        for (auto target : targets) if (!std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(target) &&
            std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(target)) return false;
        return true;
    }
    bool displayToolOptions(bool, base::VertexToolMode, base::Editors editors) override {
        const auto keepAlive = this->shared_from_this();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        const auto tool = [&] {
            if (editors.empty()) return std::shared_ptr<PathDeformTool>{};
            const auto values = detail::sourceEditorValues(editors);
            return std::dynamic_pointer_cast<PathDeformTool>(compat::ngRequireObject(values[0])->getTool());
        }();
        const auto each = [&](auto callback) {
            for (auto [node, editor] : editors)
                if (auto receiver = std::dynamic_pointer_cast<PathDeformTool>(compat::ngRequireObject(editor)->getTool())) callback(receiver);
        };
        const auto color = [](bool selected) { return selected ? widgets::colorUndefined : ImVec4(0.6f, 0.6f, 0.6f, 1); };
        ImGui::BeginGroup();
        if (widgets::ngButtonColored("\xee\x9d\x86", ImVec2(0, 0), color(tool && tool->mode == PathDeformTool::Mode::Define)))
            each([](auto receiver) { compat::ngRequireObject(receiver)->setMode(PathDeformTool::Mode::Define); });
        widgets::ngTooltip(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Define paths")));
        ImGui::SameLine(0, 0);
        if (widgets::ngButtonColored("\xee\xa7\xac", ImVec2(0, 0), color(tool && tool->mode == PathDeformTool::Mode::Transform)))
            each([](auto receiver) { compat::ngRequireObject(receiver)->setMode(PathDeformTool::Mode::Transform); });
        widgets::ngTooltip(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Transform path")));
        ImGui::EndGroup(); ImGui::SameLine(0, 4); ImGui::BeginGroup();
        if (widgets::ngButtonColored("\xee\xa8\xb7", ImVec2(0, 0), color(tool && tool->getIsRotateMode())))
            each([](auto receiver) { compat::ngRequireObject(receiver)->setIsRotateMode(!compat::ngRequireObject(receiver)->getIsRotateMode()); });
        widgets::ngTooltip(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Set rotation center")));
        ImGui::EndGroup(); ImGui::SameLine(0, 4); ImGui::BeginGroup();
        if (widgets::ngButtonColored("\xee\xab\x8d", ImVec2(0, 0), color(tool && tool->getIsShiftMode())))
            each([](auto receiver) { compat::ngRequireObject(receiver)->setIsShiftMode(!compat::ngRequireObject(receiver)->getIsShiftMode()); });
        widgets::ngTooltip(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Move points along the path")));
        ImGui::EndGroup(); ImGui::PopStyleVar(2);
        return false;
    }
    base::VertexToolMode mode() override { return base::VertexToolMode::PathDeform; }
    compat::RetainedString icon() override { return compat::RetainedString::fromStaticLiteral("\xee\xab\x8d"); }
    compat::RetainedString description() override {
        return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Path Deform Tool"));
    }
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<
    ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::PathDeformTool::Mode> {
    using Enum = ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::PathDeformTool::Mode;
    inline static constexpr std::string_view name = "Mode";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Define, "Define"},
        FormatEnumMember<Enum>{Enum::Transform, "Transform"}};
};
template<> struct FormatEnumTraits<
    ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::PathDeformTool::PathDeformActionID> {
    using Enum = ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::PathDeformTool::PathDeformActionID;
    inline static constexpr std::string_view name = "PathDeformActionID";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::SwitchMode, "SwitchMode"},
        FormatEnumMember<Enum>{Enum::RemovePoint, "RemovePoint"},
        FormatEnumMember<Enum>{Enum::AddPoint, "AddPoint"},
        FormatEnumMember<Enum>{Enum::TranslatePoint, "TranslatePoint"},
        FormatEnumMember<Enum>{Enum::StartTransform, "StartTransform"},
        FormatEnumMember<Enum>{Enum::StartShiftTransform, "StartShiftTransform"},
        FormatEnumMember<Enum>{Enum::Transform, "Transform"},
        FormatEnumMember<Enum>{Enum::Rotate, "Rotate"},
        FormatEnumMember<Enum>{Enum::SetRotateCenter, "SetRotateCenter"},
        FormatEnumMember<Enum>{Enum::UnsetRotateCenter, "UnsetRotateCenter"},
        FormatEnumMember<Enum>{Enum::Shift, "Shift"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::tools::pathdeform::PathDeformTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.common.mesheditor.tools.pathdeform.PathDeformTool)";
    inline static constexpr char unqualifiedArgument[] = "(PathDeformTool)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.common.mesheditor.tools.pathdeform.PathDeformTool";
    inline static constexpr char unqualifiedElement[] = "PathDeformTool";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::PathDeformTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.tools.pathdeform.PathDeformTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.tools.pathdeform.PathDeformTool)";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::PathDeformTool::Mode> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.pathdeform.PathDeformTool.Mode)";
    inline static constexpr char unqualifiedArgument[] = "(Mode)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.pathdeform.PathDeformTool.Mode";
    inline static constexpr char unqualifiedElement[] = "Mode";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::PathDeformTool::PathDeformActionID> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.pathdeform.PathDeformTool.PathDeformActionID)";
    inline static constexpr char unqualifiedArgument[] = "(PathDeformActionID)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.pathdeform.PathDeformTool.PathDeformActionID";
    inline static constexpr char unqualifiedElement[] = "PathDeformActionID";
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::PathDeformTool>
struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::ToolInfoImpl<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        return {source::sourceToolFamilyText(
                "(nijigenerate.viewport.common.mesheditor.tools.pathdeform.ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText("(ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText(
                "nijigenerate.viewport.common.mesheditor.tools.pathdeform.ToolInfoImpl!", argument, ""),
            source::sourceToolFamilyText("ToolInfoImpl!", argument, "")};
    }
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::PathDeformTool>
struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::ToolInfoImpl<T>> {
    static RetainedString className() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument);
        return source::sourceToolFamilyText(
            "nijigenerate.viewport.common.mesheditor.tools.pathdeform.ToolInfoImpl!", argument, ".ToolInfoImpl");
    }
    static RetainedString templateArgumentName() {
        return SourceTypeSpellings<
            ::nijigenerate::viewport::common::mesheditor::tools::pathdeform::ToolInfoImpl<T>>::get().qualifiedArgument;
    }
};
}
