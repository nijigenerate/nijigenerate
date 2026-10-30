#pragma once
#include "tools.hpp"
#include "resources.hpp"
#include "prompts.hpp"
#include <nijigenerate/api/acp/transport/mcp_stdio.hpp>

namespace nijigenerate::providers::mcp::server {
class MCPServer {
    struct State;
    std::shared_ptr<State> state_;
    std::shared_ptr<stdio::Transport> transport_;
public:
    explicit MCPServer(std::shared_ptr<stdio::Transport> transport,
        String name=text("D MCP Server"),String version=text("1.0.0"));
    explicit MCPServer(String name=text("D MCP Server"),String version=text("1.0.0"));
    virtual ~MCPServer()=default;
    virtual Json executeTool(String name,Json arguments);
    virtual void addTool(String name,String description,schema::SchemaBuilder::Ref schema,tools::ToolHandler handler);
    virtual void addPrompt(String name,String description,compat::Slice<prompts::PromptArgument> arguments,
        prompts::PromptHandler handler);
    virtual resources::ResourceNotifier addResource(String uri,String name,String description,resources::StaticResourceReader reader);
    virtual resources::ResourceNotifier addDynamicResource(String uri,String name,String description,resources::DynamicResourceReader reader);
    virtual resources::ResourceNotifier addTemplate(String uri,String name,String description,String mimeType,
        resources::TemplateResourceReader reader);
    virtual void start();
};
}
