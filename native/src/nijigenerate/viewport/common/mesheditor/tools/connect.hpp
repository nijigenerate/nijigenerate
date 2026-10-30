#pragma once
#include "select.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nodes/drawable.hpp>
#include <concepts>
#include <cstdint>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::common::mesheditor::tools::connect {
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
class ConnectTool : public select::NodeSelect {
protected:
    void prepareSourceClassInfo() override {
        select::NodeSelect::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ConnectTool),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.tools.connect.ConnectTool"));
            return true;
        }();
        (void)registered;
    }
public:
    ConnectTool() = default;
    ConnectTool(const ConnectTool&) = delete;
    ConnectTool& operator=(const ConnectTool&) = delete;
    virtual bool updateMeshEdit(ImGuiIO* io, EditorRef editor, bool& changed);
    bool update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) override {
        changed = false;
        const auto keepAlive = shared_from_this();
        NodeSelect::update(io, editor, action, changed);
        if (!compat::ngRequireObject(editor)->deformOnly) updateMeshEdit(io, editor, changed);
        return changed;
    }
};
template<class T> requires std::derived_from<T, ConnectTool>
class ToolInfoImpl : public base::ToolInfoBase<T> {
protected:
    void prepareSourceClassInfo() override {
        base::ToolInfoBase<T>::prepareSourceClassInfo();
        base::detail::prepareSourceTemplateClassRecord<ToolInfoImpl<T>, T>(
            "nijigenerate.viewport.common.mesheditor.tools.connect.ToolInfoImpl!(", ").ToolInfoImpl");
    }
public:
    bool viewportTools(bool deformOnly, base::VertexToolMode mode, base::Editors editors) override {
        const auto keepAlive = this->shared_from_this();
        if (deformOnly) return false;
        {
            const auto keys = detail::sourceEditorKeys(editors);
            for (auto node : keys)
                if (!std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(node)) return false;
        }
        return base::ToolInfoBase<T>::viewportTools(deformOnly, mode, editors);
    }
    bool canUse(bool deformOnly, compat::Slice<base::NodeRef> targets) override {
        if (deformOnly) return false;
        for (auto node : targets) if (!std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(node)) return false;
        return true;
    }
    base::VertexToolMode mode() override { return base::VertexToolMode::Connect; }
    compat::RetainedString icon() override { return compat::RetainedString::fromStaticLiteral("\xee\x9d\x86"); }
    compat::RetainedString description() override {
        return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Edge Tool"));
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::tools::connect::ConnectTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.common.mesheditor.tools.connect.ConnectTool)";
    inline static constexpr char unqualifiedArgument[] = "(ConnectTool)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.common.mesheditor.tools.connect.ConnectTool";
    inline static constexpr char unqualifiedElement[] = "ConnectTool";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::connect::ConnectTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.tools.connect.ConnectTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.tools.connect.ConnectTool)";
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::connect::ConnectTool>
struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::connect::ToolInfoImpl<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        return {source::sourceToolFamilyText(
                "(nijigenerate.viewport.common.mesheditor.tools.connect.ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText("(ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText(
                "nijigenerate.viewport.common.mesheditor.tools.connect.ToolInfoImpl!", argument, ""),
            source::sourceToolFamilyText("ToolInfoImpl!", argument, "")};
    }
};
template<class T> requires std::derived_from<T,
    ::nijigenerate::viewport::common::mesheditor::tools::connect::ConnectTool>
struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::connect::ToolInfoImpl<T>> {
    static RetainedString className() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument);
        return source::sourceToolFamilyText(
            "nijigenerate.viewport.common.mesheditor.tools.connect.ToolInfoImpl!", argument, ".ToolInfoImpl");
    }
    static RetainedString templateArgumentName() {
        return SourceTypeSpellings<
            ::nijigenerate::viewport::common::mesheditor::tools::connect::ToolInfoImpl<T>>::get().qualifiedArgument;
    }
};
}
