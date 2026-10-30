#pragma once

#include "task.hpp"
#include "nijigenerate/compat/retained_string_slice.hpp"
#include <cstdint>
#ifdef NG_COMMAND_BROWSER_DIFFERENTIAL
#include <nijigenerate/commands/base.hpp>
#include <nijigenerate/core/settings_value.hpp>
#endif

namespace nijigenerate::api::mcp::server {

// Preserve the original public task import as aliases of the real declarations.
using task::ngMcpSetExternalCommandsBlocked;
using task::ngMcpExternalCommandsBlocked;
using task::ngMcpInitTask;
using task::ngMcpEnqueueAction;
using task::ngMcpProcessQueue;
using task::ngRunInMainThread;

void ngMcpPrepareActionScopeForCurrentMode(compat::RetainedStringSlice toolName);
void ngMcpFinishActionBoundary();
void ngAcpLoadSettings();
void ngMcpInit(compat::RetainedStringSlice host = compat::RetainedString::fromStaticLiteral("127.0.0.1"),
    std::uint16_t port = 8088);
bool ngMcpIsRunning();
void ngMcpStop();
// Native process teardown joins the owned worker while the UI providers still exist.
void ngMcpShutdown();
void ngMcpApplySettings(bool enabled, compat::RetainedStringSlice host, std::uint16_t port);
void ngMcpLoadSettings();
void ngMcpAuthEnabled(bool value);
bool ngMcpAuthEnabled();
#ifdef NG_COMMAND_BROWSER_DIFFERENTIAL
core::settings::SettingsJsonValue ngMcpJsonArgumentSchemaForRegression(commands::CommandJsonSchema schema);
void ngMcpValidateJsonArgumentForRegression(commands::CommandJsonSchema schema, core::settings::SettingsJsonValue value);
#endif

} // namespace nijigenerate::api::mcp::server
