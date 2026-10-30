#include "server_registration.hpp"
#include "server_guides.hpp"
#include "server_log.hpp"
#include <cstddef>

namespace nijigenerate::api::mcp::server::detail {
namespace {
template<std::size_t N> providers::mcp::String literal(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
}
void registerPrompts(providers::mcp::server::MCPServer& server) {
    using namespace providers::mcp;
    using namespace providers::mcp::prompts;
    using namespace guides;
    mcpLog("[MCP] addPrompt: resources/find");
    server.addPrompt(literal("resources/find"),literal("How to use the resources/find resource endpoint"),
        compat::Slice<PromptArgument>{},[](String,StringMap<String>) {
            return PromptResponse{literal("Guide for resources/find"),compat::Slice<PromptMessage>{
                PromptMessage::text(literal("assistant"),compat::RetainedString::fromStaticView(RES_FIND_PROMPT))}};
        });
    mcpLog("[MCP] addPrompt: resources/get");
    server.addPrompt(literal("resources/get"),literal("How to use resources/get tool"),
        compat::Slice<PromptArgument>{},[](String,StringMap<String>) {
            return PromptResponse{literal("Guide for resources/get"),compat::Slice<PromptMessage>{
                PromptMessage::text(literal("assistant"),compat::RetainedString::fromStaticView(RESOURCE_PROMPT))}};
        });
    mcpLog("[MCP] addPrompt: selectors/guide");
    server.addPrompt(literal("selectors/guide"),
        literal("Selector syntax, examples, and returned tree shape for resources/find"),
        compat::Slice<PromptArgument>{},[](String,StringMap<String>) {
            return PromptResponse{literal("Selector syntax"),compat::Slice<PromptMessage>{
                PromptMessage::text(literal("assistant"),compat::RetainedString::fromStaticView(SELECTOR_GUIDE))}};
        });
    mcpLog("[MCP] addPrompt: resources/guide");
    server.addPrompt(literal("resources/guide"),literal("Resources usage guide"),
        compat::Slice<PromptArgument>{},[](String,StringMap<String>) {
            return PromptResponse{literal("Resources policy"),compat::Slice<PromptMessage>{
                PromptMessage::text(literal("assistant"),compat::RetainedString::fromStaticView(RESOURCES_GUIDE))}};
        });
    mcpLog("[MCP] addPrompt: tools/guide");
    server.addPrompt(literal("tools/guide"),literal("Tools input and context guide"),
        compat::Slice<PromptArgument>{},[](String,StringMap<String>) {
            return PromptResponse{literal("Tools usage"),compat::Slice<PromptMessage>{
                PromptMessage::text(literal("assistant"),compat::RetainedString::fromStaticView(TOOLS_GUIDE))}};
        });
}
}
