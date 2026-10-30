#pragma once
#include "package.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/viewport/common/spline.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/onetimedeform_types.hpp>
#include <nijigenerate/project.hpp>
#include <param/binding_impl.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::viewport::model::mesheditor::drawable {
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
class IncMeshEditorOneFor;
}
namespace nijigenerate::actions::mesheditor {
using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
using ParameterRef = std::shared_ptr<nicxlive::core::param::Parameter>;
using BindingRef = std::shared_ptr<nicxlive::core::param::DeformationParameterBinding>;
using EditorRef = std::shared_ptr<viewport::common::mesheditor::operations::base::IncMeshEditorOne>;
using Vec2u = nicxlive::core::param::Vec2u;
using Vec2 = nicxlive::core::math::Vec2;
using Vec3 = nicxlive::core::math::Vec3;
using Mat4 = nicxlive::core::math::Mat4;
using SubToolMode = viewport::common::mesheditor::tools::onetimedeform::SubToolMode;
using MeshVertex = core::math::mesh::MeshVertex;

namespace detail {
template<class T> nicxlive::core::d_runtime::SourceTypeSpellingNames
ngMeshActionTypeSpellings(std::string_view declaration);
template<class T> nicxlive::core::d_runtime::SourceTypeSpellingNames ngMeshSnapshotTypeSpellings();
template<class T> void ngPrepareMeshActionMetadata();
}

class DeformationAction : public LazyBoundAction, public std::enable_shared_from_this<DeformationAction> {
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void notifyChanged();
    void exchange(bool redo);
protected:
    DeformationAction() = default;
    // Invoke on the real shared receiver after allocation, including derived factories.
    void constructDeformationOwned(ActionText name, NodeRef target, compat::SharedDelegate<void()> update);
public:
    DeformationAction(const DeformationAction&) = delete;
    DeformationAction& operator=(const DeformationAction&) = delete;
    DeformationAction(DeformationAction&&) = delete;
    DeformationAction& operator=(DeformationAction&&) = delete;
    ActionText name;
    bool dirty = false;
    ParameterRef param;
    NodeRef target;
    BindingRef deform;
    bool isSet = false;
    compat::Slice<Vec2> vertices;
    Vec2u keypoint{};
    bool bindingAdded = false;
    bool undoable = true;
    static std::shared_ptr<DeformationAction> create(ActionText name, NodeRef target,
                                                   compat::SharedDelegate<void()> update = {});
    template<class Arguments> static std::shared_ptr<DeformationAction> createSource(Arguments&& arguments) {
        detail::ngPrepareMeshActionMetadata<DeformationAction>();
        auto result = std::allocate_shared<DeformationAction>(
            nicxlive::core::d_runtime::SourceAllocator<DeformationAction>{});
        std::apply([&](auto&&... values) {
            result->constructDeformationOwned(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    virtual compat::RetainedString toString();
    virtual EditorRef self();
    virtual void addVertex(MeshVertex*) {}
    virtual void resolveCurrentFilter();
    virtual void markAsDirty() { dirty = true; }
    void updateNewState() override;
    void clear() override;
    virtual bool isApplyable();
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr other) override;
    bool canMerge(ActionPtr) override { return false; }
};

template<class T> class MeshEditorAction : public LazyBoundAction,
                                         public std::enable_shared_from_this<MeshEditorAction<T>> {
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
protected:
    MeshEditorAction() = default;
    void initialize(NodeRef target, std::shared_ptr<T> action);
public:
    using TSelf = MeshEditorAction<T>;
    MeshEditorAction(const MeshEditorAction&) = delete;
    MeshEditorAction& operator=(const MeshEditorAction&) = delete;
    MeshEditorAction(MeshEditorAction&&) = delete;
    MeshEditorAction& operator=(MeshEditorAction&&) = delete;
    NodeRef target;
    std::shared_ptr<T> action;
    Mat4 oldEditorTransform = Mat4::identity(), newEditorTransform = Mat4::identity();
    ParameterRef param;
    Vec2u oldKeypoint{}, newKeypoint{};
    struct SubToolModeSnapshot {
        NodeRef target;
        SubToolMode mode = SubToolMode::Select;
        // This defining declaration knows its real enclosing T. ADL exposes
        // that exact record's origin without deducing T from a nested type.
        friend nicxlive::core::d_runtime::SourceTypeSpellingNames
        ngMeshSnapshotTypeOrigin(SubToolModeSnapshot*) {
            return detail::ngMeshSnapshotTypeSpellings<T>();
        }
    };
    compat::Slice<SubToolModeSnapshot> oldSubToolModes, newSubToolModes;
    static std::shared_ptr<MeshEditorAction> create(NodeRef target, std::shared_ptr<T> action = {});
    virtual compat::RetainedString toString();
    virtual compat::Slice<SubToolModeSnapshot> captureSubToolModes();
    virtual void restoreSubToolModes(compat::Slice<SubToolModeSnapshot> modes);
    virtual void setAction(std::shared_ptr<T> value) {
        [[maybe_unused]] const auto keepAlive = this->shared_from_this();
        action = value;
    }
    virtual EditorRef self();
    void updateNewState() override;
    void clear() override;
    virtual bool isApplyable();
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr other) override;
    bool canMerge(ActionPtr other) override;
    virtual bool dirty() { return bool(action); }
};

template<class T> class MeshEditorPathDeformAction : public MeshEditorAction<T> {
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void captureOld();
    void restorePathState(bool newer);
protected:
    MeshEditorPathDeformAction() = default;
    // Run the complete Source super prefix on an already-owned most-derived receiver.
    void constructMeshEditorPathOwned(NodeRef target, std::shared_ptr<T> action);
public:
    using Points = viewport::common::spline::Array<viewport::common::spline::SplinePoint>;
    using DeformEditor = viewport::model::mesheditor::drawable::IncMeshEditorOneFor<
        nicxlive::core::nodes::Drawable, project::EditMode::ModelEdit>;
    Points oldPathPoints, oldTargetPathPoints, newPathPoints, newTargetPathPoints;
    compat::Slice<Vec2> oldInitTangents, newInitTangents;
    compat::Slice<Vec3> oldRefOffsets, newRefOffsets;
    compat::Slice<std::uint64_t> oldSelected, newSelected;
    compat::Slice<Vec2> oldDeformation, newDeformation;
    bool hasOldInitTangents = false, hasOldRefOffsets = false;
    bool hasOldDeformation = false, hasNewDeformation = false;
    float oldOrigX = NAN, oldOrigY = NAN, oldOrigRotZ = NAN;
    float newOrigX = NAN, newOrigY = NAN, newOrigRotZ = NAN;
    static std::shared_ptr<MeshEditorPathDeformAction> create(NodeRef target, std::shared_ptr<T> action = {});
    virtual std::shared_ptr<DeformEditor> selfDeform();
    virtual std::shared_ptr<viewport::common::spline::CatmullSpline> path();
    void updateNewState() override;
    void clear() override;
    void rollback() override;
    void redo() override;
};

class SubToolModeChangeAction : public Action, public std::enable_shared_from_this<SubToolModeChangeAction> {
public:
    SubToolModeChangeAction(const SubToolModeChangeAction&) = delete;
    SubToolModeChangeAction& operator=(const SubToolModeChangeAction&) = delete;
    SubToolModeChangeAction(SubToolModeChangeAction&&) = delete;
    SubToolModeChangeAction& operator=(SubToolModeChangeAction&&) = delete;
    compat::Slice<NodeRef> targets;
    compat::Slice<SubToolMode> oldModes, newModes;
    SubToolModeChangeAction(compat::Slice<NodeRef> targets, compat::Slice<SubToolMode> oldModes,
                            compat::Slice<SubToolMode> newModes);
    virtual compat::RetainedString toString();
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
};
// These are the two actual source instantiations used by the editor/tool modules.
extern template class MeshEditorAction<DeformationAction>;
extern template class MeshEditorAction<GroupAction>;
extern template class MeshEditorPathDeformAction<DeformationAction>;
extern template class MeshEditorPathDeformAction<GroupAction>;

namespace detail {

template<class T> nicxlive::core::d_runtime::SourceTypeSpellingNames
ngMeshActionTypeSpellings(std::string_view declaration) {
    using namespace nicxlive::core::d_runtime;
    const auto argument=ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
    try {
        std::string unqualified(declaration);
        unqualified+='!';
        unqualified.append(argument.view());
        auto qualified=std::string("nijigenerate.actions.mesheditor.")+unqualified;
        return {compat::RetainedString("("+qualified+")"),compat::RetainedString("("+unqualified+")"),
            compat::RetainedString(std::move(qualified)),compat::RetainedString(std::move(unqualified))};
    } catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class T> nicxlive::core::d_runtime::SourceTypeSpellingNames ngMeshSnapshotTypeSpellings() {
    const auto parent=ngMeshActionTypeSpellings<T>("MeshEditorAction");
    try {
        std::string qualified(parent.qualifiedElement.view());
        qualified+=".MeshEditorAction.SubToolModeSnapshot";
        return {compat::RetainedString("("+qualified+")"),
            compat::RetainedString::fromStaticLiteral("(SubToolModeSnapshot)"),
            compat::RetainedString(std::move(qualified)),
            compat::RetainedString::fromStaticLiteral("SubToolModeSnapshot")};
    } catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class T> compat::RetainedString ngMeshActionSourceClassName(std::string_view declaration) {
    const auto argument = nicxlive::core::d_runtime::ngObjectTemplateArgumentName<T>();
    constexpr std::string_view module = "nijigenerate.actions.mesheditor.";
    try {
        std::string result;
        result.reserve(module.size() + declaration.size() + 1 + argument.size() + 1 + declaration.size());
        result.append(module);
        result.append(declaration);
        result.push_back('!');
        result.append(argument.view());
        result.push_back('.');
        result.append(declaration);
        return compat::RetainedString(std::move(result));
    } catch (const std::bad_alloc&) {
        throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
            std::source_location::current());
    } catch (const std::length_error&) {
        throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
            std::source_location::current());
    }
}

template<class T> void ngPrepareMeshActionMetadata() {
    using namespace nicxlive::core::d_runtime;
    using Names=SourceObjectTypeNames<T>;
    const auto className=[] {
        if constexpr (requires { Names::className(); }) return Names::className();
        else return compat::RetainedString::fromStaticLiteral(Names::className);
    }();
    const auto argumentName=[] {
        if constexpr (requires { Names::templateArgumentName(); }) return Names::templateArgumentName();
        else return compat::RetainedString::fromStaticLiteral(Names::templateArgumentName);
    }();
    ngPrepareSourceTypeSpellings<T>();
    ngRegisterObjectTypeNames<T>(className,argumentName);
    ngRegisterObjectClassName(typeid(T),className);
}
}
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::actions::mesheditor::SubToolModeChangeAction> {
    inline static constexpr char className[]="nijigenerate.actions.mesheditor.SubToolModeChangeAction";
    inline static constexpr char templateArgumentName[]="(nijigenerate.actions.mesheditor.SubToolModeChangeAction)";
};

template<class T> struct SourceObjectTypeNames<::nijigenerate::actions::mesheditor::MeshEditorAction<T>> {
    static RetainedString className() {
        return ::nijigenerate::actions::mesheditor::detail::ngMeshActionSourceClassName<T>("MeshEditorAction");
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::actions::mesheditor::detail::ngMeshActionTypeSpellings<T>(
            "MeshEditorAction").qualifiedArgument;
    }
};

template<class T> struct SourceObjectTypeNames<::nijigenerate::actions::mesheditor::MeshEditorPathDeformAction<T>> {
    static RetainedString className() {
        return ::nijigenerate::actions::mesheditor::detail::ngMeshActionSourceClassName<T>("MeshEditorPathDeformAction");
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::actions::mesheditor::detail::ngMeshActionTypeSpellings<T>(
            "MeshEditorPathDeformAction").qualifiedArgument;
    }
};

template<> struct SourceTypeSpellings<::nijigenerate::actions::mesheditor::DeformationAction> {
    inline static constexpr bool isNativeObject=true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.actions.mesheditor.DeformationAction)"),
            RetainedString::fromStaticLiteral("(DeformationAction)"),
            RetainedString::fromStaticLiteral("nijigenerate.actions.mesheditor.DeformationAction"),
            RetainedString::fromStaticLiteral("DeformationAction")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::actions::mesheditor::SubToolModeChangeAction> {
    inline static constexpr bool isNativeObject=true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.actions.mesheditor.SubToolModeChangeAction)"),
            RetainedString::fromStaticLiteral("(SubToolModeChangeAction)"),
            RetainedString::fromStaticLiteral("nijigenerate.actions.mesheditor.SubToolModeChangeAction"),
            RetainedString::fromStaticLiteral("SubToolModeChangeAction")};
    }
};
template<class T> struct SourceTypeSpellings<::nijigenerate::actions::mesheditor::MeshEditorAction<T>> {
    inline static constexpr bool isNativeObject=true;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::actions::mesheditor::detail::ngMeshActionTypeSpellings<T>("MeshEditorAction");
    }
};
template<class T> struct SourceTypeSpellings<::nijigenerate::actions::mesheditor::MeshEditorPathDeformAction<T>> {
    inline static constexpr bool isNativeObject=true;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::actions::mesheditor::detail::ngMeshActionTypeSpellings<T>("MeshEditorPathDeformAction");
    }
};
// The ADL function is defined by the actual nested record above. This only
// selects its defining metadata; it never constructs a record or tests a body.
template<class Record> requires requires(Record* record) { ngMeshSnapshotTypeOrigin(record); }
struct SourceTypeSpellings<Record> {
    inline static constexpr bool isNativeObject=false;
    static SourceTypeSpellingNames get() {
        return ngMeshSnapshotTypeOrigin(static_cast<Record*>(nullptr));
    }
};

// Original actions/mesheditor.d: non-template class in the named module.
template<> struct SourceObjectTypeNames<::nijigenerate::actions::mesheditor::DeformationAction> {
    inline static constexpr char className[] = "nijigenerate.actions.mesheditor.DeformationAction";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.actions.mesheditor.DeformationAction)";
};

}

#include "mesheditor_action_templates.hpp"
#include "mesheditor_path_templates.hpp"

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::mesheditor::DeformationAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::mesheditor::DeformationAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::mesheditor::SubToolModeChangeAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::mesheditor::SubToolModeChangeAction>> {};
template<class T> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::mesheditor::MeshEditorAction<T>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::mesheditor::MeshEditorAction<T>>> {};
template<class T> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::mesheditor::MeshEditorPathDeformAction<T>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::mesheditor::MeshEditorPathDeformAction<T>>> {};

} // namespace nijigenerate::core::translation
