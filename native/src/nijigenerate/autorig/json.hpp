#pragma once

#include <nijigenerate/core/settings_value.hpp>

namespace nijigenerate::autorig::json {

core::settings::SettingsJsonValue ngParseAutoRigJson(compat::RetainedStringSlice text);

} // namespace nijigenerate::autorig::json
