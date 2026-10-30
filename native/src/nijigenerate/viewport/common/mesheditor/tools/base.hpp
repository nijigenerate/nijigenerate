#pragma once
#include <nijigenerate/viewport/common/mesheditor/operations/base.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/tooltip.hpp>
#include <concepts>
#include <memory>
#include <new>
#include <source_location>
#include <string>
#include <stdexcept>
#include <utility>
#include <common/resource_key_map.hpp>
#include <resource.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/throwable.hpp>

namespace nijigenerate::actions { class GroupAction; }
namespace nijigenerate::actions::mesheditor {
class DeformationAction;
template<class T> class MeshEditorAction;
}

namespace nijigenerate::viewport::common::mesheditor::tools::base {

using Editor = operations::base::IncMeshEditorOne;
using EditorRef = std::shared_ptr<Editor>;
using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
using DrawableRef = std::shared_ptr<nicxlive::core::nodes::Drawable>;
using CameraRef = std::shared_ptr<nicxlive::core::math::Camera>;
using Vec2 = nicxlive::core::math::Vec2;
using VertexToolMode = enums::VertexToolMode;
using DeformationAction = actions::mesheditor::DeformationAction;
template<class T> using MeshEditorActionRef = std::shared_ptr<actions::mesheditor::MeshEditorAction<T>>;
struct EditorNodeHash {
    std::size_t operator()(const NodeRef& value) const {
        return nicxlive::core::ngResourceInterfaceHash(value);
    }
};
struct EditorNodeEqual {
    bool operator()(const NodeRef& query, const NodeRef& stored) const {
        return nicxlive::core::ngResourceObjectEquals(query, stored);
    }
};
using Editors = nicxlive::core::common::ResourceKeyMap<NodeRef, EditorRef, EditorNodeHash, EditorNodeEqual>;

namespace detail {
template<class Class, class Argument, std::size_t P, std::size_t S>
void prepareSourceTemplateClassRecord(const char (&prefix)[P], const char (&suffix)[S]) {
    static const bool registered = [&prefix, &suffix] {
        const auto argument = nicxlive::core::d_runtime::ngSourceTypeSpelling<Argument>(
            nicxlive::core::d_runtime::SourceTypeSpelling::QualifiedElement);
        const auto name = [&] {
            try {
                std::string text(prefix, P - 1);
                text.append(argument.view());
                text.append(suffix, S - 1);
                return compat::RetainedString(std::move(text));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }();
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Class),
            name);
        return true;
    }();
    (void)registered;
}
template<class T> void prepareSourceClassRecord() {
    using namespace nicxlive::core::d_runtime;
    if constexpr (requires { SourceObjectTypeNames<T>::className(); }) {
        static const bool registered = [] {
            ngRegisterObjectClassName(typeid(T), RetainedString(SourceObjectTypeNames<T>::className()));
            return true;
        }();
        (void)registered;
    } else if constexpr (requires { SourceObjectTypeNames<T>::className; }) {
        static const bool registered = [] {
            ngRegisterObjectClassName(typeid(T),
                RetainedString::fromStaticLiteral(SourceObjectTypeNames<T>::className));
            return true;
        }();
        (void)registered;
    }
    // An extension may instead register its exact Source ClassInfo explicitly.
    // Missing metadata is never replaced by a native type spelling or base name.
}
[[noreturn]] void throwObjectComparison(const std::type_info& type);
}

class Tool : public std::enable_shared_from_this<Tool> {
public:
    virtual ~Tool() = default;
    Tool() = default;
    Tool(const Tool&) = delete;
    Tool& operator=(const Tool&) = delete;
    virtual std::int32_t peek(ImGuiIO* io, EditorRef editor) = 0;
    virtual std::int32_t unify(compat::Slice<std::int32_t> actions) = 0;
    // Each actual derived body must initialize changed=false at entry, matching
    // source out bool; a native reference alone does not perform that reset.
    virtual bool update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) = 0;
    virtual void setToolMode(VertexToolMode mode, EditorRef editor) = 0;
    virtual void draw(CameraRef camera, EditorRef editor) = 0;
    virtual void finalizeToolMode(EditorRef editor) = 0;
    virtual void abortToolMode(EditorRef) {}
    // Called only when the owning editing session is discarded, never on a tool switch.
    virtual void releasePendingActions() {}

    virtual MeshEditorActionRef<DeformationAction> editorAction(NodeRef target, std::shared_ptr<DeformationAction> action);
    virtual MeshEditorActionRef<actions::GroupAction> editorAction(NodeRef target, std::shared_ptr<actions::GroupAction> action);
    virtual MeshEditorActionRef<DeformationAction> editorAction(DrawableRef target, std::shared_ptr<DeformationAction> action);
    virtual MeshEditorActionRef<actions::GroupAction> editorAction(DrawableRef target, std::shared_ptr<actions::GroupAction> action);
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Tool> other);
    virtual int opCmp(std::shared_ptr<Tool> other);
protected:
    virtual void prepareSourceClassInfo();
};

class Draggable {
public:
    virtual ~Draggable() = default;
    virtual bool onDragStart(Vec2 mouse, EditorRef editor) = 0;
    virtual bool onDragUpdate(Vec2 mouse, EditorRef editor) = 0;
    virtual bool onDragEnd(Vec2 mouse, EditorRef editor) = 0;
};

class ToolInfo : public std::enable_shared_from_this<ToolInfo> {
public:
    ToolInfo() = default;
    virtual ~ToolInfo() = default;
    ToolInfo(const ToolInfo&) = delete;
    ToolInfo& operator=(const ToolInfo&) = delete;
    virtual void setupToolMode(EditorRef editor, VertexToolMode mode) = 0;
    virtual bool viewportTools(bool deformOnly, VertexToolMode mode, Editors editors) = 0;
    virtual bool displayToolOptions(bool deformOnly, VertexToolMode mode, Editors editors) = 0;
    virtual compat::RetainedString icon() = 0;
    virtual compat::RetainedString description() = 0;
    virtual VertexToolMode mode() = 0;
    virtual std::shared_ptr<Tool> newTool() = 0;
    virtual bool canUse(bool deformOnly, compat::Slice<NodeRef> targets) = 0;
};

template<class T> class ToolInfoBase : public ToolInfo {
public:
    void setupToolMode(EditorRef editor, VertexToolMode mode) override {
        const auto keepAlive = this->shared_from_this();
        compat::ngRequireObject(editor)->finalizeToolMode();
        compat::ngRequireObject(editor)->setToolMode(mode);
        compat::ngRequireObject(editor)->setPath(nullptr);
        compat::ngRequireObject(editor)->refreshMesh();
    }
    bool viewportTools(bool, VertexToolMode currentMode, Editors) override {
        const auto keepAlive = this->shared_from_this();
        // Preserve source icon/toStringz evaluation before the virtual mode call.
        const auto label = [&] {
            const auto text = icon();
            compat::SourceCString value{compat::RetainedStringSlice(text)};
            compat::detail::ngSourceCStringPostconditions(text.view(), value.data());
            return value;
        }();
        const auto color = currentMode == mode() ? widgets::colorUndefined : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        const bool result = widgets::ngButtonColored(label.data(), ImVec2(0.0f, 0.0f), color);
        widgets::ngTooltip(compat::RetainedStringSlice(description()));
        return result;
    }
    bool displayToolOptions(bool, VertexToolMode, Editors) override { return false; }
    virtual VertexToolMode mode() override = 0;
    virtual compat::RetainedString icon() override = 0;
    virtual compat::RetainedString description() override = 0;
    std::shared_ptr<Tool> newTool() override {
        static_assert(std::derived_from<T, Tool>);
        auto tool = std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{});
        detail::prepareSourceClassRecord<T>();
        return tool;
    }
    bool canUse(bool, compat::Slice<NodeRef>) override { return true; }
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->shared_from_this();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
    virtual std::size_t toHash() {
        const auto keepAlive = this->weak_from_this().lock();
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<ToolInfo> other) {
        const auto keepAlive = this->weak_from_this().lock();
        return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
    }
    virtual int opCmp(std::shared_ptr<ToolInfo>) {
        const auto keepAlive = this->shared_from_this();
        prepareSourceClassInfo();
        detail::throwObjectComparison(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo() {
        detail::prepareSourceTemplateClassRecord<ToolInfoBase<T>, T>(
            "nijigenerate.viewport.common.mesheditor.tools.base.ToolInfoBase!(", ").ToolInfoBase");
    }
};

} // namespace nijigenerate::viewport::common::mesheditor::tools::base

namespace nijigenerate::viewport::common::mesheditor::tools::base::detail {
inline compat::RetainedString sourceToolFamilyText(std::string_view prefix,
    const compat::RetainedString& argument, std::string_view suffix) {
    try {
        std::string text(prefix);
        text.append(argument.view());
        text.append(suffix);
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
} // namespace nijigenerate::viewport::common::mesheditor::tools::base::detail

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::common::mesheditor::tools::base::Tool> {
    inline static constexpr char className[] = "nijigenerate.viewport.common.mesheditor.tools.base.Tool";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.common.mesheditor.tools.base.Tool)";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::tools::base::Tool> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.viewport.common.mesheditor.tools.base.Tool)"),
            RetainedString::fromStaticLiteral("(Tool)"),
            RetainedString::fromStaticLiteral("nijigenerate.viewport.common.mesheditor.tools.base.Tool"),
            RetainedString::fromStaticLiteral("Tool")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::tools::base::Draggable> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.viewport.common.mesheditor.tools.base.Draggable)"),
            RetainedString::fromStaticLiteral("(Draggable)"),
            RetainedString::fromStaticLiteral("nijigenerate.viewport.common.mesheditor.tools.base.Draggable"),
            RetainedString::fromStaticLiteral("Draggable")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::tools::base::ToolInfo> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.viewport.common.mesheditor.tools.base.ToolInfo)"),
            RetainedString::fromStaticLiteral("(ToolInfo)"),
            RetainedString::fromStaticLiteral("nijigenerate.viewport.common.mesheditor.tools.base.ToolInfo"),
            RetainedString::fromStaticLiteral("ToolInfo")};
    }
};
template<class T> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::base::ToolInfoBase<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        return {source::sourceToolFamilyText(
                    "(nijigenerate.viewport.common.mesheditor.tools.base.ToolInfoBase!", argument, ")"),
            source::sourceToolFamilyText("(ToolInfoBase!", argument, ")"),
            source::sourceToolFamilyText(
                "nijigenerate.viewport.common.mesheditor.tools.base.ToolInfoBase!", argument, ""),
            source::sourceToolFamilyText("ToolInfoBase!", argument, "")};
    }
};
template<class T> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::base::ToolInfoBase<T>> {
    static RetainedString className() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::base::detail;
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument);
        return source::sourceToolFamilyText(
            "nijigenerate.viewport.common.mesheditor.tools.base.ToolInfoBase!", argument, ".ToolInfoBase");
    }
    static RetainedString templateArgumentName() {
        return SourceTypeSpellings<
            ::nijigenerate::viewport::common::mesheditor::tools::base::ToolInfoBase<T>>::get().qualifiedArgument;
    }
};
} // namespace nicxlive::core::d_runtime
