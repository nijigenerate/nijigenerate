#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/settings_value.hpp>

namespace nijigenerate::autorig::deterministic::templates {

core::settings::SettingsJsonValue ngRigTemplate(compat::RetainedStringSlice id);
core::settings::SettingsJsonValue ngRigTemplateManifest();
core::settings::SettingsJsonValue ngRigTemplateSchema();
core::settings::SettingsJsonValue ngRigHumanoidPrior();
core::settings::SettingsJsonValue ngRigMaterialRoles();
void ngRigValidateEmbeddedTemplates();

inline compat::Slice<compat::RetainedStringSlice> ngRigTemplateNames() {
    using compat::RetainedString;
    return {
        RetainedString::fromStaticLiteral("face_head"),
        RetainedString::fromStaticLiteral("eye_opening"),
        RetainedString::fromStaticLiteral("mouth_opening"),
        RetainedString::fromStaticLiteral("torso"),
        RetainedString::fromStaticLiteral("neck"),
        RetainedString::fromStaticLiteral("shoulder"),
        RetainedString::fromStaticLiteral("arm"),
        RetainedString::fromStaticLiteral("leg"),
        RetainedString::fromStaticLiteral("hand"),
        RetainedString::fromStaticLiteral("foot"),
        RetainedString::fromStaticLiteral("skirt"),
        RetainedString::fromStaticLiteral("sleeve"),
        RetainedString::fromStaticLiteral("hanging_sheet"),
        RetainedString::fromStaticLiteral("tube"),
        RetainedString::fromStaticLiteral("leaf"),
        RetainedString::fromStaticLiteral("ribbon_network"),
        RetainedString::fromStaticLiteral("frill"),
        RetainedString::fromStaticLiteral("surface_layer"),
        RetainedString::fromStaticLiteral("rigid_panel")
    };
}

} // namespace nijigenerate::autorig::deterministic::templates
