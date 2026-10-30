#include "deformable.hpp"
#include "binding_add_remove_action.hpp"
#include "depthbone_publishers.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/ext/nodes/exgriddeformer.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/vertex/package.hpp>
#include <nodes/drawable.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::actions::deformable {
namespace {
ActionText retainFormatted(std::string text) {
    try {
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class T> void prepareDeformableActionOrigin() {
    using namespace nicxlive::core::d_runtime;
    using Names=SourceObjectTypeNames<T>;
    ngPrepareSourceTypeSpellings<T>();
    ngRegisterObjectTypeNames<T>(compat::RetainedString::fromStaticLiteral(Names::className),
        compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
}

void prepareDeformableActionClassInfo() {
    static const bool registered = [] {
        prepareDeformableActionOrigin<DeformableChangeAction>();
        prepareDeformableActionOrigin<GridDeformerDefineAction>();
        using nicxlive::core::d_runtime::ngRegisterObjectClassName;
        ngRegisterObjectClassName(typeid(DeformableChangeAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.deformable.DeformableChangeAction"));
        ngRegisterObjectClassName(typeid(GridDeformerDefineAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.deformable.GridDeformerDefineAction"));
        return true;
    }();
    (void)registered;
}
[[maybe_unused]] const bool deformableActionClassInfo = [] {
    prepareDeformableActionClassInfo();
    return true;
}();
}
// Object.toString, object.d (Digital Mars 2000-2011), BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void DeformableChangeAction::prepareSourceClassInfo() {
    prepareDeformableActionClassInfo();
}
compat::RetainedString DeformableChangeAction::toString() {
    const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
void GridDeformerDefineAction::prepareSourceClassInfo() {
    prepareDeformableActionClassInfo();
}
compat::RetainedString GridDeformerDefineAction::toString() {
    const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
namespace {

using Vec2 = nicxlive::core::math::Vec2;
using Vec2Array = nicxlive::core::math::Vec2Array;

// Bridge the existing retained AoS snapshot to the actual runtime SoA API.
Vec2Array snapshotVertices(compat::Slice<Vec2> vertices) {
    Vec2Array result(vertices.size());
    for (std::size_t i = 0; i < vertices.size(); ++i) result.set(i, vertices[i]);
    return result;
}

template<class T>
auto duplicateGridRows(nicxlive::core::common::SharedArray<nicxlive::core::common::SharedArray<T>> values) {
    decltype(values) result;
    result.resize(values.size());
    for (std::size_t x = 0; x < values.size(); ++x) {
        const auto row = values[x];
        // Use the actual element copy; no action-side value clone is supplied.
        result[x] = row.duplicate();
    }
    return result;
}
} // namespace

void DeformableChangeAction::copy(MeshData& source, MeshData& destination) {
    destination.vertices = source.vertices.duplicate();
    destination.uvs = source.uvs.duplicate();
    destination.indices = source.indices.duplicate();
    destination.origin = source.origin;
}

DeformableChangeAction::DeformableChangeAction(SourceAllocationTag) : GroupAction() {}

DeformableChangeAction::DeformableChangeAction(ActionText value,
        std::shared_ptr<Deformable> target) : DeformableChangeAction(SourceAllocationTag{}) {
    initializeSource(std::move(value), std::move(target));
}

void DeformableChangeAction::initializeSource(ActionText value, std::shared_ptr<Deformable> target) {
    name = std::move(value);
    self = std::move(target);
    undoable = true;
    state = captureState();
}

void DeformableChangeAction::updateNewState() {
    const auto receiver = weak_from_this().lock();
    notifyGeometryChanged();
}

// The original clear overrides intentionally have no side effects.
void DeformableChangeAction::clear() {}

void DeformableChangeAction::addBinding(std::shared_ptr<nicxlive::core::param::Parameter> parameter,
        std::shared_ptr<nicxlive::core::param::ParameterBinding> bindingValue) {
    const auto receiver = weak_from_this().lock();
    addAction(binding::ngCreateBindingAddRemoveAction<binding::ParameterBindingAddAction>(
        std::move(parameter), std::move(bindingValue)));
}

void DeformableChangeAction::rollback() {
    const auto receiver = weak_from_this().lock();
    if (undoable) {
        auto current = captureState();
        applyState(state);
        state = current;
        undoable = false;
    }
    GroupAction::rollback();
}

void DeformableChangeAction::redo() {
    const auto receiver = weak_from_this().lock();
    if (!undoable) {
        auto current = captureState();
        applyState(state);
        state = current;
        undoable = true;
    }
    GroupAction::redo();
}

ActionText DeformableChangeAction::describe() {
    const auto receiver = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Changed vertices of %s"));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(self)->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

ActionText DeformableChangeAction::describeUndo() {
    const auto receiver = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Vertices of %s was changed"));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(self)->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

ActionText DeformableChangeAction::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}

bool DeformableChangeAction::merge(ActionPtr) { return false; }
bool DeformableChangeAction::canMerge(ActionPtr) { return false; }

DeformableChangeAction::DeformableState DeformableChangeAction::captureState() {
    DeformableState result;
    result.vertices = compat::Slice<Vec2>(compat::ngRequireObject(self)->getVertices().toArray());
    if (auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(self)) {
        result.depths = mapped->copyDepths();
        result.hasDepths = true;
    }
    if (auto operated = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(self)) {
        result.depthOperations = operated->copyDepthOps();
        result.depthOperationBaseDepths = operated->copyDepthOpBaseDepths();
        result.hasDepthOperations = true;
    }
    return result;
}

void DeformableChangeAction::applyState(DeformableState& snapshot) {
    compat::ngRequireObject(self)->rebuffer(snapshotVertices(snapshot.vertices));
    if (snapshot.hasDepths) {
        if (auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(self))
            mapped->replaceDepths(snapshot.depths);
    }
    if (snapshot.hasDepthOperations) {
        if (auto operated = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(self)) {
            operated->replaceDepthOps(snapshot.depthOperations);
            operated->replaceDepthOpBaseDepths(snapshot.depthOperationBaseDepths);
        }
    }
    compat::ngRequireObject(self)->clearCache();
    viewport::vertex::ngRefreshDeformableCommandEditors(self);
    notifyGeometryChanged();
}

void DeformableChangeAction::notifyGeometryChanged() {
    depthboneinvalidation::ngNotifyDepthBoneTargetChanged(self,
        depthboneinvalidation::DepthBoneMutationKind::TargetGeometry,
        compat::RetainedString::fromStaticLiteral("Target Geometry"));
}

GridDeformerDefineAction::BindingValues GridDeformerDefineAction::dupValues(BindingValues values) {
    return duplicateGridRows(std::move(values));
}

GridDeformerDefineAction::BindingIsSet GridDeformerDefineAction::dupIsSet(BindingIsSet values) {
    return duplicateGridRows(std::move(values));
}

GridDeformerDefineAction::State GridDeformerDefineAction::captureState() {
    State result;
    result.vertices = compat::Slice<Vec2>(compat::ngRequireObject(self)->getVertices().toArray());
    if (auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(self)) {
        result.depths = mapped->copyDepths();
        result.hasDepths = true;
    }
    if (auto operated = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(self)) {
        result.depthOperations = operated->copyDepthOps();
        result.depthOperationBaseDepths = operated->copyDepthOpBaseDepths();
        result.hasDepthOperations = true;
    }
    const auto captureBinding = [&](std::shared_ptr<nicxlive::core::param::Parameter> parameter) {
        auto bindingValue = std::dynamic_pointer_cast<Binding>(compat::ngRequireObject(parameter)->getBinding(self,
            nicxlive::core::common::NullableString::fromStaticLiteral("deform")));
        if (!bindingValue) return;
        BindingState bindingState;
        bindingState.binding = bindingValue;
        bindingState.values = dupValues(bindingValue->values);
        using BindingStorage = nicxlive::core::param::ParameterBindingImpl<nicxlive::core::param::DeformSlot>;
        bindingState.isSet = dupIsSet(bindingValue->BindingStorage::getIsSet());
        result.bindings.append(bindingState);
    };
    const auto parameters = compat::ngRequireObject(project::incActivePuppet())->parameters;
    for (auto parameter : parameters) {
        if (auto group = std::dynamic_pointer_cast<ext::ExParameterGroup>(parameter)) {
            const auto children = group->children;
            for (auto child : children) captureBinding(child);
        } else captureBinding(parameter);
    }
    return result;
}

void GridDeformerDefineAction::applyState(State& snapshot) {
    compat::ngRequireObject(self)->rebuffer(snapshotVertices(snapshot.vertices));
    if (snapshot.hasDepths) {
        if (auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(self))
            mapped->replaceDepths(snapshot.depths);
    }
    if (snapshot.hasDepthOperations) {
        if (auto operated = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(self)) {
            operated->replaceDepthOps(snapshot.depthOperations);
            operated->replaceDepthOpBaseDepths(snapshot.depthOperationBaseDepths);
        }
    }
    const auto bindings = snapshot.bindings;
    for (auto bindingState : bindings) {
        const auto valuesTarget = bindingState.binding;
        auto values = dupValues(bindingState.values);
        if (!valuesTarget) compat::ngFailNullAccess(true);
        valuesTarget->values = std::move(values);
        using BindingStorage = nicxlive::core::param::ParameterBindingImpl<nicxlive::core::param::DeformSlot>;
        const auto flagsTarget = bindingState.binding;
        auto flags = dupIsSet(bindingState.isSet);
        if (!flagsTarget) compat::ngFailNullAccess(true);
        flagsTarget->BindingStorage::getIsSet() = std::move(flags);
        compat::ngRequireObject(bindingState.binding)->reInterpolate();
    }
    compat::ngRequireObject(project::incActivePuppet())->resetDrivers();
    compat::ngRequireObject(self)->clearCache();
    compat::ngRequireObject(self)->notifyChange(self, nicxlive::core::nodes::NotifyReason::StructureChanged);
    viewport::vertex::ngRefreshDeformableCommandEditors(self);
    notifyGeometryChanged();
}

GridDeformerDefineAction::GridDeformerDefineAction(SourceAllocationTag) {}

GridDeformerDefineAction::GridDeformerDefineAction(ActionText value,
        std::shared_ptr<Deformable> target) : GridDeformerDefineAction(SourceAllocationTag{}) {
    initializeSource(std::move(value), std::move(target));
}

void GridDeformerDefineAction::initializeSource(ActionText value, std::shared_ptr<Deformable> target) {
    name = std::move(value);
    self = std::move(target);
    oldState = captureState();
}

void GridDeformerDefineAction::updateNewState() {
    const auto receiver = weak_from_this().lock();
    if (auto operated = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(self)) {
        const auto oldVertices = snapshotVertices(oldState.vertices);
        const auto currentVertices = compat::ngRequireObject(self)->getVertices();
        const auto oldOperations = oldState.depthOperations;
        operated->replaceDepthOps(ext::nodes::ngRemapGridIndexBoundDepthOperations(
            oldVertices, currentVertices, oldOperations, false));
    }
    newState = captureState();
    notifyGeometryChanged();
}

void GridDeformerDefineAction::clear() {}
void GridDeformerDefineAction::rollback() {
    const auto receiver = weak_from_this().lock();
    applyState(oldState);
}
void GridDeformerDefineAction::redo() {
    const auto receiver = weak_from_this().lock();
    applyState(newState);
}

ActionText GridDeformerDefineAction::describe() {
    const auto receiver = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Changed grid of %s"));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(self)->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

ActionText GridDeformerDefineAction::describeUndo() {
    const auto receiver = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Grid of %s was changed"));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(self)->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

ActionText GridDeformerDefineAction::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}

bool GridDeformerDefineAction::merge(ActionPtr) { return false; }
bool GridDeformerDefineAction::canMerge(ActionPtr) { return false; }

void GridDeformerDefineAction::notifyGeometryChanged() {
    depthboneinvalidation::ngNotifyDepthBoneTargetChanged(self,
        depthboneinvalidation::DepthBoneMutationKind::TargetGeometry,
        compat::RetainedString::fromStaticLiteral("Target Geometry"));
}

} // namespace nijigenerate::actions::deformable
