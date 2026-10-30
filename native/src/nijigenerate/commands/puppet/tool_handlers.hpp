#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <memory>

namespace nijigenerate::commands::puppet::tool {

// Reuse the actual shared JSON representation also published by autorig/json.hpp.
// Each nullable delegate descriptor retains one callable environment.
using AutoRigExecuteHandler = compat::SharedDelegate<compat::RetainedStringSlice(
    core::settings::SettingsJsonValue, core::settings::SettingsJsonValue)>;
using AutoRigStatusHandler = compat::SharedDelegate<core::settings::SettingsJsonValue(
    compat::RetainedStringSlice)>;
using AutoRigResumeHandler = compat::SharedDelegate<compat::RetainedStringSlice(
    compat::RetainedStringSlice, compat::RetainedStringSlice)>;
using AutoRigRemoveHandler = compat::SharedDelegate<void(compat::RetainedStringSlice)>;

void ngSetAutoRigCommandHandlers(AutoRigExecuteHandler execute, AutoRigStatusHandler status,
    AutoRigResumeHandler resume, AutoRigRemoveHandler remove = {});

} // namespace nijigenerate::commands::puppet::tool
