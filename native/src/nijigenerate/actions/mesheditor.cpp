#include "mesheditor.hpp"
#include "depthbone_publishers.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/viewport/common/mesheditor/package.hpp>
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/onetimedeform.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/viewport/model/mesheditor/drawable.hpp>
#include <resource.hpp>
#include <compat/object_metadata.hpp>
#include <algorithm>
#include <array>
#include <type_traits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>

namespace nijigenerate::actions::mesheditor {
namespace one = viewport::common::mesheditor::tools::onetimedeform;
namespace impl = viewport::common::mesheditor::operations::impl;
using nicxlive::core::math::Vec2Array;
using nicxlive::core::math::Vec3Array;
using nicxlive::core::param::ParamMergeMode;
using project::incArmedParameter;
namespace {
using BindingStorage = nicxlive::core::param::ParameterBindingImpl<nicxlive::core::param::DeformSlot>;
EditorRef findEditor(const NodeRef& target) {
    auto editor = viewport::model::deform::ngViewportModelDeformGetEditor();
    // Bind the live field, then copy each source argument only when its call is
    // reached: both callback-bearing lookups can change the action's target.
    auto result = editor ? editor->getEditorFor(NodeRef(target)) : nullptr;
    if (!result) result = one::ngGetArmedParameterEditorFor(NodeRef(target));
    return result;
}
ActionText retainFormatted(std::string value) {
    try {
        return compat::RetainedString(std::move(value));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<std::size_t N,class... T> ActionText text(const char (&key)[N], const T&... values) {
    const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
    const std::array<core::translation::TypedFormatArgument, sizeof...(T)> args{
        core::translation::ngFormatArgument(values)...};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), args));
}
template<class Array, class Value> Array soa(compat::Slice<Value> values) {
    Array result(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) result.set(i, values[i]);
    return result;
}
Vec2u closest(ParameterRef parameter) { return parameter->findClosestKeypoint(); }
template<class Array> decltype(auto) sourceMeshActionElement(Array& values, std::size_t index,
                                                            std::size_t sourceLine) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
        index, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/mesheditor.d"), sourceLine);
    return values[index];
}
template<class Matrix> decltype(auto) sourceMeshActionCell(Matrix& values, Vec2u point,
                                                         std::size_t sourceLine) {
    auto& row = sourceMeshActionElement(values, point.x, sourceLine);
    return sourceMeshActionElement(row, point.y, sourceLine);
}
}

namespace detail {
EditorRef ngMeshActionFindEditor(const NodeRef& target) { return findEditor(target); }
Vec2u ngMeshActionClosest(ParameterRef parameter) { return closest(std::move(parameter)); }
std::optional<SubToolMode> ngMeshActionOneTimeMode(const NodeRef& current) {
    return one::ngOneTimeDeformMode(impl::ngGetEditorFor(current));
}
void ngMeshActionRestoreMode(const NodeRef& current, SubToolMode mode) {
    one::ngRestoreOneTimeDeformMode(impl::ngGetEditorFor(current), mode);
}
Vec2Array ngMeshActionSoa2(compat::Slice<Vec2> values) { return soa<Vec2Array>(std::move(values)); }
Vec3Array ngMeshActionSoa3(compat::Slice<Vec3> values) { return soa<Vec3Array>(std::move(values)); }
std::shared_ptr<MeshPathEditor> ngMeshPathCastEditor(EditorRef editor) {
    return std::dynamic_pointer_cast<MeshPathEditor>(std::move(editor));
}
Vec2Array& ngMeshPathDeformation(const std::shared_ptr<MeshPathEditor>& editor) {
    return editor->deformation;
}
} // namespace detail

std::shared_ptr<DeformationAction> DeformationAction::create(ActionText name, NodeRef target,
                                                            compat::SharedDelegate<void()> update) {
    detail::ngPrepareMeshActionMetadata<DeformationAction>();
    auto result = std::allocate_shared<DeformationAction>(
        nicxlive::core::d_runtime::SourceAllocator<DeformationAction>{});
    result->constructDeformationOwned(name, target, update);
    return result;
}
void DeformationAction::constructDeformationOwned(ActionText name, NodeRef target,
                                                 compat::SharedDelegate<void()> update) {
    const auto keepAlive = shared_from_this();
    this->name = name;
    this->target = target;
    bindingAdded = false;
    clear();
    if (update) { update(); updateNewState(); }
}
compat::RetainedString DeformationAction::toString() {
    const auto keepAlive = weak_from_this().lock();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
EditorRef DeformationAction::self() {
    const auto keepAlive = shared_from_this();
    return findEditor(target);
}
void DeformationAction::resolveCurrentFilter() {
    const auto keepAlive = shared_from_this();
    if (auto filter = std::dynamic_pointer_cast<nicxlive::core::nodes::NodeFilter>(target)) {
        if (auto current = one::ngCurrentNodeFilter())
            target = std::dynamic_pointer_cast<nicxlive::core::nodes::Node>(current);
    }
}
void DeformationAction::notifyChanged() {
    const auto keepAlive = shared_from_this();
    if (auto current = target) current->notifyChange(current, nicxlive::core::nodes::NotifyReason::AttributeChanged);
    if (deform && param) depthboneinvalidation::ngNotifyDepthBoneBindingValueChanged(deform, {keypoint.x, keypoint.y});
}
void DeformationAction::updateNewState() {
    const auto keepAlive = shared_from_this();
    resolveCurrentFilter();
    if (param) {
        auto current = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(
            compat::ngRequireObject(param)->getBinding(target,
                nicxlive::core::common::NullableString::fromStaticLiteral("deform")));
        if (!deform && current) bindingAdded = true;
        deform = current;
    }
    notifyChanged();
}
void DeformationAction::clear() {
    const auto keepAlive = shared_from_this();
    resolveCurrentFilter();
    if (!self()) {
        target.reset(); param.reset(); deform.reset(); bindingAdded = false;
        dirty = false; vertices = {}; isSet = false;
    } else {
        param = incArmedParameter();
        keypoint = compat::ngRequireObject(param)->findClosestKeypoint();
        vertices = compat::Slice<Vec2>(compat::ngRequireObject(self())->getOffsets().toArray());
        deform = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(
            compat::ngRequireObject(param)->getBinding(target,
                nicxlive::core::common::NullableString::fromStaticLiteral("deform")));
        bindingAdded = false;
    }
    if (deform) {
        auto& flags = deform->BindingStorage::getIsSet();
        auto& row = sourceMeshActionElement(flags, keypoint.x, 117);
        isSet = sourceMeshActionElement(row, keypoint.y, 117);
    }
    dirty = false;
}
bool DeformationAction::isApplyable() {
    const auto keepAlive = shared_from_this();
    if (!self()) return false;
    const auto editorTarget = compat::ngRequireObject(self())->getTarget();
    const auto currentTarget = target;
    if (!nicxlive::core::ngResourceObjectEquals(editorTarget, currentTarget)) return false;
    const auto armedParameter = incArmedParameter();
    const auto currentParameter = param;
    if (!nicxlive::core::ngResourceObjectEquals(armedParameter, currentParameter)) return false;
    const auto current = compat::ngRequireObject(incArmedParameter())->findClosestKeypoint();
    return current.x == keypoint.x && current.y == keypoint.y;
}
void DeformationAction::exchange(bool forward) {
    const auto keepAlive = shared_from_this();
    resolveCurrentFilter();
    if (undoable == forward) return;
    if (!vertices.empty()) {
        if (deform) {
            auto temporaryVertices = vertices;
            const bool temporaryIsSet = isSet;
            vertices = compat::Slice<Vec2>(sourceMeshActionCell(
                compat::ngRequireObject(deform)->values, keypoint, forward ? 177 : 137).vertexOffsets.toArray());
            isSet = sourceMeshActionCell(compat::ngRequireObject(deform)->BindingStorage::getIsSet(),
                keypoint, forward ? 178 : 138);
            {
                const auto receiver = compat::ngRequireObject(deform);
                const auto point = keypoint;
                receiver->update(point, soa<Vec2Array>(temporaryVertices));
            }
            sourceMeshActionCell(compat::ngRequireObject(deform)->BindingStorage::getIsSet(),
                keypoint, forward ? 180 : 140) = temporaryIsSet;
            compat::ngRequireObject(deform)->reInterpolate();
            if (bindingAdded) {
                const auto receiver = compat::ngRequireObject(param);
                const auto binding = deform;
                if (forward) receiver->addBinding(binding);
                else receiver->removeBinding(binding);
            }
        }
        const bool hasSameTarget = [&] {
            if (!self()) return false;
            const auto editorTarget = compat::ngRequireObject(self())->getTarget();
            const auto currentTarget = target;
            return nicxlive::core::ngResourceObjectEquals(editorTarget, currentTarget);
        }();
        if (hasSameTarget) {
            compat::ngRequireObject(self())->resetMesh();
            if (deform) {
                auto offsets = sourceMeshActionCell(compat::ngRequireObject(deform)->values,
                    keypoint, forward ? 189 : 149).vertexOffsets.dup();
                const auto length = compat::ngRequireObject(self())->getOffsets().size();
                if (offsets.size() != length) {
                    Vec2Array resized(length);
                    const auto n = std::min(offsets.size(), length);
                    for (std::size_t i = 0; i < n; ++i) resized.set(i, offsets.at(i));
                    for (std::size_t i = n; i < length; ++i) resized.set(i, {0, 0});
                    offsets = resized;
                }
                compat::ngRequireObject(self())->applyOffsets(offsets);
            }
        }
        notifyChanged();
    }
    undoable = forward;
}
void DeformationAction::rollback() { exchange(false); }
void DeformationAction::redo() { exchange(true); }
ActionText DeformationAction::describe() {
    const auto keepAlive = shared_from_this();
    return text("%s->Edited deformation of %s.", "deform", name);
}
ActionText DeformationAction::describeUndo() {
    const auto keepAlive = shared_from_this();
    return text("%s->deformation of %s was edited.", "deform", name);
}
ActionText DeformationAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
bool DeformationAction::merge(ActionPtr other) {
    const auto keepAlive = shared_from_this();
    return canMerge(other);
}


SubToolModeChangeAction::SubToolModeChangeAction(compat::Slice<NodeRef> targets,
    compat::Slice<SubToolMode> oldModes, compat::Slice<SubToolMode> newModes)
    : targets(targets.duplicate()), oldModes(oldModes.duplicate()), newModes(newModes.duplicate()) {
    detail::ngPrepareMeshActionMetadata<SubToolModeChangeAction>();
}
compat::RetainedString SubToolModeChangeAction::toString() {
    const auto keepAlive = weak_from_this().lock();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
void SubToolModeChangeAction::rollback() {
    const auto keepAlive = shared_from_this();
    const auto nodes = targets;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        auto editor = impl::ngGetEditorFor(nodes[i]);
        if (!editor) continue;
        const auto tool = std::dynamic_pointer_cast<one::OneTimeDeformBase>(editor->getTool());
        if (!tool) continue;
        if (i < oldModes.size()) tool->mode = oldModes[i];
    }
}
void SubToolModeChangeAction::redo() {
    const auto keepAlive = shared_from_this();
    const auto nodes = targets;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        auto editor = impl::ngGetEditorFor(nodes[i]);
        if (!editor) continue;
        const auto tool = std::dynamic_pointer_cast<one::OneTimeDeformBase>(editor->getTool());
        if (!tool) continue;
        if (i < newModes.size()) tool->mode = newModes[i];
    }
}
ActionText SubToolModeChangeAction::describe() {
    const auto keepAlive = weak_from_this().lock();
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Switched deform sub-tool mode"));
}
ActionText SubToolModeChangeAction::describeUndo() {
    const auto keepAlive = weak_from_this().lock();
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Reverted deform sub-tool mode"));
}
ActionText SubToolModeChangeAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
template class MeshEditorAction<DeformationAction>;
template class MeshEditorAction<GroupAction>;
template class MeshEditorPathDeformAction<DeformationAction>;
template class MeshEditorPathDeformAction<GroupAction>;
}
