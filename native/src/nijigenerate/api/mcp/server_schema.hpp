#pragma once
#include <nijigenerate/providers/mcp/schema.hpp>
#include <nijigenerate/commands/base.hpp>

namespace nijigenerate::api::mcp::server::detail {
using McpSchema=providers::mcp::schema::SchemaBuilder;
McpSchema::Ref depthOperationSchema();
McpSchema::Ref jsonArgumentSchema(commands::CommandJsonSchema kind,providers::mcp::String description);
McpSchema::Ref contextSchema();
}
