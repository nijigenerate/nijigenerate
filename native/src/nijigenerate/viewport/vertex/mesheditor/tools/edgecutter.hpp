#pragma once
#include <nijigenerate/viewport/common/mesheditor/tools/select.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nodes/drawable.hpp>
#include <limits>
#include <concepts>
#include <cstdint>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::viewport::vertex::mesheditor::tools::edgecutter {
namespace toolbase = common::mesheditor::tools::base;
namespace detail {
// Only the actual passive Node-handle key-array copy uses this boundary.
template<class Operation> auto allocateEdgeKeys(Operation&& operation) {
    try { return std::forward<Operation>(operation)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
class EdgeCutterTool : public common::mesheditor::tools::select::NodeSelect {
protected:
    void prepareSourceClassInfo() override {
        common::mesheditor::tools::select::NodeSelect::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(EdgeCutterTool),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.vertex.mesheditor.tools.edgecutter.EdgeCutterTool"));
            return true;
        }();
        (void)registered;
    }
public:
    Vec2 dragOrigin{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    Vec2 dragEnd{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    void setToolMode(VertexToolMode, EditorRef) override { isDragging = false; }
    bool onDragStart(Vec2 mouse, EditorRef) override {
        dragOrigin = mouse; dragEnd = mouse; isDragging = true; return true;
    }
    bool onDragUpdate(Vec2 mouse, EditorRef) override {
        if (!isDragging) return false;
        dragEnd = mouse; return true;
    }
    bool onDragEnd(Vec2 mouse, EditorRef editor) override;
    bool update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) override;
    void draw(toolbase::CameraRef camera, EditorRef editor) override;
};
template<class T> requires std::derived_from<T, EdgeCutterTool>
class ToolInfoImpl : public toolbase::ToolInfoBase<T> {
protected:
    void prepareSourceClassInfo() override {
        toolbase::ToolInfoBase<T>::prepareSourceClassInfo();
        toolbase::detail::prepareSourceTemplateClassRecord<ToolInfoImpl<T>, T>(
            "nijigenerate.viewport.vertex.mesheditor.tools.edgecutter.ToolInfoImpl!(", ").ToolInfoImpl");
    }
public:
    bool viewportTools(bool deformOnly, toolbase::VertexToolMode mode, toolbase::Editors editors) override {
        const auto keepAlive = this->weak_from_this().lock();
        if (deformOnly) return false;
        {
            const auto keys = detail::allocateEdgeKeys([&] { return editors.keys(); });
            for (auto node : keys)
                if (!std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(node)) return false;
        }
        return toolbase::ToolInfoBase<T>::viewportTools(deformOnly, mode, editors);
    }
    bool canUse(bool deformOnly, compat::Slice<toolbase::NodeRef> targets) override {
        if (deformOnly) return false;
        for (auto node : targets) if (!std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(node)) return false;
        return true;
    }
    toolbase::VertexToolMode mode() override { return toolbase::VertexToolMode::EdgeCutter; }
    compat::RetainedString icon() override { return compat::RetainedString::fromStaticLiteral("\xef\x87\xb8"); }
    compat::RetainedString description() override {
        return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Edge Cutter"));
    }
    bool displayToolOptions(bool, toolbase::VertexToolMode, toolbase::Editors) override { return false; }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::vertex::mesheditor::tools::edgecutter::EdgeCutterTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.vertex.mesheditor.tools.edgecutter.EdgeCutterTool)";
    inline static constexpr char unqualifiedArgument[] = "(EdgeCutterTool)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.vertex.mesheditor.tools.edgecutter.EdgeCutterTool";
    inline static constexpr char unqualifiedElement[] = "EdgeCutterTool";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::vertex::mesheditor::tools::edgecutter::EdgeCutterTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.vertex.mesheditor.tools.edgecutter.EdgeCutterTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.vertex.mesheditor.tools.edgecutter.EdgeCutterTool)";
};
template<class T>
    requires std::derived_from<T,
        ::nijigenerate::viewport::vertex::mesheditor::tools::edgecutter::EdgeCutterTool>
struct SourceTypeSpellings<
    ::nijigenerate::viewport::vertex::mesheditor::tools::edgecutter::ToolInfoImpl<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        return {source::sourceToolFamilyText(
                    "(nijigenerate.viewport.vertex.mesheditor.tools.edgecutter.ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText("(ToolInfoImpl!", argument, ")"),
            source::sourceToolFamilyText(
                "nijigenerate.viewport.vertex.mesheditor.tools.edgecutter.ToolInfoImpl!", argument, ""),
            source::sourceToolFamilyText("ToolInfoImpl!", argument, "")};
    }
};
template<class T>
    requires std::derived_from<T,
        ::nijigenerate::viewport::vertex::mesheditor::tools::edgecutter::EdgeCutterTool>
struct SourceObjectTypeNames<
    ::nijigenerate::viewport::vertex::mesheditor::tools::edgecutter::ToolInfoImpl<T>> {
    static RetainedString className() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument);
        return source::sourceToolFamilyText(
            "nijigenerate.viewport.vertex.mesheditor.tools.edgecutter.ToolInfoImpl!", argument, ".ToolInfoImpl");
    }
    static RetainedString templateArgumentName() {
        return SourceTypeSpellings<
            ::nijigenerate::viewport::vertex::mesheditor::tools::edgecutter::ToolInfoImpl<T>>::get().qualifiedArgument;
    }
};
}
