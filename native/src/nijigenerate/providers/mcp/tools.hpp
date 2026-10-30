#pragma once
#include "schema.hpp"

namespace nijigenerate::providers::mcp::tools {
using ToolHandler=Delegate<Json(Json)>;
class ToolExecutionError:public protocol::MCPError {
public:
    explicit ToolExecutionError(String message,String details={},
        std::source_location source=std::source_location::current())
        :MCPError(static_cast<std::int32_t>(protocol::ErrorCode::invalidParams),message,details,
            text(source.file_name()),source.line()) {}
};
class Tool {
    String name,description;
    schema::SchemaBuilder::Ref schema_;
    ToolHandler handler;
public:
    Tool(String name,String description,schema::SchemaBuilder::Ref schema,ToolHandler handler)
        :name(name),description(description),schema_(schema),handler(handler) {}
    Json toJSON() const;
    Json execute(Json args);
};
class ToolRegistry {
    StringMap<std::shared_ptr<Tool>> tools;
public:
    void addTool(String name,String description,schema::SchemaBuilder::Ref schema,ToolHandler handler);
    std::shared_ptr<Tool> getTool(String name);
    Json listTools();
};
}
