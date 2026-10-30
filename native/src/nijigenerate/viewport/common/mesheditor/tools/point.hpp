#pragma once
#include "select.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>
#include <concepts>
#include <cstdint>

namespace nijigenerate::viewport::common::mesheditor::tools::point {
class PointTool : public select::NodeSelect {
protected:
    void prepareSourceClassInfo() override {
        select::NodeSelect::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(PointTool),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.tools.point.PointTool"));
            return true;
        }();
        (void)registered;
    }
public:
    PointTool() = default;
    PointTool(const PointTool&) = delete;
    PointTool& operator=(const PointTool&) = delete;
    actions::ActionPtr action;
    void releasePendingActions() override { action.reset(); }
    inline static thread_local bool autoConnect = false;
    enum PointActionID : std::int32_t {
        Add = static_cast<std::int32_t>(SelectActionID::End), Remove, Translate,
        TranslateUp, TranslateDown, TranslateLeft, TranslateRight, End
    };
    bool onDragStart(Vec2 mouse, EditorRef editor) override;
    bool onDragEnd(Vec2 mouse, EditorRef editor) override;
    bool onDragUpdate(Vec2 mouse, EditorRef editor) override;
    virtual bool updateMeshEdit(ImGuiIO* io, EditorRef editor, bool& changed);
    virtual std::int32_t peekDeformEdit(ImGuiIO* io, EditorRef editor);
    virtual bool updateDeformEdit(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed);
    std::int32_t peek(ImGuiIO* io, EditorRef editor) override;
    std::int32_t unify(compat::Slice<std::int32_t> actions) override;
    bool update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) override;
    virtual bool isAutoConnect() { return autoConnect; }
    virtual void setAutoConnect(bool value) { autoConnect = value; }
};
namespace detail { bool displayOptions(bool deformOnly, base::Editors editors); }
template<class T> requires std::derived_from<T, PointTool>
class ToolInfoImpl : public base::ToolInfoBase<T> {
protected:
    void prepareSourceClassInfo() override {
        base::ToolInfoBase<T>::prepareSourceClassInfo();
        base::detail::prepareSourceTemplateClassRecord<ToolInfoImpl<T>, T>(
            "nijigenerate.viewport.common.mesheditor.tools.point.ToolInfoImpl!(", ").ToolInfoImpl");
    }
public:
    base::VertexToolMode mode() override { return base::VertexToolMode::Points; }
    compat::RetainedString icon() override { return compat::RetainedString::fromStaticLiteral("\xef\x82\x97"); }
    compat::RetainedString description() override {
        return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Vertex Tool"));
    }
    bool displayToolOptions(bool deformOnly, base::VertexToolMode, base::Editors editors) override {
        const auto keepAlive = this->shared_from_this();
        return detail::displayOptions(deformOnly, std::move(editors));
    }
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<
    ::nijigenerate::viewport::common::mesheditor::tools::point::PointTool::PointActionID> {
    using Enum = ::nijigenerate::viewport::common::mesheditor::tools::point::PointTool::PointActionID;
    inline static constexpr std::string_view name = "PointActionID";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Add, "Add"},
        FormatEnumMember<Enum>{Enum::Remove, "Remove"},
        FormatEnumMember<Enum>{Enum::Translate, "Translate"},
        FormatEnumMember<Enum>{Enum::TranslateUp, "TranslateUp"},
        FormatEnumMember<Enum>{Enum::TranslateDown, "TranslateDown"},
        FormatEnumMember<Enum>{Enum::TranslateLeft, "TranslateLeft"},
        FormatEnumMember<Enum>{Enum::TranslateRight, "TranslateRight"},
        FormatEnumMember<Enum>{Enum::End, "End"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::point::PointTool::PointActionID> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.point.PointTool.PointActionID)";
    inline static constexpr char unqualifiedArgument[] = "(PointActionID)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.point.PointTool.PointActionID";
    inline static constexpr char unqualifiedElement[] = "PointActionID";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::tools::point::PointTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.common.mesheditor.tools.point.PointTool)";
    inline static constexpr char unqualifiedArgument[] = "(PointTool)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.common.mesheditor.tools.point.PointTool";
    inline static constexpr char unqualifiedElement[] = "PointTool";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::point::PointTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.tools.point.PointTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.tools.point.PointTool)";
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::point::PointTool>
struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::point::ToolInfoImpl<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        return {source::sourceToolFamilyText(
                "(nijigenerate.viewport.common.mesheditor.tools.point.ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText("(ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText(
                "nijigenerate.viewport.common.mesheditor.tools.point.ToolInfoImpl!", argument, ""),
            source::sourceToolFamilyText("ToolInfoImpl!", argument, "")};
    }
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::point::PointTool>
struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::point::ToolInfoImpl<T>> {
    static RetainedString className() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument);
        return source::sourceToolFamilyText(
            "nijigenerate.viewport.common.mesheditor.tools.point.ToolInfoImpl!", argument, ".ToolInfoImpl");
    }
    static RetainedString templateArgumentName() {
        return SourceTypeSpellings<
            ::nijigenerate::viewport::common::mesheditor::tools::point::ToolInfoImpl<T>>::get().qualifiedArgument;
    }
};
}
