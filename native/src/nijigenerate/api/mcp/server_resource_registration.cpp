#include "server_registration.hpp"
#include "server_resources.hpp"
#include "server_guides.hpp"
#include "task.hpp"
#include "server_log.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/phobos_unsigned_conversion.hpp>
#include <nijigenerate/core/selector/parse_uint.hpp>
#include <serde_json.hpp>
#include <common/resource_key_map.hpp>
#include <functional>
#include <string_view>

namespace nijigenerate::api::mcp::server::detail {
using namespace providers::mcp;
using namespace providers::mcp::resources;
namespace {
template<std::size_t N> String literal(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
struct UuidHash {
    std::size_t operator()(std::uint32_t value) const noexcept { return value; }
};
using SeenUuids=nicxlive::core::common::ResourceKeyMap<std::uint32_t,bool,
    UuidHash,std::equal_to<std::uint32_t>>;

Json readUuid(std::uint32_t uuid) {
    return task::ngRunInMainThread<Json>(task::MainThreadAction<Json>([uuid] {
        Object item;
        if(auto puppet=project::incActivePuppet();puppet&&uuid!=0) {
            if(auto node=puppet->findNodeById(uuid)) {
                item[literal("typeId")]=jsonString(literal("Node"));
                item[literal("uuid")]=static_cast<std::int64_t>(uuid);
                assignResourceMember(item,literal("name"),[&] {return node->getName();});
                using F=nicxlive::core::nodes::SerializeNodeFlags;
                nicxlive::core::serde::TypedJsonWriter writer;auto scope=writer.structBegin();
                node->serializePartial(writer,F::Basics|F::State|F::Geometry|F::Links,true);
                writer.structEnd(scope);writer.flush();
                assignResourceMember(item,literal("data"),[&] {
                    return autorig::json::detail::parseJSON(std::string_view(writer.str()));
                });
            } else if(auto parameter=puppet->findParameter(uuid)) {
                item[literal("typeId")]=jsonString(literal("Parameter"));
                item[literal("uuid")]=static_cast<std::int64_t>(uuid);
                assignResourceMember(item,literal("name"),[&] {return parameter->getName();});
            }
        }
        Object output;output[literal("item")]=item.empty()?Json{}:Json(item);return Json(output);
    }));
}
ResourceContents jsonContents(Json value) {
    return ResourceContents::makeText(literal("application/json"),compat::ngJsonStringify(value));
}
}
void registerResources(providers::mcp::server::MCPServer& server, ResourceNotifications& notifications) {
    using namespace guides;
    auto guide=[&](String uri,String name,String description,String content) {
        mcpLog("[MCP] addResource: %s",uri);
        server.addResource(uri,name,description,[content] {
            return ResourceContents::makeText(literal("text/markdown"),content);
        });
    };
    guide(literal("resource://nijigenerate/guides/resources"),literal("Resources Guide"),
        literal("Guide for resource discovery and reading."),compat::RetainedString::fromStaticView(GUIDES_RESOURCES));
    guide(literal("resource://nijigenerate/guides/selectors"),literal("Selectors Guide"),
        literal("Selector syntax reference for resources/find."),compat::RetainedString::fromStaticView(GUIDES_SELECTORS));
    guide(literal("resource://nijigenerate/guides/find"),literal("Find Guide"),
        literal("Usage examples for resources/find."),compat::RetainedString::fromStaticView(GUIDES_FIND));
    mcpLog("[MCP] addDynamicResource: %s",literal("resource://nijigenerate/resources/find?selector="));
    notifications.find=server.addDynamicResource(literal("resource://nijigenerate/resources/find?selector="),
        literal("Start Here: Explore Entire Model Tree"),literal("Hierarchical resource discovery by selector."),
        [](String path) {
            mcpLog("[MCP] read resource resources/find?selector=%s",path);
            return buildFindResource(ngDecodeMcpResourceComponent(path));
        });
    mcpLog("[MCP] addDynamicResource: %s",literal("resource://nijigenerate/bindings/get?parameter="));
    notifications.binding=server.addDynamicResource(literal("resource://nijigenerate/bindings/get?parameter="),
        literal("Read Binding By Descriptor"),literal("Read binding values by parameter UUID, target UUID, and binding name."),
        [](String path) {
            auto result=task::ngRunInMainThread<Json>(task::MainThreadAction<Json>([path] {
                std::uint32_t parameter=0,target=0;String name,message;
                if(!parseBindingDescriptorPath(path,parameter,target,name,message)) {
                    Object error;error[literal("item")]={};error[literal("error")]=jsonString(message);return Json(error);
                }
                return readBindingByDescriptor(parameter,target,name);
            }));
            return jsonContents(result);
        });
    if(auto puppet=project::incActivePuppet()) {
        SeenUuids seen;
        auto add=[&](std::uint32_t uuid,String name,String type) {
            const String uri=allocateMcpText([&] {
                return compat::RetainedString("resource://nijigenerate/resources/"+std::to_string(uuid));
            });
            mcpLog("[MCP] addResource: %s",uri);
            const auto description=allocateMcpText([&] {
                return compat::RetainedString(std::string(type.view())+" resource instance.");
            });
            server.addResource(uri,name,description,
                [uuid] {return jsonContents(readUuid(uuid));});
        };
        const auto nodes=[&](auto&& self,std::shared_ptr<nicxlive::core::nodes::Node> node) -> void {
            if(!node) return;
            if(seen.contains(node->getUUID())) return;
            seen[node->getUUID()]=true;
            // Source call arguments evaluate left-to-right, including virtual getters.
            const auto uuid=node->getUUID();
            const auto nodeName=node->getName();
            const String name=allocateMcpText([&] {return compat::RetainedString(nodeName);});
            const auto nodeType=node->typeId();
            const String type=allocateMcpText([&] {return compat::RetainedString(nodeType);});
            add(uuid,name,type);
            const auto children=node->children;
            for(auto child:children) self(self,child);
        };
        nodes(nodes,puppet->root);
        const auto parameters=puppet->parameters;
        for(auto parameter:parameters) {
            if(!parameter) continue;
            if(seen.contains(parameter->getUUID())) continue;
            seen[parameter->getUUID()]=true;
            const auto uuid=parameter->getUUID();
            const auto parameterName=parameter->getName();
            const String name=allocateMcpText([&] {return compat::RetainedString(parameterName);});
            add(uuid,name,compat::RetainedString::fromStaticLiteral("Parameter"));
        }
    }
    mcpLog("[MCP] addTemplate: %s",literal("resource://nijigenerate/resources/{uuid}"));
    notifications.byUuid=server.addTemplate(literal("resource://nijigenerate/resources/{uuid}"),
        literal("Read Resource Instance By UUID"),literal("Read one resource instance by UUID."),literal("application/json"),
        [](StringMap<String> parameters) {
            std::uint32_t uuid=0;
            const String uuidKey=compat::RetainedString::fromStaticLiteral("uuid");
            if(auto entry=parameters.find(uuidKey);entry!=parameters.end()) {
                const String value=entry->second;
                try {
                    const auto bytes=value.view();
                    // The original 0x branch still calls decimal to!ulong, without stripping its prefix.
                    if(bytes.size()>2&&(bytes.substr(0,2)=="0x"||bytes.substr(0,2)=="0X"))
                        uuid=static_cast<std::uint32_t>(compat::phobos::ngUnsignedDecimal64(bytes));
                    else {
                        auto remaining=bytes;
                        const auto parsed=core::selector::ngParseSelectorUInt(remaining);
                        // to!uint rejects trailing input through the same convError path.
                        // A remaining suffix necessarily starts with a non-digit.
                        if(!remaining.empty()) (void)core::selector::ngParseSelectorUInt(remaining);
                        uuid=parsed;
                    }
                } catch(const compat::ThrowableException&) {}
            }
            return jsonContents(readUuid(uuid));
        });
}
}
