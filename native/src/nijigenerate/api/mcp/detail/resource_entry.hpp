#pragma once

#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::api::mcp::resource_listing::detail {
inline core::settings::SettingsJsonValue resourceEntry(compat::RetainedStringSlice uri,
    compat::RetainedStringSlice name) {
    core::settings::SettingsJsonObject entry;
    entry[core::settings::SettingsString(compat::RetainedString::fromStaticLiteral("uri"))] =
        core::settings::SettingsJsonValue(core::settings::SettingsString(uri));
    entry[core::settings::SettingsString(compat::RetainedString::fromStaticLiteral("name"))] =
        core::settings::SettingsJsonValue(core::settings::SettingsString(name));
    return core::settings::SettingsJsonValue(entry);
}
} // namespace nijigenerate::api::mcp::resource_listing::detail
