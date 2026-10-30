#pragma once
#include "select.hpp"
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>
#include <nodes/curve.hpp>
#include <nodes/path_deformer.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <common/resource_key_map.hpp>
#include <functional>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <optional>
#include <vector>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::viewport::common::mesheditor::tools::bezierdeform {
namespace detail {
template<class Operation>
decltype(auto) allocateBezierStorage(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
class BezierDeformTool : public select::NodeSelect {
protected:
    void prepareSourceClassInfo() override {
        select::NodeSelect::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(BezierDeformTool),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.tools.bezierdeform.BezierDeformTool"));
            return true;
        }();
        (void)registered;
    }
public:
    std::shared_ptr<actions::Action> action;
    void releasePendingActions() override { action.reset(); }
    std::uint64_t lockedPoint = std::numeric_limits<std::uint64_t>::max();
    enum class BezierDeformActionID : std::int32_t {
        SwitchMode = static_cast<std::int32_t>(SelectActionID::End),
        RemovePoint,
        AddPoint,
        TranslatePoint,
        StartTransform,
        StartShiftTransform,
        Transform,
        Rotate,
        SetRotateCenter,
        UnsetRotateCenter,
        Shift,
        End,
    };
    bool _isShiftMode = false;
    bool _isRotateMode = false;
    bool preDragging = false;

    void setToolMode(VertexToolMode mode, EditorRef editor) override {
        lockedPoint = std::numeric_limits<std::uint64_t>::max();
        _isRotateMode = false;
        _isShiftMode = false;
        NodeSelect::setToolMode(mode, editor);
    }
    virtual bool getIsShiftMode() { return _isShiftMode; }
    virtual void setIsShiftMode(bool value) { _isShiftMode = value; }
    virtual bool getIsRotateMode() { return _isRotateMode; }
    virtual void setIsRotateMode(bool value) { _isRotateMode = value; }

    std::optional<std::shared_ptr<nicxlive::core::nodes::Curve>> origCurve;
    compat::Vec2u origCurvePoint{};
private:
    std::shared_ptr<nicxlive::core::nodes::Curve> originalCurve() const {
        if(!origCurve)throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Called `get' on null Nullable!Curve."),
            compat::RetainedString::fromStaticLiteral("std/typecons.d"),4035);
        return *origCurve;
    }
public:
    bool onDragStart(Vec2 mouse, EditorRef editor) override;
    bool onDragEnd(Vec2 mouse, EditorRef editor) override;
    bool onDragUpdate(Vec2 mouse, EditorRef editor) override;
    virtual std::int32_t peekVertexEdit(ImGuiIO* io, EditorRef editor);
    virtual std::int32_t peekDeformEdit(ImGuiIO* io, EditorRef editor);
    std::int32_t peek(ImGuiIO* io, EditorRef editor) override {
        const auto keepAlive = weak_from_this().lock();
        const auto result = NodeSelect::peek(io, editor);
        if (result != static_cast<std::int32_t>(SelectActionID::None)) return result;
        if (compat::ngRequireObject(editor)->deformOnly) return peekDeformEdit(io, editor);
        return peekVertexEdit(io, editor);
    }
    std::int32_t unify(compat::Slice<std::int32_t> actions) override {
        struct PriorityHash {
            std::size_t operator()(std::int32_t value) const noexcept { return static_cast<std::size_t>(value); }
        };
        nicxlive::core::common::ResourceKeyMap<std::int32_t, std::int32_t,
            PriorityHash, std::equal_to<std::int32_t>> priorities;
        priorities[static_cast<std::int32_t>(BezierDeformActionID::SwitchMode)]=2;
        priorities[static_cast<std::int32_t>(BezierDeformActionID::RemovePoint)]=1;
        priorities[static_cast<std::int32_t>(BezierDeformActionID::AddPoint)]=1;
        priorities[static_cast<std::int32_t>(BezierDeformActionID::TranslatePoint)]=0;
        priorities[static_cast<std::int32_t>(BezierDeformActionID::StartTransform)]=0;
        priorities[static_cast<std::int32_t>(BezierDeformActionID::StartShiftTransform)]=0;
        priorities[static_cast<std::int32_t>(BezierDeformActionID::Shift)]=0;
        priorities[static_cast<std::int32_t>(BezierDeformActionID::Transform)]=0;
        priorities[static_cast<std::int32_t>(BezierDeformActionID::Rotate)]=0;
        priorities[static_cast<std::int32_t>(BezierDeformActionID::SetRotateCenter)]=0;
        priorities[static_cast<std::int32_t>(BezierDeformActionID::UnsetRotateCenter)]=0;
        priorities[static_cast<std::int32_t>(SelectActionID::None)]=10;
        priorities[static_cast<std::int32_t>(SelectActionID::SelectArea)]=5;
        priorities[static_cast<std::int32_t>(SelectActionID::ToggleSelect)]=2;
        priorities[static_cast<std::int32_t>(SelectActionID::SelectOne)]=2;
        priorities[static_cast<std::int32_t>(SelectActionID::MaybeSelectOne)]=2;
        priorities[static_cast<std::int32_t>(SelectActionID::StartDrag)]=2;
        priorities[static_cast<std::int32_t>(SelectActionID::SelectMaybeSelectOne)]=2;
        auto selected = static_cast<std::int32_t>(SelectActionID::None);
        auto priority = priorities.at(selected);
        for (auto candidate : actions) {
            const auto found = priorities.find(candidate);
            if (found == priorities.end()) throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/bezierdeform.d"),291);
            const auto next = found->second;
            if (next < priority) {
                priority = next;
                selected = candidate;
            }
        }
        return selected;
    }
    virtual bool updateVertexEdit(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed);
    virtual bool updateDeformEdit(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed);
    bool update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) override {
        changed = false;
        const auto keepAlive = weak_from_this().lock();
        NodeSelect::update(io, editor, action, changed);
        if (compat::ngRequireObject(editor)->deformOnly) updateDeformEdit(io, editor, action, changed);
        else updateVertexEdit(io, editor, action, changed);
        return changed;
    }
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

template<class T> requires std::derived_from<T, BezierDeformTool>
class ToolInfoImpl : public base::ToolInfoBase<T> {
protected:
    void prepareSourceClassInfo() override {
        base::ToolInfoBase<T>::prepareSourceClassInfo();
        base::detail::prepareSourceTemplateClassRecord<ToolInfoImpl<T>, T>(
            "nijigenerate.viewport.common.mesheditor.tools.bezierdeform.ToolInfoImpl!(", ").ToolInfoImpl");
    }
public:
    void setupToolMode(base::EditorRef editor, base::VertexToolMode mode) override {
        const auto keepAlive = this->weak_from_this().lock();
        compat::ngRequireObject(editor)->setToolMode(mode);
        compat::ngRequireObject(editor)->deforming = false;
        compat::ngRequireObject(editor)->refreshMesh();
    }
    bool viewportTools(bool deformOnly, base::VertexToolMode currentMode, base::Editors editors) override {
        bool isDeformer = true;
        {
            const auto keys = detail::allocateBezierStorage([&] { return editors.keys(); });
            for (const auto& key : keys) {
                if (!std::dynamic_pointer_cast<nicxlive::core::nodes::PathDeformer>(key)) {
                    isDeformer = false;
                    break;
                }
            }
        }
        if (isDeformer) return base::ToolInfoBase<T>::viewportTools(deformOnly, currentMode, editors);
        return false;
    }
    bool canUse(bool, compat::Slice<base::NodeRef> targets) override {
        for (const auto& target : targets) {
            if (!std::dynamic_pointer_cast<nicxlive::core::nodes::PathDeformer>(target)) return false;
        }
        return true;
    }
    bool displayToolOptions(bool, base::VertexToolMode, base::Editors editors) override {
        const auto keepAlive = this->weak_from_this().lock();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
        std::shared_ptr<T> deformTool;
        if(!editors.empty()) {
            const auto values = detail::allocateBezierStorage([&] { return editors.values(); });
            deformTool=std::dynamic_pointer_cast<T>(compat::ngRequireObject(values[0])->getTool());
        }
        ImGui::BeginGroup();
        if (widgets::ngButtonColored("\xee\xa8\xb7", ImVec2(0.0f, 0.0f),
            deformTool && deformTool->getIsRotateMode() ? widgets::colorUndefined : ImVec4(0.6f, 0.6f, 0.6f, 1.0f))) {
            if (!editors.isNull()) {
                for (auto [key, editor] : editors) {
                    (void)key;
                    auto deform = std::dynamic_pointer_cast<T>(compat::ngRequireObject(editor)->getTool());
                    if (deform) deform->setIsRotateMode(!deform->getIsRotateMode());
                }
            }
        }
        widgets::ngTooltip(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Set rotation center")));
        ImGui::EndGroup();
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::BeginGroup();
        if (widgets::ngButtonColored("\xee\xa3\xa4", ImVec2(0.0f, 0.0f),
            deformTool && deformTool->getIsShiftMode() ? widgets::colorUndefined : ImVec4(0.6f, 0.6f, 0.6f, 1.0f))) {
            if (!editors.isNull()) {
                for (auto [key, editor] : editors) {
                    (void)key;
                    auto deform = std::dynamic_pointer_cast<T>(compat::ngRequireObject(editor)->getTool());
                    if (deform) deform->setIsShiftMode(!deform->getIsShiftMode());
                }
            }
        }
        widgets::ngTooltip(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Move points along the path")));
        ImGui::EndGroup();
        ImGui::PopStyleVar(2);
        return false;
    }
    base::VertexToolMode mode() override { return base::VertexToolMode::BezierDeform; }
    compat::RetainedString icon() override { return compat::RetainedString::fromStaticLiteral("\xee\xab\x8d"); }
    compat::RetainedString description() override {
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Path Deform Tool"));
    }
};

} // namespace nijigenerate::viewport::common::mesheditor::tools::bezierdeform

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<
    ::nijigenerate::viewport::common::mesheditor::tools::bezierdeform::BezierDeformTool::BezierDeformActionID> {
    using Enum = ::nijigenerate::viewport::common::mesheditor::tools::bezierdeform::
        BezierDeformTool::BezierDeformActionID;
    inline static constexpr std::string_view name = "BezierDeformActionID";
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
        FormatEnumMember<Enum>{Enum::Shift, "Shift"},
        FormatEnumMember<Enum>{Enum::End, "End"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::bezierdeform::BezierDeformTool::BezierDeformActionID> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.bezierdeform.BezierDeformTool.BezierDeformActionID)";
    inline static constexpr char unqualifiedArgument[] = "(BezierDeformActionID)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.bezierdeform.BezierDeformTool.BezierDeformActionID";
    inline static constexpr char unqualifiedElement[] = "BezierDeformActionID";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::tools::bezierdeform::BezierDeformTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.common.mesheditor.tools.bezierdeform.BezierDeformTool)";
    inline static constexpr char unqualifiedArgument[] = "(BezierDeformTool)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.common.mesheditor.tools.bezierdeform.BezierDeformTool";
    inline static constexpr char unqualifiedElement[] = "BezierDeformTool";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::bezierdeform::BezierDeformTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.tools.bezierdeform.BezierDeformTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.tools.bezierdeform.BezierDeformTool)";
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::bezierdeform::BezierDeformTool>
struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::bezierdeform::ToolInfoImpl<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        return {source::sourceToolFamilyText(
                "(nijigenerate.viewport.common.mesheditor.tools.bezierdeform.ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText("(ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText(
                "nijigenerate.viewport.common.mesheditor.tools.bezierdeform.ToolInfoImpl!", argument, ""),
            source::sourceToolFamilyText("ToolInfoImpl!", argument, "")};
    }
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::bezierdeform::BezierDeformTool>
struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::bezierdeform::ToolInfoImpl<T>> {
    static RetainedString className() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument);
        return source::sourceToolFamilyText(
            "nijigenerate.viewport.common.mesheditor.tools.bezierdeform.ToolInfoImpl!", argument, ".ToolInfoImpl");
    }
    static RetainedString templateArgumentName() {
        return SourceTypeSpellings<
            ::nijigenerate::viewport::common::mesheditor::tools::bezierdeform::ToolInfoImpl<T>>::get().qualifiedArgument;
    }
};
}
