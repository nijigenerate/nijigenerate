#pragma once

#include "mcp/package.hpp"
#include "acp/package.hpp"

namespace nijigenerate::api {
using mcp::ngMcpPrepareActionScopeForCurrentMode;
using mcp::ngMcpFinishActionBoundary;
using mcp::ngAcpLoadSettings;
using mcp::ngMcpInit;
using mcp::ngMcpIsRunning;
using mcp::ngMcpStop;
using mcp::ngMcpApplySettings;
using mcp::ngMcpLoadSettings;
using mcp::ngMcpAuthEnabled;
using mcp::ngMcpSetExternalCommandsBlocked;
using mcp::ngMcpExternalCommandsBlocked;
using mcp::ngMcpInitTask;
using mcp::ngMcpEnqueueAction;
using mcp::ngMcpProcessQueue;
using mcp::ngRunInMainThread;
#ifdef NG_COMMAND_BROWSER_DIFFERENTIAL
using mcp::ngMcpJsonArgumentSchemaForRegression;
using mcp::ngMcpValidateJsonArgumentForRegression;
#endif

using acp::ACP_PROTOCOL_VERSION;
using acp::JSONRPC_VERSION;
using acp::ErrorCode;
using acp::ACP_METHOD_INITIALIZE;
using acp::ACP_METHOD_PING;
using acp::ACP_METHOD_SESSION_NEW;
using acp::ACP_METHOD_SESSION_PROMPT;
using acp::ACP_METHOD_SESSION_CANCEL;
using acp::ACP_METHOD_SESSION_UPDATE;
using acp::ACP_METHOD_APPLY_EDIT;
using acp::ACP_METHOD_STATUS;
using acp::ACP_METHOD_USER_MSG;
using acp::ACPError;
using acp::Position;
using acp::Range;
using acp::TextEdit;
using acp::WorkspaceEdit;
using acp::Document;
using acp::StatusLevel;
using acp::StatusNotification;
using acp::Transport;
using acp::StdioTransport;
using acp::createStdioTransport;
using acp::ApplyEditHandler;
using acp::PromptHandler;
using acp::ACPAgent;
using acp::ACPClient;
} // namespace nijigenerate::api
