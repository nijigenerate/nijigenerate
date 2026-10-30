#pragma once
#include "mcp_stdio.hpp"

namespace nijigenerate::api::acp::transport::stdio {
using Transport = providers::mcp::stdio::Transport;
using StdioTransport = providers::mcp::stdio::StdioTransport;
std::shared_ptr<StdioTransport> createStdioTransport();
}
