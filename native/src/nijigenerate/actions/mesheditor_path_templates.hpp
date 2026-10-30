#pragma once
// Copyright 2020-2023,2022 Inochi2D Project; 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "mesheditor.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <math/veca.hpp>
#include <utility>

namespace nijigenerate::actions::mesheditor {
namespace detail {
// Forward to the existing concrete array conversion bodies without another provider.
nicxlive::core::math::Vec2Array ngMeshActionSoa2(compat::Slice<Vec2> values);
nicxlive::core::math::Vec3Array ngMeshActionSoa3(compat::Slice<Vec3> values);
using MeshPathEditor = viewport::model::mesheditor::drawable::IncMeshEditorOneFor<
    nicxlive::core::nodes::Drawable, project::EditMode::ModelEdit>;
std::shared_ptr<MeshPathEditor> ngMeshPathCastEditor(EditorRef editor);
nicxlive::core::math::Vec2Array& ngMeshPathDeformation(const std::shared_ptr<MeshPathEditor>& editor);
} // namespace detail

template<class T> std::shared_ptr<MeshEditorPathDeformAction<T>>
MeshEditorPathDeformAction<T>::create(NodeRef target, std::shared_ptr<T> action) {
    detail::ngPrepareMeshActionMetadata<MeshEditorPathDeformAction<T>>();
    auto result = std::allocate_shared<MeshEditorPathDeformAction<T>>(
        nicxlive::core::d_runtime::SourceAllocator<MeshEditorPathDeformAction<T>>{});
    result->constructMeshEditorPathOwned(target, action);
    return result;
}

template<class T> void MeshEditorPathDeformAction<T>::constructMeshEditorPathOwned(
        NodeRef target, std::shared_ptr<T> action) {
    const auto keepAlive = this->shared_from_this();
    this->initialize(target, action);
    captureOld();
}
template<class T> std::shared_ptr<typename MeshEditorPathDeformAction<T>::DeformEditor>
MeshEditorPathDeformAction<T>::selfDeform() {
    return detail::ngMeshPathCastEditor(this->self());
}
template<class T> std::shared_ptr<viewport::common::spline::CatmullSpline> MeshEditorPathDeformAction<T>::path() {
    const auto keepAlive = this->shared_from_this();
    if (this->self()) return compat::ngRequireObject(this->self())->getPath();
    return nullptr;
}
template<class T> void MeshEditorPathDeformAction<T>::captureOld() {
    const auto keepAlive = this->shared_from_this();
    if (path()) {
        oldPathPoints = compat::ngRequireObject(path())->points.duplicate();
        oldInitTangents = compat::Slice<Vec2>(compat::ngRequireObject(path())->initTangents.toArray()); hasOldInitTangents = true;
        oldRefOffsets = compat::Slice<Vec3>(compat::ngRequireObject(path())->refOffsets.toArray()); hasOldRefOffsets = true;
        oldOrigX = compat::ngRequireObject(path())->origX;
        oldOrigY = compat::ngRequireObject(path())->origY;
        oldOrigRotZ = compat::ngRequireObject(path())->origRotZ;
    } else {
        oldPathPoints = {}; oldInitTangents = {}; hasOldInitTangents = false;
        oldRefOffsets = {}; hasOldRefOffsets = false; oldOrigX = oldOrigY = oldOrigRotZ = 0;
    }
    if (this->self()) {
        oldSelected = compat::ngRequireObject(this->self())->selected;
        if (selfDeform()) {
            oldDeformation = compat::Slice<Vec2>(
                detail::ngMeshPathDeformation(compat::ngRequireObject(selfDeform())).toArray());
            hasOldDeformation = true;
        } else { oldDeformation = {}; hasOldDeformation = false; }
    } else { oldSelected = {}; oldDeformation = {}; hasOldDeformation = false; }
    if (path() && compat::ngRequireObject(path())->target)
        oldTargetPathPoints = compat::ngRequireObject(compat::ngRequireObject(path())->target)->points.duplicate();
    else oldTargetPathPoints = {};
}
template<class T> void MeshEditorPathDeformAction<T>::updateNewState() {
    const auto keepAlive = this->shared_from_this();
    MeshEditorAction<T>::updateNewState();
    if (path()) {
        newPathPoints = compat::ngRequireObject(path())->points.duplicate();
        newInitTangents = compat::Slice<Vec2>(compat::ngRequireObject(path())->initTangents.toArray());
        newOrigX = compat::ngRequireObject(path())->origX;
        newOrigY = compat::ngRequireObject(path())->origY;
        newOrigRotZ = compat::ngRequireObject(path())->origRotZ;
        newRefOffsets = compat::Slice<Vec3>(compat::ngRequireObject(path())->refOffsets.toArray());
    }
    if (this->self()) {
        newSelected = compat::ngRequireObject(this->self())->selected;
        if (selfDeform()) {
            newDeformation = compat::Slice<Vec2>(
                detail::ngMeshPathDeformation(compat::ngRequireObject(selfDeform())).toArray());
            hasNewDeformation = true;
        } else { newDeformation = {}; hasNewDeformation = false; }
    } else { newSelected = {}; newDeformation = {}; hasNewDeformation = false; }
    if (path() && compat::ngRequireObject(path())->target)
        newTargetPathPoints = compat::ngRequireObject(compat::ngRequireObject(path())->target)->points.duplicate();
}
template<class T> void MeshEditorPathDeformAction<T>::clear() {
    const auto keepAlive = this->shared_from_this();
    MeshEditorAction<T>::clear(); captureOld();
    newPathPoints = {}; newTargetPathPoints = {}; newDeformation = {}; hasNewDeformation = false;
}
template<class T> void MeshEditorPathDeformAction<T>::restorePathState(bool newer) {
    const auto keepAlive = this->shared_from_this();
    if (!this->isApplyable()) return;
    using Spline = viewport::common::spline::CatmullSpline;
    // Resolve the receiver before the RHS; a null field faults only at the store.
    const auto writePathField = [&](auto member, auto value) {
        const auto receiver = path();
        auto replacement = value();
        if (!receiver) compat::ngFailNullAccess(true);
        auto& destination = receiver.get()->*member;
        destination = std::move(replacement);
    };
    if (!(newer ? newPathPoints : oldPathPoints).isNull() &&
        (newer ? newPathPoints : oldPathPoints).size() && path()) {
        writePathField(&Spline::points, [&] { return (newer ? newPathPoints : oldPathPoints).duplicate(); });
        writePathField(&Spline::initTangents, [&] {
            return detail::ngMeshActionSoa2(newer ? newInitTangents : oldInitTangents);
        });
        writePathField(&Spline::refOffsets, [&] {
            return detail::ngMeshActionSoa3(newer ? newRefOffsets : oldRefOffsets);
        });
        writePathField(&Spline::origX, [&] { return newer ? newOrigX : oldOrigX; });
        writePathField(&Spline::origY, [&] { return newer ? newOrigY : oldOrigY; });
        writePathField(&Spline::origRotZ, [&] { return newer ? newOrigRotZ : oldOrigRotZ; });
        compat::ngRequireObject(path())->update();
    }
    if (!(newer ? newSelected : oldSelected).isNull()) {
        const auto receiver = this->self();
        auto replacement = (newer ? newSelected : oldSelected).duplicate();
        if (!receiver) compat::ngFailNullAccess(true);
        auto& destination = receiver->selected;
        destination = std::move(replacement);
    }
    if (newer ? hasNewDeformation : hasOldDeformation) {
        const auto receiver = selfDeform();
        auto replacement = detail::ngMeshActionSoa2(newer ? newDeformation : oldDeformation);
        if (!receiver) compat::ngFailNullAccess(true);
        auto& destination = detail::ngMeshPathDeformation(receiver);
        destination = std::move(replacement);
    }
    if (!(newer ? newTargetPathPoints : oldTargetPathPoints).isNull() &&
        (newer ? newTargetPathPoints : oldTargetPathPoints).size() &&
        path() && compat::ngRequireObject(path())->target) {
        const auto receiver = compat::ngRequireObject(path())->target;
        auto replacement = (newer ? newTargetPathPoints : oldTargetPathPoints).duplicate();
        if (!receiver) compat::ngFailNullAccess(true);
        auto& destination = receiver->points;
        destination = std::move(replacement);
        compat::ngRequireObject(compat::ngRequireObject(path())->target)->update();
    }
}
template<class T> void MeshEditorPathDeformAction<T>::rollback() {
    const auto keepAlive = this->shared_from_this();
    MeshEditorAction<T>::rollback(); restorePathState(false);
}
template<class T> void MeshEditorPathDeformAction<T>::redo() {
    const auto keepAlive = this->shared_from_this();
    MeshEditorAction<T>::redo(); restorePathState(true);
}
} // namespace nijigenerate::actions::mesheditor

#include <nijigenerate/viewport/model/mesheditor/drawable.hpp>
