#pragma once
#include "common.hpp"
#include <mutex>

namespace nijigenerate::providers::mcp::prompts {
struct PromptArgument {
    String name,description;
    bool required=false;
    Json toJSON() const;
};
struct Prompt {
    String name,description;
    compat::Slice<PromptArgument> arguments;
    Json toJSON() const;
};
struct TextContent { String content; Json toJSON() const; };
struct ImageContent { String data,mimeType; Json toJSON() const; };
struct ResourceContent { String uri,mimeType,content; Json toJSON() const; };
struct PromptMessage {
    enum class MessageType { text,image,resource };
    String role;
    TextContent textContent;
    ImageContent imageContent;
    ResourceContent resourceContent;
    MessageType type=MessageType::text;
    Json toJSON() const;
    static PromptMessage text(String role,String content);
    static PromptMessage image(String role,String data,String mimeType);
    static PromptMessage resource(String role,String uri,String mimeType,String content={});
};
struct PromptResponse {
    String description;
    compat::Slice<PromptMessage> messages;
    Json toJSON() const;
};
using PromptHandler=Delegate<PromptResponse(String,StringMap<String>)>;
class PromptRegistry {
    struct StoredPrompt { Prompt prompt; PromptHandler handler; };
    StringMap<StoredPrompt> prompts;
    std::mutex promptsMutex;
    Delegate<void(String)> notifyCallback;
public:
    explicit PromptRegistry(Delegate<void(String)> callback):notifyCallback(callback) {}
    void addPrompt(Prompt prompt,PromptHandler handler);
    Json listPrompts();
    Json getPromptContent(String name,Json rawArguments);
};
}
