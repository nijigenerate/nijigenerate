#pragma once

#include "server.hpp"
#include "task.hpp"

namespace nijigenerate::api::mcp {
using server::ngMcpPrepareActionScopeForCurrentMode;
using server::ngMcpFinishActionBoundary;
using server::ngAcpLoadSettings;
using server::ngMcpInit;
using server::ngMcpIsRunning;
using server::ngMcpStop;
using server::ngMcpApplySettings;
using server::ngMcpLoadSettings;
using server::ngMcpAuthEnabled;

using task::ngMcpSetExternalCommandsBlocked;
using task::ngMcpExternalCommandsBlocked;
using task::ngMcpInitTask;
using task::ngMcpEnqueueAction;
using task::ngMcpProcessQueue;
using task::ngRunInMainThread;
#ifdef NG_COMMAND_BROWSER_DIFFERENTIAL
using server::ngMcpJsonArgumentSchemaForRegression;
using server::ngMcpValidateJsonArgumentForRegression;
#endif
} // namespace nijigenerate::api::mcp
