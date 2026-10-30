// Copyright 2026, nijigenerate Project. Distributed under the 2-Clause BSD License.
#include "depthbone.hpp"
#include "depthbone_publishers.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <utility>

namespace nijigenerate::actions::depthbone {
using nicxlive::core::nodes::NotifyReason;
using depthboneinvalidation::ngNotifyDepthBoneRigChanged;
namespace {
template<class T> void prepareDepthBoneActionOrigin() {
    using namespace nicxlive::core::d_runtime;
    using Names=SourceObjectTypeNames<T>;
    ngPrepareSourceTypeSpellings<T>();
    ngRegisterObjectTypeNames<T>(compat::RetainedString::fromStaticLiteral(Names::className),
        compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
}

void prepareDepthBoneActionClassInfo() {
    static const bool registered = [] {
        prepareDepthBoneActionOrigin<DepthRigBindingsChangeAction>();
        prepareDepthBoneActionOrigin<DepthBoneFitZChangeAction>();
        prepareDepthBoneActionOrigin<DepthBoneRestChangeAction>();
        prepareDepthBoneActionOrigin<DepthBoneConstraintChangeAction>();
        using nicxlive::core::d_runtime::ngRegisterObjectClassName;
        ngRegisterObjectClassName(typeid(DepthRigBindingsChangeAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.depthbone.DepthRigBindingsChangeAction"));
        ngRegisterObjectClassName(typeid(DepthBoneFitZChangeAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.depthbone.DepthBoneFitZChangeAction"));
        ngRegisterObjectClassName(typeid(DepthBoneRestChangeAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.depthbone.DepthBoneRestChangeAction"));
        ngRegisterObjectClassName(typeid(DepthBoneConstraintChangeAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.depthbone.DepthBoneConstraintChangeAction"));
        return true;
    }();
    (void)registered;
}
[[maybe_unused]] const bool depthBoneActionClassInfo = [] {
    prepareDepthBoneActionClassInfo();
    return true;
}();
}
// Object.toString, object.d (Digital Mars 2000-2011), BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString DepthRigBindingsChangeAction::toString() {
    const auto receiverLifetime = weak_from_this().lock();
    prepareDepthBoneActionClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
compat::RetainedString DepthBoneFitZChangeAction::toString() {
    const auto receiverLifetime = weak_from_this().lock();
    prepareDepthBoneActionClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
compat::RetainedString DepthBoneRestChangeAction::toString() {
    const auto receiverLifetime = weak_from_this().lock();
    prepareDepthBoneActionClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
compat::RetainedString DepthBoneConstraintChangeAction::toString() {
    const auto receiverLifetime = weak_from_this().lock();
    prepareDepthBoneActionClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
namespace {
template<std::size_t N> ActionText literal(const char (&text)[N]) {
    return compat::RetainedString::fromStaticLiteral(text);
}
template<std::size_t N> ActionText translated(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}

// All callers copy actual data records, scalar lanes or class handles. Their
// copies allocate no user objects and invoke no Source postblit or callback.
template<class T> compat::Slice<T> duplicateDepthBoneArray(const compat::Slice<T>& values) {
    try {
        return values.duplicate();
    } catch (const compat::OutOfMemoryError&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"),394);
    }
}
}

Bindings ngCopyDepthRigBindings(Bindings bindings) {
    auto result = duplicateDepthBoneArray(bindings);
    for (std::size_t i = 0; i < result.size(); ++i) {
        auto& binding = result[i];
        binding.sourceBoneUuids = duplicateDepthBoneArray(binding.sourceBoneUuids);
        binding.sourceSettings = duplicateDepthBoneArray(binding.sourceSettings);
        ext::nodes::DepthUuidFloatMap copied;
        for (const auto& [uuid, value] : binding.influenceRule.multipliersByBoneUuid.entries())
            copied[uuid] = value;
        binding.influenceRule.multipliersByBoneUuid = std::move(copied);
    }
    return result;
}

DepthRigBindingsChangeAction::DepthRigBindingsChangeAction(ActionText label,
        std::shared_ptr<ExDepthRigRoot> root, Bindings oldBindings, Bindings newBindings,
        bool settleBeforeDispatch, std::shared_ptr<Node> affectedTarget,
        std::uint64_t mergeSession, std::uint64_t mergeSourceUuid, ActionText mergeProperty) {
    prepareDepthBoneActionClassInfo();
    // The source writes label/root before either allocating snapshot copy.
    // Member declaration order must not silently reorder these operations.
    this->label = std::move(label);
    this->root = std::move(root);
    this->oldBindings = ngCopyDepthRigBindings(oldBindings);
    this->newBindings = ngCopyDepthRigBindings(newBindings);
    this->settleBeforeDispatch = settleBeforeDispatch;
    this->affectedTarget = std::move(affectedTarget);
    this->mergeSession = mergeSession;
    this->mergeSourceUuid = mergeSourceUuid;
    this->mergeProperty = std::move(mergeProperty);
    notifyChanged();
}
void DepthRigBindingsChangeAction::notifyChanged() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    compat::ngRequireObject(root)->notifyChange(root, NotifyReason::AttributeChanged);
    ngNotifyDepthBoneRigChanged(root, label, settleBeforeDispatch, affectedTarget);
}
void DepthRigBindingsChangeAction::rollback() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto receiver = compat::ngRequireObject(root);
    receiver->bindings = ngCopyDepthRigBindings(oldBindings);
    notifyChanged();
}
void DepthRigBindingsChangeAction::redo() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto receiver = compat::ngRequireObject(root);
    receiver->bindings = ngCopyDepthRigBindings(newBindings);
    notifyChanged();
}
bool DepthRigBindingsChangeAction::merge(ActionPtr other) {
    // A virtual predicate can remove the caller's final history-owned handle.
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto next = std::dynamic_pointer_cast<DepthRigBindingsChangeAction>(other);
    if (!canMerge(next)) return false;
    compat::ngRequireObject(next);
    newBindings = ngCopyDepthRigBindings(next->newBindings);
    label = next->label;
    settleBeforeDispatch = settleBeforeDispatch || next->settleBeforeDispatch;
    return true;
}
bool DepthRigBindingsChangeAction::canMerge(ActionPtr other) {
    const auto next = std::dynamic_pointer_cast<DepthRigBindingsChangeAction>(other);
    return next && mergeSession != 0 && mergeSession == next->mergeSession && root == next->root &&
        affectedTarget == next->affectedTarget && mergeSourceUuid == next->mergeSourceUuid &&
        mergeProperty == next->mergeProperty;
}

DepthBoneFitZChangeAction::DepthBoneFitZChangeAction(std::shared_ptr<ExDepthRigRoot> root,
        compat::Slice<std::shared_ptr<ExDepthBone>> bones, compat::Slice<float> oldValues,
        compat::Slice<float> newValues) {
    prepareDepthBoneActionClassInfo();
#ifndef NDEBUG
    // Unmessaged D assertions use AssertError("Assertion failure"), rather
    // than C assert's process termination or an ordinary native Exception.
    if (!root) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Assertion failure"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/depthbone.d"), 117);
    if (bones.size() != oldValues.size())
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/depthbone.d"), 118);
    if (bones.size() != newValues.size())
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/depthbone.d"), 119);
#endif
    this->root = std::move(root);
    this->bones = duplicateDepthBoneArray(bones);
    this->oldValues = duplicateDepthBoneArray(oldValues);
    this->newValues = duplicateDepthBoneArray(newValues);
    notifyChanged();
}
void DepthBoneFitZChangeAction::notifyChanged() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    // D foreach retains one shallow descriptor and binds each handle by value.
    // Callbacks may rebind the member or edit slots of the original backing.
    const auto iterationBones = bones;
    for (std::size_t i = 0; i < iterationBones.size(); ++i)
        if (const auto bone = iterationBones[i]) bone->notifyChange(bone, NotifyReason::AttributeChanged);
    ngNotifyDepthBoneRigChanged(root, literal("Fit Z to Depth"));
}
void DepthBoneFitZChangeAction::apply(compat::Slice<float> values) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto iterationBones = bones;
    for (std::size_t i = 0; i < iterationBones.size(); ++i) {
        const auto bone = iterationBones[i];
        if (!bone) continue;
        if (i >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/depthbone.d"), 138);
        bone->localTransform.translation.z = values.data()[i];
        bone->localTransform.update();
        bone->transformChanged();
    }
    notifyChanged();
}
void DepthBoneFitZChangeAction::rollback() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    apply(oldValues);
}
void DepthBoneFitZChangeAction::redo() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    apply(newValues);
}
ActionText DepthBoneFitZChangeAction::describe() {
    const auto receiverLifetime = weak_from_this().lock();
    return translated("Fit Z to Depth");
}
ActionText DepthBoneFitZChangeAction::describeUndo() {
    const auto receiverLifetime = weak_from_this().lock();
    return translated("Fit Z to Depth");
}
ActionText DepthBoneFitZChangeAction::getName() { return literal("DepthBoneFitZChangeAction"); }

DepthBoneRestChangeAction::DepthBoneRestChangeAction(std::shared_ptr<ExDepthBone> bone,
        Vec3 oldHead, Vec3 oldTail, float oldRoll, Vec3 newHead, Vec3 newTail, float newRoll)
    : bone(std::move(bone)), oldHead(oldHead), oldTail(oldTail), oldRoll(oldRoll),
      newHead(newHead), newTail(newTail), newRoll(newRoll) {
    prepareDepthBoneActionClassInfo();
    notifyChanged();
}
void DepthBoneRestChangeAction::notifyChanged() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    compat::ngRequireObject(bone)->notifyChange(bone, NotifyReason::AttributeChanged);
    ngNotifyDepthBoneRigChanged(bone, literal("Depth Bone Rest"));
}
void DepthBoneRestChangeAction::apply(Vec3 head, Vec3 tail, float roll) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    compat::ngRequireObject(bone)->restHead = head;
    compat::ngRequireObject(bone)->restTail = tail;
    compat::ngRequireObject(bone)->restRoll = roll;
    notifyChanged();
}
void DepthBoneRestChangeAction::rollback() {
    // Keep the receiver alive even when a derived apply removes the history entry.
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    apply(oldHead, oldTail, oldRoll);
}
void DepthBoneRestChangeAction::redo() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    apply(newHead, newTail, newRoll);
}
ActionText DepthBoneRestChangeAction::describe() {
    const auto receiverLifetime = weak_from_this().lock();
    return translated("Depth bone rest changed");
}
ActionText DepthBoneRestChangeAction::describeUndo() {
    const auto receiverLifetime = weak_from_this().lock();
    return translated("Depth bone rest changed");
}
ActionText DepthBoneRestChangeAction::getName() { return literal("DepthBoneRestChangeAction"); }

DepthBoneConstraintChangeAction::DepthBoneConstraintChangeAction() {
    prepareDepthBoneActionClassInfo();
}
void DepthBoneConstraintChangeAction::initialize(std::shared_ptr<ExDepthBone> value) {
    bone = std::move(value);
    captureOld();
}
void DepthBoneConstraintChangeAction::captureOld() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    oldConstraintType = compat::ngRequireObject(bone)->constraintType;
    oldHingeAxis = compat::ngRequireObject(bone)->hingeAxis;
    oldLockRotation = compat::ngRequireObject(bone)->lockRotation;
    oldLockTranslation = compat::ngRequireObject(bone)->lockTranslation;
    oldAllowParentToTargets = compat::ngRequireObject(bone)->allowParentToTargets;
    oldRotationLimits = duplicateDepthBoneArray(compat::ngRequireObject(bone)->rotationLimits);
    oldMaxStepRadians = compat::ngRequireObject(bone)->maxStepRadians;
}
void DepthBoneConstraintChangeAction::updateNewState() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    newConstraintType = compat::ngRequireObject(bone)->constraintType;
    newHingeAxis = compat::ngRequireObject(bone)->hingeAxis;
    newLockRotation = compat::ngRequireObject(bone)->lockRotation;
    newLockTranslation = compat::ngRequireObject(bone)->lockTranslation;
    newAllowParentToTargets = compat::ngRequireObject(bone)->allowParentToTargets;
    newRotationLimits = duplicateDepthBoneArray(compat::ngRequireObject(bone)->rotationLimits);
    newMaxStepRadians = compat::ngRequireObject(bone)->maxStepRadians;
    notifyChanged();
}
void DepthBoneConstraintChangeAction::notifyChanged() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    compat::ngRequireObject(bone)->notifyChange(bone, NotifyReason::AttributeChanged);
    ngNotifyDepthBoneRigChanged(bone, literal("Depth Bone Constraint"));
}
void DepthBoneConstraintChangeAction::apply(compat::RetainedString constraintType, Vec3 hingeAxis,
        bool lockRotation, bool lockTranslation, bool allowParentToTargets,
        compat::Slice<float> rotationLimits, float maxStepRadians) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    compat::ngRequireObject(bone)->constraintType = std::move(constraintType);
    compat::ngRequireObject(bone)->hingeAxis = hingeAxis;
    compat::ngRequireObject(bone)->lockRotation = lockRotation;
    compat::ngRequireObject(bone)->lockTranslation = lockTranslation;
    compat::ngRequireObject(bone)->allowParentToTargets = allowParentToTargets;
    {
        // D reaches the actual field destination before evaluating .dup.
        const auto receiver = compat::ngRequireObject(bone);
        receiver->rotationLimits = duplicateDepthBoneArray(rotationLimits);
    }
    compat::ngRequireObject(bone)->maxStepRadians = maxStepRadians;
    notifyChanged();
}
void DepthBoneConstraintChangeAction::rollback() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    apply(oldConstraintType, oldHingeAxis, oldLockRotation, oldLockTranslation, oldAllowParentToTargets,
        oldRotationLimits, oldMaxStepRadians);
}
void DepthBoneConstraintChangeAction::redo() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    apply(newConstraintType, newHingeAxis, newLockRotation, newLockTranslation, newAllowParentToTargets,
        newRotationLimits, newMaxStepRadians);
}
ActionText DepthBoneConstraintChangeAction::describe() {
    const auto receiverLifetime = weak_from_this().lock();
    return translated("Depth bone constraint changed");
}
ActionText DepthBoneConstraintChangeAction::describeUndo() {
    const auto receiverLifetime = weak_from_this().lock();
    return translated("Depth bone constraint changed");
}
ActionText DepthBoneConstraintChangeAction::getName() { return literal("DepthBoneConstraintChangeAction"); }
} // namespace nijigenerate::actions::depthbone
