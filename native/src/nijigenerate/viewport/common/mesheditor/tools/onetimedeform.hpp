#pragma once
#include "select.hpp"
#include "onetimedeform_types.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <render/scheduler.hpp>
#include <common/resource_key_map.hpp>
#include <resource.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <concepts>
#include <mutex>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::viewport::common::mesheditor::tools::onetimedeform {
using GridDeformer = nicxlive::core::nodes::GridDeformer;
using PathDeformer = nicxlive::core::nodes::PathDeformer;
using Vec2Array = nicxlive::core::math::Vec2Array;
using Mat4 = nicxlive::core::math::Mat4;
using Indices = compat::Slice<std::uint64_t>;
namespace detail {
template<class T> struct TargetSourceTypeNames;
template<> struct TargetSourceTypeNames<GridDeformer> {
    inline static constexpr char className[] = "nijilive.core.nodes.deformer.grid.GridDeformer";
    inline static constexpr char templateArgumentName[] = "(nijilive.core.nodes.deformer.grid.GridDeformer)";
    inline static constexpr char unqualifiedArgument[] = "(GridDeformer)";
    inline static constexpr char qualifiedElement[] = "nijilive.core.nodes.deformer.grid.GridDeformer";
    inline static constexpr char unqualifiedName[] = "GridDeformer";
};
template<> struct TargetSourceTypeNames<PathDeformer> {
    inline static constexpr char className[] = "nijilive.core.nodes.deformer.path.PathDeformer";
    inline static constexpr char templateArgumentName[] = "(nijilive.core.nodes.deformer.path.PathDeformer)";
    inline static constexpr char unqualifiedArgument[] = "(PathDeformer)";
    inline static constexpr char qualifiedElement[] = "nijilive.core.nodes.deformer.path.PathDeformer";
    inline static constexpr char unqualifiedName[] = "PathDeformer";
};
template<class T> void prepareTargetSourceTypeNames() {
    using namespace nicxlive::core::d_runtime;
    if constexpr (requires { TargetSourceTypeNames<T>::className; }) {
        using Names = TargetSourceTypeNames<T>;
        ngRegisterSourceTypeSpellings<T>({
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName),
            compat::RetainedString::fromStaticLiteral(Names::unqualifiedArgument),
            compat::RetainedString::fromStaticLiteral(Names::qualifiedElement),
            compat::RetainedString::fromStaticLiteral(Names::unqualifiedName)});
        ngRegisterObjectTypeNames<T>(
            compat::RetainedString::fromStaticLiteral(Names::className),
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
    } else {
        ngPrepareSourceTypeSpellings<T>();
    }
}
template<std::size_t P, std::size_t S>
compat::RetainedString sourceFamilyName(const char (&prefix)[P], std::string_view argument,
                                      const char (&suffix)[S]) {
    try {
        std::string name(prefix, P - 1);
        name.append(argument);
        name.append(suffix, S - 1);
        return compat::RetainedString(std::move(name));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
struct SelectionTargetHash {
    std::size_t operator()(const NodeRef& value) const {
        return nicxlive::core::ngResourceInterfaceHash(value);
    }
};
struct SelectionTargetEqual {
    bool operator()(const NodeRef& query, const NodeRef& stored) const {
        return nicxlive::core::ngResourceObjectEquals(query, stored);
    }
};
}

template<class T> requires (std::same_as<T, GridDeformer> || std::same_as<T, PathDeformer>)
class OneTimeDeformer : public T {
    nicxlive::core::common::ResourceKeyMap<NodeRef, Indices,
        detail::SelectionTargetHash, detail::SelectionTargetEqual> selectedVerticesByTarget;
    std::recursive_mutex selectionMutex;
protected:
    OneTimeDeformer() : T(typename T::DeferredConstruction{}) {}
    void constructSource(NodeRef parent) {
        if constexpr (std::same_as<T, GridDeformer>) this->constructGridOwned(std::move(parent));
        else this->constructPathOwned(std::move(parent), PathDeformer::CurveType::Spline);
    }
public:
    static std::shared_ptr<OneTimeDeformer> create(NodeRef parent) {
        return ngCreateDerived<OneTimeDeformer>(std::move(parent));
    }
    template<class Arguments> static std::shared_ptr<OneTimeDeformer> createSource(Arguments&& arguments) {
        return ngCreateDerivedSource<OneTimeDeformer>(std::forward<Arguments>(arguments));
    }
    // Derived native constructors initialize fields only; their logical constructor
    // calls OneTimeDeformer::constructSource at the authored super(parent) point.
    // Private derived native/logical constructors may grant friendship to OneTimeDeformer.
    template<class Derived, class... Args> requires std::derived_from<Derived, OneTimeDeformer>
    static std::shared_ptr<Derived> ngCreateDerived(Args&&... args) {
        return ngCreateDerivedSource<Derived>([&] {
            return std::forward_as_tuple(std::forward<Args>(args)...);
        });
    }
    template<class Derived, class Arguments> requires std::derived_from<Derived, OneTimeDeformer>
    static std::shared_ptr<Derived> ngCreateDerivedSource(Arguments&& arguments) {
        nicxlive::core::ngRegisterModelResourceClassNames();
        // Keep construction in the existing friend context. Only memory requests
        // use the allocator's OOM boundary; derived constructors remain outside it.
        nicxlive::core::d_runtime::SourceAllocator<Derived> allocator;
        auto* memory = allocator.allocate(1);
        Derived* object;
        try {
            object = ::new (static_cast<void*>(memory)) Derived();
        } catch (...) {
            allocator.deallocate(memory, 1);
            throw;
        }
        auto result = std::shared_ptr<Derived>(object, [](Derived* value) {
            std::destroy_at(value);
            nicxlive::core::d_runtime::SourceAllocator<Derived>{}.deallocate(value, 1);
        }, allocator);
        base::detail::prepareSourceClassRecord<Derived>();
        std::apply([&](auto&&... values) {
            result->constructSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    virtual bool setSelectedVertices(NodeRef target, Indices selected, Indices mirrored = {}) {
        const auto keepAlive = this->shared_from_this();
        auto next = selected.duplicate();
        for (auto index : mirrored) {
            bool found = false;
            for (auto value : next) if (value == index) { found = true; break; }
            if (!found) next.append(index);
        }
        std::lock_guard lock(selectionMutex);
        auto old = selectedVerticesByTarget.find(target);
        if (next.empty()) {
            if (old != selectedVerticesByTarget.end()) { selectedVerticesByTarget.erase(target); return true; }
        } else {
            bool same = old != selectedVerticesByTarget.end() && old->second.size() == next.size();
            if (same) for (std::size_t i = 0; i < next.size(); ++i)
                if (old->second[i] != next[i]) { same = false; break; }
            if (!same) { selectedVerticesByTarget[std::move(target)] = next; return true; }
        }
        return false;
    }
    void releaseTarget(const NodeRef& target) override {
        const auto keepAlive = this->shared_from_this();
        const auto retainedTarget = target;
        setSelectedVertices(retainedTarget, {});
        T::releaseTarget(retainedTarget);
    }
    nicxlive::core::nodes::DeformResult deformChildren(const NodeRef& target,
        const Vec2Array& originalVertices, Vec2Array& deformation, const Mat4* transform) override {
        const auto keepAlive = this->shared_from_this();
        const auto retainedTarget = target;
        const auto retainedVertices = originalVertices;
        auto resultDeformation = deformation;
        Indices selected;
        bool active = false;
        {
            std::lock_guard lock(selectionMutex);
            active = !selectedVerticesByTarget.empty();
            auto found = selectedVerticesByTarget.find(retainedTarget);
            if (found != selectedVerticesByTarget.end()) selected = found->second.duplicate();
        }
        if (!active) {
            auto result = T::deformChildren(retainedTarget, retainedVertices, resultDeformation, transform);
            deformation = resultDeformation;
            return result;
        }
        auto original = resultDeformation.dup();
        auto result = T::deformChildren(retainedTarget, retainedVertices, resultDeformation, transform);
        if (resultDeformation.size() == original.size() && resultDeformation.size() == retainedVertices.size()) {
            std::vector<bool> included(resultDeformation.size(), false);
            for (auto index : selected) if (index < included.size()) included[index] = true;
            for (std::size_t i = 0; i < resultDeformation.size(); ++i)
                if (!included[i]) resultDeformation[i] = original[i];
        }
        deformation = resultDeformation;
        return result;
    }
    nicxlive::core::nodes::SourceDeformResult deformChildrenSource(NodeRef target,
        Vec2Array originalVertices, Vec2Array originalDeformation, Mat4* transform) override {
        const auto keepAlive = this->shared_from_this();
        Indices selected;
        bool active;
        {
            std::lock_guard lock(selectionMutex);
            active = !selectedVerticesByTarget.empty();
            const auto found = selectedVerticesByTarget.find(target);
            if (found != selectedVerticesByTarget.end()) selected = found->second.duplicate();
        }
        if (!active) return T::deformChildrenSource(target, originalVertices, originalDeformation, transform);
        auto original = originalDeformation.dup();
        auto result = T::deformChildrenSource(target, originalVertices, originalDeformation, transform);
        auto& offsets = std::get<0>(result);
        if (offsets.size() != original.size() || offsets.size() != originalVertices.size()) return result;
        compat::Slice<bool> included;
        included.resize(offsets.size());
        for (auto index : selected) if (index < included.size()) included[index] = true;
        for (std::size_t i = 0; i < included.size(); ++i)
            if (!included[i]) offsets[i] = original[i];
        return result;
    }
    void registerRenderTasks(std::shared_ptr<nicxlive::core::TaskScheduler> scheduler) override {
        if (!scheduler) return;
        using nicxlive::core::TaskOrder;
        using nicxlive::core::TaskKind;
        const auto self = std::static_pointer_cast<OneTimeDeformer>(this->shared_from_this());
        scheduler->addTask(TaskOrder::Init, TaskKind::Init, [self](auto& ctx) { self->runBeginTask(ctx); });
        scheduler->addTask(TaskOrder::PreProcess, TaskKind::PreProcess, [self](auto& ctx) { self->runPreProcessTask(ctx); });
        scheduler->addTask(TaskOrder::Post0, TaskKind::PostProcess, [self](auto& ctx) { self->runPostTask0(ctx); });
        bool first = false;
        {
            const auto filters = this->postProcessFilters;
            for (const auto filter : filters) if (filter.stage == 1) { first = true; break; }
        }
        if (first) scheduler->addTask(TaskOrder::Post1, TaskKind::PostProcess,
            [self](auto& ctx) { self->runPostTask1(ctx); });
        bool second = false;
        {
            const auto filters = this->postProcessFilters;
            for (const auto filter : filters) if (filter.stage == 2) { second = true; break; }
        }
        if (second) scheduler->addTask(TaskOrder::Post2, TaskKind::PostProcess,
            [self](auto& ctx) { self->runPostTask2(ctx); });
        scheduler->addTask(TaskOrder::Final, TaskKind::Finalize, [self](auto& ctx) { self->runFinalTask(ctx); });
    }
    void registerRenderTasks(nicxlive::core::TaskScheduler& scheduler) override {
        using nicxlive::core::TaskOrder;
        using nicxlive::core::TaskKind;
        auto self = std::static_pointer_cast<OneTimeDeformer>(this->shared_from_this());
        scheduler.addTask(TaskOrder::Init, TaskKind::Init, [self](auto& ctx) { self->runBeginTask(ctx); });
        scheduler.addTask(TaskOrder::PreProcess, TaskKind::PreProcess, [self](auto& ctx) { self->runPreProcessTask(ctx); });
        scheduler.addTask(TaskOrder::Post0, TaskKind::PostProcess, [self](auto& ctx) { self->runPostTaskImpl(0, ctx); });
        bool first = false;
        for (const auto& filter : this->postProcessFilters) {
            if (filter.stage == 1) { first = true; break; }
        }
        if (first) scheduler.addTask(TaskOrder::Post1, TaskKind::PostProcess, [self](auto& ctx) { self->runPostTaskImpl(1, ctx); });
        bool second = false;
        for (const auto& filter : this->postProcessFilters) {
            if (filter.stage == 2) { second = true; break; }
        }
        if (second) scheduler.addTask(TaskOrder::Post2, TaskKind::PostProcess, [self](auto& ctx) { self->runPostTaskImpl(2, ctx); });
        scheduler.addTask(TaskOrder::Final, TaskKind::Finalize, [self](auto& ctx) { self->runFinalTask(ctx); });
    }
};

class OneTimeDeformBase : public select::NodeSelect {
protected:
    void prepareSourceClassInfo() override {
        select::NodeSelect::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(OneTimeDeformBase),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformBase"));
            return true;
        }();
        (void)registered;
    }
public:
    enum class OneTimeDeformActionID : std::int32_t {
        SwitchMode = 10000, SelectTargetVertex, ToggleTargetVertex, SelectTargetArea,
        ContinueTargetSelection, EndTargetSelection, End
    };
    SubToolMode mode = SubToolMode::Select;
    SubToolMode prevMode = SubToolMode::Vertex;
    SubToolMode appliedMode = SubToolMode::Vertex;
    bool acquired = false, selectingTargetVertices = false, subToolGestureActive = false;
    virtual void setSubToolMode(SubToolMode next, EditorRef editor);
};

template<class T> class OneTimeDeform : public OneTimeDeformBase {
    bool subToolIsDragging();
    void switchSubTool(SubToolMode next);
protected:
    void prepareSourceClassInfo() override {
        OneTimeDeformBase::prepareSourceClassInfo();
        base::detail::prepareSourceClassRecord<OneTimeDeform<T>>();
    }
public:
    bool onDragStart(base::Vec2 mouse, EditorRef editor) override;
    bool onDragUpdate(base::Vec2 mouse, EditorRef editor) override;
    bool onDragEnd(base::Vec2 mouse, EditorRef editor) override;
    void setToolMode(base::VertexToolMode mode, EditorRef editor) override;
    void finalizeToolMode(EditorRef editor) override;
    void abortToolMode(EditorRef editor) override;
    std::int32_t peek(ImGuiIO* io, EditorRef editor) override;
    std::int32_t unify(compat::Slice<std::int32_t> actions) override;
    bool update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) override;
    void draw(base::CameraRef camera, EditorRef editor) override;
};

namespace detail {
bool ngDisplayOneTimeDeformToolOptions(bool grid, base::Editors editors);
}

template<class T>
requires (std::derived_from<T, OneTimeDeform<GridDeformer>> ||
          std::derived_from<T, OneTimeDeform<PathDeformer>>)
class ToolInfoImpl : public base::ToolInfoBase<T> {
    static constexpr bool grid = std::derived_from<T, OneTimeDeform<GridDeformer>>;
protected:
    void prepareSourceClassInfo() override {
        base::ToolInfoBase<T>::prepareSourceClassInfo();
        base::detail::prepareSourceTemplateClassRecord<ToolInfoImpl<T>, T>(
            "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.ToolInfoImpl!(", ").ToolInfoImpl");
    }
public:
    bool displayToolOptions(bool, base::VertexToolMode, base::Editors editors) override {
        const auto keepAlive = this->shared_from_this();
        return detail::ngDisplayOneTimeDeformToolOptions(grid, std::move(editors));
    }
    bool viewportTools(bool deformOnly, base::VertexToolMode mode, base::Editors editors) override {
        if (deformOnly) return base::ToolInfoBase<T>::viewportTools(deformOnly, mode, std::move(editors));
        return false;
    }
    bool canUse(bool deformOnly, compat::Slice<NodeRef>) override { return deformOnly; }
    base::VertexToolMode mode() override {
        return grid ? base::VertexToolMode::AltGridDeform : base::VertexToolMode::AltBezierDeform;
    }
    compat::RetainedString icon() override {
        if constexpr (grid) return compat::RetainedString::fromStaticLiteral("\xee\x8f\xac");
        else return compat::RetainedString::fromStaticLiteral("\xee\xab\x8d");
    }
    compat::RetainedString description() override {
        return core::translation::ngTranslateRetained(grid ?
            compat::RetainedString::fromStaticLiteral("Grid deformation") :
            compat::RetainedString::fromStaticLiteral("Path deformation"));
    }
};
template<class T> using OneTimeDeformToolInfo = ToolInfoImpl<OneTimeDeform<T>>;
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeformBase::OneTimeDeformActionID> {
    using Enum = ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::
        OneTimeDeformBase::OneTimeDeformActionID;
    inline static constexpr std::string_view name = "OneTimeDeformActionID";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::SwitchMode, "SwitchMode"},
        FormatEnumMember<Enum>{Enum::SelectTargetVertex, "SelectTargetVertex"},
        FormatEnumMember<Enum>{Enum::ToggleTargetVertex, "ToggleTargetVertex"},
        FormatEnumMember<Enum>{Enum::SelectTargetArea, "SelectTargetArea"},
        FormatEnumMember<Enum>{Enum::ContinueTargetSelection, "ContinueTargetSelection"},
        FormatEnumMember<Enum>{Enum::EndTargetSelection, "EndTargetSelection"},
        FormatEnumMember<Enum>{Enum::End, "End"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeformBase::OneTimeDeformActionID> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformBase.OneTimeDeformActionID)";
    inline static constexpr char unqualifiedArgument[] = "(OneTimeDeformActionID)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformBase.OneTimeDeformActionID";
    inline static constexpr char unqualifiedElement[] = "OneTimeDeformActionID";
};

template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeformBase> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformBase)";
    inline static constexpr char unqualifiedArgument[] = "(OneTimeDeformBase)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformBase";
    inline static constexpr char unqualifiedElement[] = "OneTimeDeformBase";
};
template<> struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeformBase> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformBase";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformBase)";
};
template<class T>
requires (std::same_as<T, ::nicxlive::core::nodes::GridDeformer> ||
          std::same_as<T, ::nicxlive::core::nodes::PathDeformer>)
struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeformer<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::detail;
        source::prepareTargetSourceTypeNames<T>();
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        return {
            source::sourceFamilyName(
                "(nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformer!",
                argument.view(), ")"),
            source::sourceFamilyName("(OneTimeDeformer!", argument.view(), ")"),
            source::sourceFamilyName(
                "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformer!",
                argument.view(), ""),
            source::sourceFamilyName("OneTimeDeformer!", argument.view(), "")};
    }
};

template<class T>
requires (std::same_as<T, ::nicxlive::core::nodes::GridDeformer> ||
          std::same_as<T, ::nicxlive::core::nodes::PathDeformer>)
struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeformer<T>> {
    static RetainedString className() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::detail;
        static const auto name = [] {
            source::prepareTargetSourceTypeNames<T>();
            const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument);
            return source::sourceFamilyName(
                "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeformer!",
                argument.view(), ".OneTimeDeformer");
        }();
        return name;
    }
    static RetainedString templateArgumentName() {
        static const auto name = SourceTypeSpellings<
            ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeformer<T>>::get()
            .qualifiedArgument;
        return name;
    }
};
template<class T> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeform<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::detail;
        source::prepareTargetSourceTypeNames<T>();
        const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        return {
            source::sourceFamilyName(
                "(nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeform!",
                argument.view(), ")"),
            source::sourceFamilyName("(OneTimeDeform!", argument.view(), ")"),
            source::sourceFamilyName(
                "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeform!",
                argument.view(), ""),
            source::sourceFamilyName("OneTimeDeform!", argument.view(), "")};
    }
};

template<class T>
struct SourceObjectTypeNames<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeform<T>> {
    static RetainedString className() {
        namespace source = ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::detail;
        static const auto name = [] {
            source::prepareTargetSourceTypeNames<T>();
            const auto argument = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedArgument);
            return source::sourceFamilyName(
                "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.OneTimeDeform!",
                argument.view(), ".OneTimeDeform");
        }();
        return name;
    }
    static RetainedString templateArgumentName() {
        static const auto name = SourceTypeSpellings<
            ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::OneTimeDeform<T>>::get()
            .qualifiedArgument;
        return name;
    }
};
}

namespace nijigenerate::viewport::common::mesheditor::tools::onetimedeform {
extern template class OneTimeDeform<GridDeformer>;
extern template class OneTimeDeform<PathDeformer>;
}
#include "onetimedeform_scope.hpp"
