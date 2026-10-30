// Port of MCP 1.0.1 prompts. Copyright 2025 Garret Noble; port/licenses/mcp-stdio.txt.
#include "prompts.hpp"
#include <nijigenerate/compat/assert_error.hpp>

namespace nijigenerate::providers::mcp::prompts {
namespace {
[[noreturn]] void fail(protocol::ErrorCode code,std::string message) {
    throw protocol::MCPError(static_cast<std::int32_t>(code),text(message));
}
}
Json PromptArgument::toJSON() const {
    Object value;value["name"]=jsonString(name);
    if(!description.empty()) value["description"]=jsonString(description);
    if(required) value["required"]=true;
    return value;
}
Json Prompt::toJSON() const {
    Object value;value["name"]=jsonString(name);
    if(!description.empty()) value["description"]=jsonString(description);
    if(!arguments.empty()) {
        Array values;for(const auto& argument:arguments) values.append(argument.toJSON());
        value["arguments"]=values;
    }
    return value;
}
Json TextContent::toJSON() const {
    Object value;value["type"]="text";value["text"]=jsonString(content);return value;
}
Json ImageContent::toJSON() const {
    Object value;value["type"]="image";value["data"]=jsonString(data);value["mimeType"]=jsonString(mimeType);
    return value;
}
Json ResourceContent::toJSON() const {
    Object resource;resource["uri"]=jsonString(uri);resource["mimeType"]=jsonString(mimeType);
    resource["text"]=jsonString(content);
    Object value;value["type"]="resource";value["resource"]=resource;return value;
}
Json PromptMessage::toJSON() const {
    Json content;
    switch(type) {
    case MessageType::text:content=textContent.toJSON();break;
    case MessageType::image:content=imageContent.toJSON();break;
    case MessageType::resource:content=resourceContent.toJSON();break;
    default:compat::ngAssertUnreachable("Invalid prompt message type",__FILE__,__LINE__);
    }
    Object value;value["role"]=jsonString(role);value["content"]=content;return value;
}
PromptMessage PromptMessage::text(String role,String content) {
    PromptMessage value;value.role=role;value.textContent={content};value.type=MessageType::text;return value;
}
PromptMessage PromptMessage::image(String role,String data,String mimeType) {
    PromptMessage value;value.role=role;value.imageContent={data,mimeType};value.type=MessageType::image;return value;
}
PromptMessage PromptMessage::resource(String role,String uri,String mimeType,String content) {
    PromptMessage value;value.role=role;value.resourceContent={uri,mimeType,content};value.type=MessageType::resource;
    return value;
}
Json PromptResponse::toJSON() const {
    Array items;for(const auto& message:messages) items.append(message.toJSON());
    Object value;value["description"]=jsonString(description);value["messages"]=items;return value;
}
void PromptRegistry::addPrompt(Prompt prompt,PromptHandler handler) {
    std::lock_guard lock(promptsMutex);
    if(prompt.name.empty()) fail(protocol::ErrorCode::invalidRequest,"Prompt name cannot be empty");
    if(prompts.contains(prompt.name))
        fail(protocol::ErrorCode::invalidRequest,"Prompt already exists: "+std::string(prompt.name.view()));
    prompts[prompt.name]={prompt,handler};
}
Json PromptRegistry::listPrompts() {
    // Snapshot values before encoding, matching the source AA .values operation.
    compat::Slice<StoredPrompt> snapshot;
    {
        std::lock_guard lock(promptsMutex);
        for(const auto& [key,value]:prompts) snapshot.append(value);
    }
    Array values;for(const auto& value:snapshot) values.append(value.prompt.toJSON());
    Object result;result["prompts"]=values;return result;
}
Json PromptRegistry::getPromptContent(String name,Json rawArguments) {
    StoredPrompt stored;
    {
        // M028 registers prompts after starting its transport thread. Protect the
        // shared table, but never retain this lock while executing a user handler.
        std::lock_guard lock(promptsMutex);
        const auto found=prompts.find(name);
        if(found==prompts.end()) fail(protocol::ErrorCode::methodNotFound,"Prompt not found: "+std::string(name.view()));
        stored=found->second;
    }
    StringMap<String> arguments;
    if(!rawArguments.is_null()) {
        if(auto object=rawArguments.as_object().if_contains("arguments"))
            for(const auto& member:object->as_object())
                if(member.value().is_string()) arguments[member.name]=member.value().as_string();
    }
    for(const auto& argument:stored.prompt.arguments)
        if(argument.required && !arguments.contains(argument.name))
            fail(protocol::ErrorCode::invalidParams,"Missing required argument: "+std::string(argument.name.view()));
    // Retain the callable before invoking a handler which can change the registry.
    const auto handler=stored.handler;
    const auto response=handler(name,arguments);
    if(response.messages.empty()) fail(protocol::ErrorCode::internalError,"Invalid prompt response: missing messages");
    for(const auto& message:response.messages)
        if(message.role.view()!="user" && message.role.view()!="assistant")
            fail(protocol::ErrorCode::internalError,"Invalid role: "+std::string(message.role.view()));
    return response.toJSON();
}
}
