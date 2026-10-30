#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Included once by the original bone.d production translation unit.
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <memory>

namespace nijigenerate::actions::node::detail {
// Existing real platform fault implementation in actions/node.cpp.
[[noreturn]] void failActionObjectReference();
}

namespace nijigenerate::commands::depth::bone {
namespace {

core::settings::SettingsJsonValue boneToJson(std::shared_ptr<ext::nodes::ExDepthBone> bone) {
    using core::settings::SettingsJsonArray;
    using core::settings::SettingsJsonObject;
    using core::settings::SettingsJsonValue;
    using core::settings::SettingsString;
    using compat::RetainedString;
    SettingsJsonObject object;
    if (!bone) actions::node::detail::failActionObjectReference();
    object[RetainedString::fromStaticLiteral("uuid")] = SettingsJsonValue(bone->getUUID());
    object[RetainedString::fromStaticLiteral("name")] = SettingsJsonValue(
        SettingsString(ext::nodes::ngRetainNodeName(bone->getName())));
    object[RetainedString::fromStaticLiteral("boneId")] = SettingsJsonValue(SettingsString(bone->boneId));
    object[RetainedString::fromStaticLiteral("restHead")] = SettingsJsonValue(SettingsJsonArray{
        bone->restHead.x, bone->restHead.y, bone->restHead.z});
    object[RetainedString::fromStaticLiteral("restTail")] = SettingsJsonValue(SettingsJsonArray{
        bone->restTail.x, bone->restTail.y, bone->restTail.z});
    object[RetainedString::fromStaticLiteral("restRoll")] = SettingsJsonValue(bone->restRoll);
    object[RetainedString::fromStaticLiteral("allowParentToTargets")] = SettingsJsonValue(bone->allowParentToTargets);
    if (bone->parentPtr()) {
        const auto parent = bone->parentPtr();
        if (!parent) actions::node::detail::failActionObjectReference();
        object[RetainedString::fromStaticLiteral("parent")] = SettingsJsonValue(parent->getUUID());
    }
    return SettingsJsonValue(object);
}

} // namespace
} // namespace nijigenerate::commands::depth::bone

namespace nijigenerate::commands::depth::bone::detail {
core::settings::SettingsJsonValue ruleToJson(ext::nodes::ExDepthInfluenceRule& rule);
}
