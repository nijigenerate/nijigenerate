#pragma once
#include <nijigenerate/providers/mcp/server.hpp>

namespace nijigenerate::api::mcp::server::detail {
void registerPrompts(providers::mcp::server::MCPServer& server);
void registerTools(providers::mcp::server::MCPServer& server);
struct ResourceNotifications {
    providers::mcp::resources::ResourceNotifier find, byUuid, binding;
};
void registerResources(providers::mcp::server::MCPServer& server, ResourceNotifications& notifications);
}
