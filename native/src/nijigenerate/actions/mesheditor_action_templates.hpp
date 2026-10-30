#pragma once
// Copyright 2020-2023,2022 Inochi2D Project; 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
// The same generic method bodies are visible to source-valid instantiations.
#include "mesheditor.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <resource.hpp>
#include <optional>
#include <utility>

namespace nijigenerate::actions::mesheditor {
namespace detail {
// These typed entries call the existing implementation's genuine helpers.
EditorRef ngMeshActionFindEditor(const NodeRef& target);
Vec2u ngMeshActionClosest(ParameterRef parameter);
std::optional<SubToolMode> ngMeshActionOneTimeMode(const NodeRef& current);
void ngMeshActionRestoreMode(const NodeRef& current, SubToolMode mode);
} // namespace detail

template<class T> std::shared_ptr<MeshEditorAction<T>> MeshEditorAction<T>::create(NodeRef target,
                                                                                std::shared_ptr<T> action) {
    detail::ngPrepareMeshActionMetadata<MeshEditorAction<T>>();
    auto result = std::allocate_shared<MeshEditorAction<T>>(
        nicxlive::core::d_runtime::SourceAllocator<MeshEditorAction<T>>{});
    result->initialize(target, action);
    return result;
}
template<class T> void MeshEditorAction<T>::initialize(NodeRef value, std::shared_ptr<T> nested) {
    target = value;
    clear();
    action = nested;
    oldSubToolModes = captureSubToolModes();
}

template<class T> compat::RetainedString MeshEditorAction<T>::toString() {
    const auto keepAlive = this->weak_from_this().lock();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
template<class T> EditorRef MeshEditorAction<T>::self() {
    const auto keepAlive = this->shared_from_this();
    return detail::ngMeshActionFindEditor(target);
}
template<class T> compat::Slice<typename MeshEditorAction<T>::SubToolModeSnapshot>
MeshEditorAction<T>::captureSubToolModes() {
    const auto keepAlive = this->shared_from_this();
    compat::Slice<SubToolModeSnapshot> modes;
    if (!self()) return modes;
    auto targets = compat::ngRequireObject(self())->getFilterTargets();
    for (auto current : targets) {
        if (auto mode = detail::ngMeshActionOneTimeMode(current)) modes.append({current, *mode});
    }
    return modes;
}
template<class T> void MeshEditorAction<T>::restoreSubToolModes(compat::Slice<SubToolModeSnapshot> modes) {
    const auto keepAlive = this->shared_from_this();
    if (!self()) return;
    auto targets = compat::ngRequireObject(self())->getFilterTargets();
    for (auto current : targets) for (auto snapshot : modes) {
        if (snapshot.target != current) continue;
        detail::ngMeshActionRestoreMode(current, snapshot.mode);
        break;
    }
}
template<class T> void MeshEditorAction<T>::updateNewState() {
    const auto keepAlive = this->shared_from_this();
    if (auto lazy = std::dynamic_pointer_cast<LazyBoundAction>(action)) lazy->updateNewState();
    if (self()) newKeypoint = detail::ngMeshActionClosest(compat::ngRequireObject(param));
    newSubToolModes = captureSubToolModes();
}
template<class T> void MeshEditorAction<T>::clear() {
    const auto keepAlive = this->shared_from_this();
    if (!self()) { target.reset(); param.reset(); }
    else {
        param = project::incArmedParameter();
        oldKeypoint = detail::ngMeshActionClosest(compat::ngRequireObject(param));
        oldEditorTransform = compat::ngRequireObject(self())->transform;
    }
    if (auto lazy = std::dynamic_pointer_cast<LazyBoundAction>(action)) lazy->clear();
}
template<class T> bool MeshEditorAction<T>::isApplyable() {
    const auto keepAlive = this->shared_from_this();
    if (!self()) return false;
    const auto receiver = compat::ngRequireObject(self());
    const auto current = receiver->getTarget();
    const auto expected = target;
    return nicxlive::core::ngResourceObjectEquals(current, expected);
}
template<class T> void MeshEditorAction<T>::rollback() {
    const auto keepAlive = this->shared_from_this();
    if (action) {
        if (self()) {
            compat::ngRequireObject(param)->pushIOffset(
                compat::ngRequireObject(param)->getKeypointValue(newKeypoint), nicxlive::core::param::ParamMergeMode::Forced);
        }
        compat::ngRequireObject(action)->rollback();
        if (isApplyable()) {
            const auto receiver = self();
            if (!receiver) compat::ngFailNullAccess(true);
            auto& destination = receiver->transform;
            destination = oldEditorTransform;
        }
        if (self()) {
            compat::ngRequireObject(param)->pushIOffset(
                compat::ngRequireObject(param)->getKeypointValue(oldKeypoint), nicxlive::core::param::ParamMergeMode::Forced);
            compat::ngRequireObject(self())->forceResetAction();
        }
        restoreSubToolModes(oldSubToolModes);
    }
}
template<class T> void MeshEditorAction<T>::redo() {
    const auto keepAlive = this->shared_from_this();
    if (auto nested = action) {
        nested->redo();
        if (self()) {
            compat::ngRequireObject(param)->pushIOffset(
                compat::ngRequireObject(param)->getKeypointValue(oldKeypoint), nicxlive::core::param::ParamMergeMode::Forced);
        }
        if (isApplyable()) {
            const auto receiver = self();
            if (!receiver) compat::ngFailNullAccess(true);
            auto& destination = receiver->transform;
            destination = newEditorTransform;
        }
        if (self()) {
            compat::ngRequireObject(param)->pushIOffset(
                compat::ngRequireObject(param)->getKeypointValue(newKeypoint), nicxlive::core::param::ParamMergeMode::Forced);
            compat::ngRequireObject(self())->forceResetAction();
        }
        restoreSubToolModes(newSubToolModes);
    }
}
template<class T> ActionText MeshEditorAction<T>::describe() {
    [[maybe_unused]] const auto keepAlive = this->shared_from_this();
    if (auto nested = action) return nested->describe();
    return compat::RetainedString::emptyLiteral();
}
template<class T> ActionText MeshEditorAction<T>::describeUndo() {
    [[maybe_unused]] const auto keepAlive = this->shared_from_this();
    if (auto nested = action) return nested->describeUndo();
    return compat::RetainedString::emptyLiteral();
}
template<class T> ActionText MeshEditorAction<T>::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
template<class T> bool MeshEditorAction<T>::merge(ActionPtr other) {
    [[maybe_unused]] const auto keepAlive = this->shared_from_this();
    if (auto nested = action) return nested->merge(other);
    return false;
}
template<class T> bool MeshEditorAction<T>::canMerge(ActionPtr other) {
    [[maybe_unused]] const auto keepAlive = this->shared_from_this();
    if (auto nested = action) return nested->canMerge(other);
    return false;
}

} // namespace nijigenerate::actions::mesheditor
