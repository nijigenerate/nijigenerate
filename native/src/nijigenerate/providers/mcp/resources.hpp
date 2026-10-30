#pragma once
#include "common.hpp"

namespace nijigenerate::providers::mcp::resources {
class ResourceNotFoundException:public protocol::MCPError {
public:
    explicit ResourceNotFoundException(String uri,std::source_location source=std::source_location::current())
        :MCPError(static_cast<std::int32_t>(protocol::ErrorCode::methodNotFound),
            text("Resource not found: "+std::string(uri.view())),{},text(source.file_name()),source.line()) {}
};
struct ResourceContents {
    String mimeType,textContent;
    compat::Slice<std::uint8_t> blob;
private:
    String uri;
public:
    static ResourceContents makeText(String mimeType,String content);
    static ResourceContents makeBinary(String mimeType,compat::Slice<std::uint8_t> content);
    void setURI(String value){uri=value;}
    Json toJSON() const;
};
using ResourceNotifier=Delegate<void()>;
using StaticResourceReader=Delegate<ResourceContents()>;
using DynamicResourceReader=Delegate<ResourceContents(String)>;
using TemplateResourceReader=Delegate<ResourceContents(StringMap<String>)>;
struct ResourceTemplate {
    String uriTemplate,name,description,mimeType;
private:
    TemplateResourceReader reader;
    friend class ResourceRegistry;
};
class ResourceRegistry {
    struct ResourceEntry {
        String uri,name,description;
        bool isDynamic=false;
        std::size_t priority=0;
        StaticResourceReader staticReader;
        DynamicResourceReader dynamicReader;
        ResourceNotifier notifyChanged;
    };
    compat::Slice<ResourceEntry> resources;
    compat::Slice<ResourceTemplate> templates;
    Delegate<void(String)> notifySubscribers;
    StringMap<String> matchTemplate(String pattern,String uri);
public:
    explicit ResourceRegistry(Delegate<void(String)> callback):notifySubscribers(callback) {}
    ResourceNotifier addResource(String uri,String name,String description,StaticResourceReader reader);
    ResourceNotifier addDynamicResource(String uri,String name,String description,DynamicResourceReader reader);
    ResourceNotifier addTemplate(String uri,String name,String description,String mimeType,TemplateResourceReader reader);
    Json listTemplates();Json listResources();ResourceContents readResource(String uri);
};
// Exact Phobos regex operations remain a shared prerequisite of schema/resources.
// Capture zero is the whole match. An empty result denotes no match.
compat::Slice<compat::Slice<String>> ngPhobosResourceMatchAll(String input,String expression);
compat::Slice<String> ngPhobosResourceMatchFirst(String input,String expression);
String ngPhobosResourceReplaceAll(String input,String expression,String replacement);
}
