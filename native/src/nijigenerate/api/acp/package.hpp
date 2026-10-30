#pragma once
// Complete public-import surface of nijigenerate.api.acp.
#include "protocol.hpp"
#include "types.hpp"
#include "transport/stdio.hpp"
#include "agent.hpp"
#include "client.hpp"

namespace nijigenerate::api::acp {
using protocol::ACP_PROTOCOL_VERSION;
using protocol::JSONRPC_VERSION;
using protocol::ErrorCode;
using protocol::ACP_METHOD_INITIALIZE;
using protocol::ACP_METHOD_PING;
using protocol::ACP_METHOD_SESSION_NEW;
using protocol::ACP_METHOD_SESSION_PROMPT;
using protocol::ACP_METHOD_SESSION_CANCEL;
using protocol::ACP_METHOD_SESSION_UPDATE;
using protocol::ACP_METHOD_APPLY_EDIT;
using protocol::ACP_METHOD_STATUS;
using protocol::ACP_METHOD_USER_MSG;
using protocol::ACPError;

using types::Position;
using types::Range;
using types::TextEdit;
using types::WorkspaceEdit;
using types::Document;
using types::StatusLevel;
using types::StatusNotification;

using transport::stdio::Transport;
using transport::stdio::StdioTransport;
using transport::stdio::createStdioTransport;

using agent::ApplyEditHandler;
using agent::PromptHandler;
using agent::ACPAgent;
using client::ACPClient;
} // namespace nijigenerate::api::acp
