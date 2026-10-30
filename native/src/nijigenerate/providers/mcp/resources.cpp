// Port of MCP 1.0.1 resources. Copyright 2025 Garret Noble; port/licenses/mcp-stdio.txt.
#include "resources.hpp"
#include <nijigenerate/compat/phobos_index_sort.hpp>
#include <numeric>

namespace nijigenerate::providers::mcp::resources {
namespace {
String base64(compat::Slice<std::uint8_t> bytes) {
    static constexpr char digits[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for(std::size_t i=0;i<bytes.size();i+=3) {
        const std::uint32_t a=bytes[i],b=i+1<bytes.size()?bytes[i+1]:0,c=i+2<bytes.size()?bytes[i+2]:0;
        result+=digits[a>>2];result+=digits[((a&3)<<4)|(b>>4)];
        result+=i+1<bytes.size()?digits[((b&15)<<2)|(c>>6)]:'=';
        result+=i+2<bytes.size()?digits[c&63]:'=';
    }
    return text(result);
}
}
ResourceContents ResourceContents::makeText(String type,String content) {
    ResourceContents value;value.mimeType=type;value.textContent=content;return value;
}
ResourceContents ResourceContents::makeBinary(String type,compat::Slice<std::uint8_t> content) {
    ResourceContents value;value.mimeType=type;value.blob=content;return value;
}
Json ResourceContents::toJSON() const {
    Object value;value["uri"]=jsonString(uri);value["mimeType"]=jsonString(mimeType);
    if(!blob.isNull())value["blob"]=jsonString(base64(blob));else value["text"]=jsonString(textContent);
    return value;
}
ResourceNotifier ResourceRegistry::addResource(String uri,String name,String description,StaticResourceReader reader) {
    resources.append(ResourceEntry{uri,name,description,false,0,reader,{},{}});
    const auto callback=notifySubscribers;
    ResourceNotifier notifier=[callback,uri]{if(callback)callback(uri);};
    resources[resources.size()-1].notifyChanged=notifier;
    return notifier;
}
ResourceNotifier ResourceRegistry::addDynamicResource(String uri,String name,String description,DynamicResourceReader reader) {
    resources.append(ResourceEntry{uri,name,description,true,uri.size(),{},reader,{}});
    // D Windows x64 ResourceEntry is 112 bytes (three strings and three delegates).
    const auto snapshot=resources.duplicate();
    std::vector<std::size_t> order(resources.size());std::iota(order.begin(),order.end(),0);
    compat::phobos_index_sort::sort<112>(order,[&](std::size_t a,std::size_t b) {
        const auto& left=snapshot[a];const auto& right=snapshot[b];
        return left.isDynamic&&right.isDynamic?left.priority>right.priority:left.isDynamic<right.isDynamic;
    });
    for(std::size_t i=0;i<order.size();++i)resources[i]=snapshot[order[i]];
    const auto callback=notifySubscribers;
    ResourceNotifier notifier=[callback,uri]{if(callback)callback(uri);};
    // The source assigns the last sorted entry, even if it is not the inserted one.
    resources[resources.size()-1].notifyChanged=notifier;
    return notifier;
}
ResourceNotifier ResourceRegistry::addTemplate(String uri,String name,String description,String type,TemplateResourceReader reader) {
    ResourceTemplate value;value.uriTemplate=uri;value.name=name;value.description=description;
    value.mimeType=type;value.reader=reader;templates.append(value);
    const auto callback=notifySubscribers;
    return [callback,uri]{if(callback)callback(uri);};
}
Json ResourceRegistry::listTemplates() {
    Array values;
    for(const auto& item:templates) {
        Object value;value["uriTemplate"]=jsonString(item.uriTemplate);value["name"]=jsonString(item.name);
        value["description"]=jsonString(item.description);value["mimeType"]=jsonString(item.mimeType);values.append(Json(value));
    }
    Object result;result["resourceTemplates"]=values;return result;
}
StringMap<String> ResourceRegistry::matchTemplate(String pattern,String uri) {
    compat::Slice<String> names;
    for(const auto& match:ngPhobosResourceMatchAll(pattern,text("\\{([^}]+)\\}")))names.append(match[1]);
    if(names.empty())return {};
    auto expression=pattern;
    for(const auto& name:names)
        expression=ngPhobosResourceReplaceAll(expression,text("\\{"+std::string(name.view())+"\\}"),text("([^/]+)"));
    const auto matches=ngPhobosResourceMatchFirst(uri,text("^"+std::string(expression.view())+"$"));
    if(matches.empty()||matches.size()!=names.size()+1)return {};
    StringMap<String> parameters;
    for(std::size_t i=0;i<names.size();++i)parameters[names[i]]=matches[i+1];
    return parameters;
}
Json ResourceRegistry::listResources() {
    Array values;
    for(const auto& item:resources) {
        Object value;value["uri"]=jsonString(item.isDynamic?text(std::string(item.uri.view())+"*"):item.uri);
        value["name"]=jsonString(item.name);value["description"]=jsonString(item.description);values.append(Json(value));
    }
    Object result;result["resources"]=values;return result;
}
ResourceContents ResourceRegistry::readResource(String uri) {
    // Retain the original foreach slice when a callback appends to the registry.
    auto entries=resources;
    for(auto& entry:entries) {
        if(entry.isDynamic) {
            if(uri.view().starts_with(entry.uri.view())) {
                const auto reader=entry.dynamicReader;
                auto value=reader(uri.slice(entry.uri.size(),uri.size()));value.setURI(uri);return value;
            }
        } else if(uri==entry.uri) {
            const auto reader=entry.staticReader;auto value=reader();value.setURI(uri);return value;
        }
    }
    auto patterns=templates;
    for(auto& entry:patterns) {
        auto parameters=matchTemplate(entry.uriTemplate,uri);
        if(!parameters.empty()) {
            const auto reader=entry.reader;auto value=reader(parameters);value.setURI(uri);return value;
        }
    }
    throw ResourceNotFoundException(uri);
}
}
