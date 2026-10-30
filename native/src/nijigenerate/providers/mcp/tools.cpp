// Port of MCP 1.0.1 tools. Copyright 2025 Garret Noble; port/licenses/mcp-stdio.txt.
#include "tools.hpp"
#include <nijigenerate/compat/json_stringify.hpp>
#include <new>

namespace nijigenerate::providers::mcp::tools {
namespace {
Json content(String message,bool error) {
    Object item;item["type"]="text";item["text"]=jsonString(message);
    Array items;items.append(Json(item));
    Object result;result["content"]=items;if(error)result["isError"]=true;return result;
}
}
Json Tool::toJSON() const {
    Object value;value["name"]=jsonString(name);value["description"]=jsonString(description);
    value["inputSchema"]=schema_->toJSON();return value;
}
Json Tool::execute(Json args) {
    try {
        try {schema_->validate(args);}
        catch(const schema::SchemaValidationError& error) {throw ToolExecutionError(error.msg);}
        auto result=handler(args);
        if(result.is_object()) {
            auto object=result.as_object();
            if(object.if_contains("_meta")||object.if_contains("content"))return result;
        }
        return content(compat::ngJsonStringify(result),false);
    } catch(const std::bad_alloc&) {throw;}
      catch(const std::exception& error) {
        const auto* original=dynamic_cast<const compat::ExceptionMessage*>(&error);
        return content(text(original?original->ngExceptionMessage():std::string_view(error.what())),true);
    }
}
void ToolRegistry::addTool(String name,String description,schema::SchemaBuilder::Ref schema,ToolHandler handler) {
    if(name.empty())throw ToolExecutionError(text("Tool name cannot be empty"));
    if(description.empty())throw ToolExecutionError(text("Tool description cannot be empty"));
    if(!schema)throw ToolExecutionError(text("Tool schema cannot be null"));
    if(!handler)throw ToolExecutionError(text("Tool handler cannot be null"));
    if(tools.contains(name))throw protocol::MCPError(static_cast<std::int32_t>(protocol::ErrorCode::invalidRequest),
        text("Tool already exists: "+std::string(name.view())));
    tools[name]=std::make_shared<Tool>(name,description,schema,handler);
}
std::shared_ptr<Tool> ToolRegistry::getTool(String name) {
    const auto found=tools.find(name);
    if(found==tools.end())throw protocol::MCPError(static_cast<std::int32_t>(protocol::ErrorCode::methodNotFound),
        text("Tool not found: "+std::string(name.view())));
    return found->second;
}
Json ToolRegistry::listTools() {
    compat::Slice<std::shared_ptr<Tool>> snapshot;
    for(const auto& [key,value]:tools)snapshot.append(value);
    Array values;for(const auto& value:snapshot)values.append(value->toJSON());
    Object result;result["tools"]=values;return result;
}
}
