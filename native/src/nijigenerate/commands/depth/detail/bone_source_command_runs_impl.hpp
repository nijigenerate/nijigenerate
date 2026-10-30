#pragma once

// Included once after the actual merge-session TLS definitions in bone.cpp.
#include "../bone_commands.hpp"
#include "bone_input.hpp"
#include "bone_json_apply.hpp"
#include "bone_influence_preview.hpp"
#include "bone_strict_scope.hpp"
#include "bone_action_scope.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/actions/depthbone.hpp>
#include <nijigenerate/actions/node.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/ext/nodes/exdepthrig_create.hpp>
#include <puppet.hpp>
#include <array>

namespace nijigenerate::commands::depth::bone {
namespace {
template<class Array> decltype(auto) commandSourceElement(Array& values, std::size_t index,
        std::size_t sourceLine) {
    const auto length = values.size();
    if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), sourceLine);
    return values[index];
}

template<class Array> auto commandSourceTail(const Array& values, std::size_t lower,
        std::size_t sourceLine) {
    const auto length = values.size();
    if (lower > length) throw compat::ngStaticError<compat::ArraySliceError>(lower, length, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), sourceLine);
    return values.slice(lower, length);
}
} // namespace

std::shared_ptr<CommandResult> AddStandardDepthSkeletonCommand::run(std::shared_ptr<Context>) {
    auto rigRoot = detail::requireRoot(root);
    const auto insertOffset = rigRoot->children.size();
    ext::nodes::ngAddStandardDepthSkeleton(rigRoot, scale == 0 ? 1.0f : scale);
    compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> createdRoots;
    const auto children = rigRoot->childrenList();
    const auto createdChildren = commandSourceTail(children, insertOffset, 3864);
    for (auto child : createdChildren) createdRoots.append(child);
    for (auto child : createdRoots) {
        if (!child) actions::node::detail::failActionObjectReference();
        child->reparent({}, 0, true);
    }
    if (!createdRoots.empty())
        core::actionstack::incActionPush(detail::sourceDepthBoneObject<actions::node::NodeMoveAction>(
            createdRoots, rigRoot, insertOffset));
    if (rigRoot->puppetRef()) {
        auto puppet = rigRoot->puppetRef();
        if (!puppet) actions::node::detail::failActionObjectReference();
        puppet->rescanNodes();
    }
    rigRoot->notifyChange(rigRoot, nicxlive::core::nodes::NotifyReason::StructureChanged);
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> AddDepthBoneSourceCommand::run(std::shared_ptr<Context>) {
    auto rigRoot = detail::requireRoot(root);
    auto source = detail::requireSourceBone(rigRoot, bone);
    auto oldBindings = actions::depthbone::ngCopyDepthRigBindings(rigRoot->bindings);
    rigRoot->addBoneSource(target, detail::targetKindOf(target), source);
    core::actionstack::incActionPush(detail::sourceDepthBoneObject<
        actions::depthbone::DepthBoneSourceListChangeAction>(
        compat::RetainedString::fromStaticLiteral("Add Depth Bone Source"),
        rigRoot, oldBindings, rigRoot->bindings, false, target));
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> RemoveDepthBoneSourceCommand::run(std::shared_ptr<Context>) {
    auto rigRoot = detail::requireRoot(root);
    auto oldBindings = actions::depthbone::ngCopyDepthRigBindings(rigRoot->bindings);
    auto sourceTarget = target;
    auto sourceBone = detail::requireSourceBone(rigRoot, bone);
    rigRoot->removeBoneSource(sourceTarget, sourceBone);
    core::actionstack::incActionPush(detail::sourceDepthBoneObject<
        actions::depthbone::DepthBoneSourceListChangeAction>(
        compat::RetainedString::fromStaticLiteral("Remove Depth Bone Source"),
        rigRoot, oldBindings, rigRoot->bindings, false, target));
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> SetDepthBoneSourceSettingsCommand::run(std::shared_ptr<Context>) {
    auto rigRoot = detail::requireRoot(root);
    auto source = detail::requireSourceBone(rigRoot, bone);
    auto oldBindings = actions::depthbone::ngCopyDepthRigBindings(rigRoot->bindings);
    const auto targetKind = detail::targetKindOf(target);
    const auto bindingIndex = rigRoot->findBindingIndex(compat::ngRequireObject(target)->getUUID());
    ext::nodes::ExDepthBoneSourceSettings setting;
    if (bindingIndex >= 0) {
        auto bindingBacking = rigRoot->bindings;
        auto& selectedBinding = commandSourceElement(bindingBacking,
            static_cast<std::size_t>(bindingIndex), 4592);
        const auto sourceUuid = source->getUUID();
        setting = selectedBinding.sourceSetting(sourceUuid);
    }
    setting.boneUuid = source->getUUID();
    detail::applySourceSettingsJson(setting, settings);
    auto binding = rigRoot->getOrCreateBinding(target, targetKind);
    ext::nodes::ExDepthRigBinding::setSourceSettingNullable(binding.get(), setting);
    const auto debugRootName = rigRoot->getName();
    const auto debugTargetName = target ? target->getName()
        : nicxlive::core::common::NullableString::fromStaticLiteral("(null)");
    const auto debugSourceName = source->getName();
    const auto debugWeight = setting.weight;
    const auto debugDepthOffset = setting.depthOffset;
    const auto debugDepthScale = setting.depthScale;
    const auto debugRotation = setting.rotation;
    detail::depthBoneDebugLog(
        "[DepthBoneRefresh] source settings command: root=%s target=%s bone=%s "
        "weight=%s depthOffset=%s depthScale=%s rotation=%s",
        debugRootName, debugTargetName, debugSourceName,
        debugWeight, debugDepthOffset, debugDepthScale, debugRotation);
    const std::array<core::translation::TypedFormatArgument, 3> labelArguments{
        core::translation::ngFormatArgument(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Set Depth Bone Source Settings"))),
        core::translation::ngFormatArgument(target ? target->getName() : rigRoot->getName()),
        core::translation::ngFormatArgument(source->getName())};
    auto actionLabel = compat::RetainedString(core::translation::ngFormatTyped("%s: %s / %s", labelArguments));
    auto actionLabelArgument = actionLabel;
    auto actionRoot = rigRoot;
    auto actionOldBindings = oldBindings;
    auto actionNewBindings = rigRoot->bindings;
    auto actionTarget = target;
    const auto actionMergeSession = depthBoneSourceSettingsMergeSession;
    const auto actionBoneUuid = source->getUUID();
    auto actionMergeProperty = compat::RetainedStringSlice(depthBoneSourceSettingsMergeProperty);
    core::actionstack::incActionPush(detail::sourceDepthBoneObject<
        actions::depthbone::DepthBoneSourceListChangeAction>(
        actionLabelArgument, actionRoot, actionOldBindings, actionNewBindings, true, actionTarget,
        actionMergeSession, actionBoneUuid, actionMergeProperty));
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> SetDepthBoneInfluenceRuleCommand::run(std::shared_ptr<Context>) {
    auto rigRoot = detail::requireRoot(root);
    auto oldBindings = actions::depthbone::ngCopyDepthRigBindings(rigRoot->bindings);
    const auto targetKind = detail::targetKindOf(target);
    const auto bindingIndex = rigRoot->findBindingIndex(compat::ngRequireObject(target)->getUUID());
    ext::nodes::ExDepthInfluenceRule nextRule;
    if (bindingIndex >= 0) {
        nextRule = commandSourceElement(rigRoot->bindings,
            static_cast<std::size_t>(bindingIndex), 4638).influenceRule;
        ext::nodes::DepthUuidFloatMap copiedMultipliers;
        for (const auto& [uuid, value] : nextRule.multipliersByBoneUuid.entries())
            copiedMultipliers[uuid] = value;
        nextRule.multipliersByBoneUuid = copiedMultipliers;
    }
    detail::applyRuleJson(nextRule, rule);
    auto binding = rigRoot->getOrCreateBinding(target, targetKind);
    if (!binding) compat::ngFailNullAccess(true);
    binding->influenceRule = nextRule;
    core::actionstack::incActionPush(detail::sourceDepthBoneObject<
        actions::depthbone::DepthBoneBindingRuleChangeAction>(
        compat::RetainedString::fromStaticLiteral("Set Depth Bone Influence Rule"),
        rigRoot, oldBindings, rigRoot->bindings, false, target));
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> PreviewDepthBoneInfluenceCommand::run(std::shared_ptr<Context>) {
    auto rigRoot = detail::requireRoot(root);
    detail::targetKindOf(target);
    auto source = detail::requireSourceBone(rigRoot, bone);
    auto deformable = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(target);
    if (!deformable) detail::failInput("target is not deformable", 4678);
    ext::nodes::ExDepthRigBinding detachedBinding;
    detachedBinding.targetUuid = compat::ngRequireObject(target)->getUUID();
    detachedBinding.targetKind = detail::targetKindOf(target);
    const auto bindingIndex = rigRoot->findBindingIndex(compat::ngRequireObject(target)->getUUID());
    // Keep the actual selected array slot alive across the preview's virtual
    // vertices getter. Its callback may replace the root's bindings descriptor.
    auto bindingBacking = bindingIndex >= 0 ? rigRoot->bindings
        : compat::Slice<ext::nodes::ExDepthRigBinding>{};
    auto binding = bindingIndex >= 0
        ? &commandSourceElement(bindingBacking, static_cast<std::size_t>(bindingIndex), 4684) : &detachedBinding;
    deformable->deformation = detail::generateInfluencePreviewOffsets(binding, source, deformable);
    deformable->notifyChange(deformable, nicxlive::core::nodes::NotifyReason::AttributeChanged);
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> SetDepthBoneRestCommand::run(std::shared_ptr<Context>) {
    auto actualBone = detail::requireBone(bone);
    const auto oldHead = actualBone->restHead;
    const auto oldTail = actualBone->restTail;
    const auto oldRoll = actualBone->restRoll;
    auto nextHead = detail::vec3From(restHead, compat::RetainedString::fromStaticLiteral("restHead"));
    auto nextTail = detail::vec3From(restTail, compat::RetainedString::fromStaticLiteral("restTail"));
    actualBone->restHead = nextHead;
    actualBone->restTail = nextTail;
    actualBone->restRoll = restRoll;
    core::actionstack::incActionPush(detail::sourceDepthBoneObject<
        actions::depthbone::DepthBoneRestChangeAction>(
        actualBone, oldHead, oldTail, oldRoll, actualBone->restHead, actualBone->restTail, actualBone->restRoll));
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> SetDepthBoneConstraintCommand::run(std::shared_ptr<Context>) {
    auto actualBone = detail::requireBone(bone);
    autorig::json::detail::FirstPassReader reader{compat::RetainedStringSlice(constraint)};
    core::settings::SettingsJsonValue json;
    reader.parseValue(json);
    if (!json.is_object()) detail::failInput("constraint must be a JSON object", 4450);
    auto constraintType = actualBone->constraintType;
    auto lockRotation = actualBone->lockRotation;
    auto lockTranslation = actualBone->lockTranslation;
    auto allowParentToTargets = actualBone->allowParentToTargets;
    auto hingeAxis = actualBone->hingeAxis;
    auto rotationLimits = actualBone->rotationLimits.duplicate();
    auto maxStepRadians = actualBone->maxStepRadians;
    auto object = json.sourceObjectRef();
    if (auto value = object.if_contains("constraintType")) constraintType = value->as_string().retainedString();
    if (auto value = object.if_contains("lockRotation")) lockRotation = value->as_bool();
    if (auto value = object.if_contains("lockTranslation")) lockTranslation = value->as_bool();
    if (auto value = object.if_contains("allowParentToTargets")) allowParentToTargets = value->as_bool();
    if (auto value = object.if_contains("hingeAxis")) {
        auto values = value->as_array();
        if (values.size() != 3) detail::failInput("hingeAxis must be [x,y,z]", 4466);
        hingeAxis = {
            detail::strictJsonNumber(values[0], compat::RetainedString::fromStaticLiteral("hingeAxis[0]")),
            detail::strictJsonNumber(values[1], compat::RetainedString::fromStaticLiteral("hingeAxis[1]")),
            detail::strictJsonNumber(values[2], compat::RetainedString::fromStaticLiteral("hingeAxis[2]"))};
    }
    if (auto value = object.if_contains("rotationLimits")) {
        rotationLimits = rotationLimits.slice(0, 0);
        auto values = value->as_array();
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto element = values[i];
            const std::array<core::translation::TypedFormatArgument, 1> arguments{
                core::translation::ngFormatArgument(i)};
            auto name = compat::RetainedString(core::translation::ngFormatTyped("rotationLimits[%s]", arguments));
            rotationLimits.append(detail::strictJsonNumber(element, name));
        }
    }
    if (auto value = object.if_contains("maxStepRadians"))
        maxStepRadians = detail::strictJsonNumber(*value,
            compat::RetainedString::fromStaticLiteral("maxStepRadians"));
    auto action = actions::depthbone::ngCreateDepthBoneConstraintAction<>(actualBone);
    actualBone->constraintType = constraintType;
    actualBone->lockRotation = lockRotation;
    actualBone->lockTranslation = lockTranslation;
    actualBone->allowParentToTargets = allowParentToTargets;
    actualBone->hingeAxis = hingeAxis;
    actualBone->rotationLimits = rotationLimits;
    actualBone->maxStepRadians = maxStepRadians;
    action->updateNewState();
    core::actionstack::incActionPush(action);
    return CommandResult::opCall(true);
}

std::shared_ptr<CommandResult> ListDepthBonesCommand::run(std::shared_ptr<Context>) {
    auto rigRoot = detail::requireRoot(root);
    core::settings::SettingsJsonArray items;
    for (auto actualBone : rigRoot->depthBones()) items.append(boneToJson(actualBone));
    core::settings::SettingsJsonObject object;
    object[compat::RetainedString::fromStaticLiteral("items")] = core::settings::SettingsJsonValue(items);
    return ExCommandResult<core::settings::SettingsJsonValue>::opCall(
        true, core::settings::SettingsJsonValue(object));
}

std::shared_ptr<CommandResult> ListDepthBoneSourcesCommand::run(std::shared_ptr<Context>) {
    auto rigRoot = detail::requireRoot(root);
    core::settings::SettingsJsonArray sources;
    const auto index = rigRoot->findBindingIndex(compat::ngRequireObject(target)->getUUID());
    if (index >= 0) {
        auto uuids = commandSourceElement(rigRoot->bindings,
            static_cast<std::size_t>(index), 4557).sourceBoneUuids;
        for (auto uuid : uuids) sources.append(core::settings::SettingsJsonValue(uuid));
    }
    core::settings::SettingsJsonArray sourceObjects;
    if (index >= 0) {
        auto bindingBacking = rigRoot->bindings;
        auto binding = &commandSourceElement(bindingBacking, static_cast<std::size_t>(index), 4561);
        binding->normalizeSourceSettings();
        auto uuids = binding->sourceBoneUuids;
        for (auto uuid : uuids) sourceObjects.append(sourceSettingsToJson(binding->sourceSetting(uuid)));
    }
    core::settings::SettingsJsonObject object;
    object[compat::RetainedString::fromStaticLiteral("target")] =
        core::settings::SettingsJsonValue(compat::ngRequireObject(target)->getUUID());
    object[compat::RetainedString::fromStaticLiteral("sourceBoneUuids")] = core::settings::SettingsJsonValue(sources);
    object[compat::RetainedString::fromStaticLiteral("sources")] = core::settings::SettingsJsonValue(sourceObjects);
    return ExCommandResult<core::settings::SettingsJsonValue>::opCall(
        true, core::settings::SettingsJsonValue(object));
}

std::shared_ptr<CommandResult> GetDepthBoneInfluenceRuleCommand::run(std::shared_ptr<Context>) {
    auto rigRoot = detail::requireRoot(root);
    detail::targetKindOf(target);
    const auto index = rigRoot->findBindingIndex(compat::ngRequireObject(target)->getUUID());
    ext::nodes::ExDepthInfluenceRule actualRule;
    if (index >= 0) actualRule = commandSourceElement(rigRoot->bindings,
        static_cast<std::size_t>(index), 4662).influenceRule;
    return ExCommandResult<core::settings::SettingsJsonValue>::opCall(true, detail::ruleToJson(actualRule));
}

} // namespace nijigenerate::commands::depth::bone
