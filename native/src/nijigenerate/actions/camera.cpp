/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    Authors: Luna Nielsen
*/
#include "camera.hpp"
#include "nijigenerate/ext/nodes/excamera.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/core/typed_format.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include <nodes/node.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>

namespace nijigenerate::actions::camera {
using nicxlive::core::nodes::NotifyReason;

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

void prepareCameraActionClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names=SourceObjectTypeNames<CameraResizeAction>;
        ngPrepareSourceTypeSpellings<CameraResizeAction>();
        ngRegisterObjectTypeNames<CameraResizeAction>(
            compat::RetainedString::fromStaticLiteral(Names::className),
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(CameraResizeAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.camera.CameraResizeAction"));
        return true;
    }();
    (void)registered;
}
[[maybe_unused]] const bool cameraActionClassInfo = [] {
    prepareCameraActionClassInfo();
    return true;
}();
}
// Object.toString, object.d (Digital Mars 2000-2011), BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString CameraResizeAction::toString() {
    const auto receiver=weak_from_this().lock();
    prepareCameraActionClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}

CameraResizeAction::CameraResizeAction(SourceAllocationTag) {
    prepareCameraActionClassInfo();
}
CameraResizeAction::CameraResizeAction(std::shared_ptr<ext::nodes::ExCamera> camera,
    Transform oldTransform, Vec2 oldViewport, Transform newTransform, Vec2 newViewport,
    Children children, ChildTransforms oldChildTransforms, ChildTransforms newChildTransforms)
    : CameraResizeAction(SourceAllocationTag{}) {
    initializeSource(std::move(camera),oldTransform,oldViewport,newTransform,newViewport,
        std::move(children),std::move(oldChildTransforms),std::move(newChildTransforms));
}

void CameraResizeAction::initializeSource(std::shared_ptr<ext::nodes::ExCamera> camera,
    Transform oldTransform, Vec2 oldViewport, Transform newTransform, Vec2 newViewport,
    Children children, ChildTransforms oldChildTransforms, ChildTransforms newChildTransforms) {
    this->camera = camera;
    this->oldTransform = oldTransform;
    this->oldViewport = oldViewport;
    this->newTransform = newTransform;
    this->newViewport = newViewport;
    this->children = children;
    this->oldChildTransforms = oldChildTransforms;
    this->newChildTransforms = newChildTransforms;
    notify();
}

void CameraResizeAction::apply(Transform transform, Vec2 viewport, ChildTransforms childTransforms) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(camera)->localTransform = transform;
    compat::ngRequireObject(camera)->setViewport(viewport);
    compat::ngRequireObject(camera)->transformChanged();
    const auto capturedChildren = children;
    for (std::size_t index = 0; index < capturedChildren.size(); ++index) {
        const auto child = capturedChildren[index];
        const auto found = childTransforms.find(compat::ngRequireObject(child)->getUUID());
        if (found != childTransforms.end()) {
            child->localTransform = found->second;
            child->localTransform.update();
            child->transformChanged();
            child->notifyChange(child, NotifyReason::AttributeChanged);
            child->notifyChange(child, NotifyReason::Transformed);
        }
    }
    notify();
}

void CameraResizeAction::notify() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(camera)->notifyChange(camera, NotifyReason::AttributeChanged);
    compat::ngRequireObject(camera)->notifyChange(camera, NotifyReason::Transformed);
}

void CameraResizeAction::rollback() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    apply(oldTransform, oldViewport, oldChildTransforms);
}
void CameraResizeAction::redo() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    apply(newTransform, newViewport, newChildTransforms);
}

ActionText CameraResizeAction::describe() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Resized camera %s"));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(camera)->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

ActionText CameraResizeAction::describeUndo() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Undo resize camera %s"));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(compat::ngRequireObject(camera)->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

ActionText CameraResizeAction::getName() {
    return compat::RetainedString::fromStaticLiteral("CameraResize");
}

bool CameraResizeAction::merge(ActionPtr other) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (!canMerge(other)) return false;
    const auto resize = compat::ngRequireObject(std::dynamic_pointer_cast<CameraResizeAction>(other));
    newTransform = resize->newTransform;
    newViewport = resize->newViewport;
    children = resize->children;
    newChildTransforms = resize->newChildTransforms;
    return true;
}

bool CameraResizeAction::canMerge(ActionPtr other) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    const auto resize = std::dynamic_pointer_cast<CameraResizeAction>(other);
    if (!resize) return false;
    const auto otherUuid = compat::ngRequireObject(resize->camera)->getUUID();
    return otherUuid == compat::ngRequireObject(camera)->getUUID();
}

} // namespace nijigenerate::actions::camera
