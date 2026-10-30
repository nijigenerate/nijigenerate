#pragma once
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/ext/nodes/exdepthrig_data.hpp>

namespace nijigenerate::commands::depth::bone {
namespace {
core::settings::SettingsJsonValue sourceSettingsToJson(ext::nodes::ExDepthBoneSourceSettings setting) {
    using core::settings::SettingsJsonObject;
    using core::settings::SettingsJsonValue;
    using compat::RetainedString;
    SettingsJsonObject object;
    object[RetainedString::fromStaticLiteral("uuid")] = SettingsJsonValue(setting.boneUuid);
    object[RetainedString::fromStaticLiteral("weight")] = SettingsJsonValue(setting.weight);
    object[RetainedString::fromStaticLiteral("depthOffset")] = SettingsJsonValue(setting.depthOffset);
    object[RetainedString::fromStaticLiteral("depthScale")] = SettingsJsonValue(setting.depthScale);
    object[RetainedString::fromStaticLiteral("rotation")] = SettingsJsonValue(
        ext::nodes::ngNormalizeDepthBoneSourceRotation(setting.rotation));
    return SettingsJsonValue(object);
}
} // namespace
} // namespace nijigenerate::commands::depth::bone
