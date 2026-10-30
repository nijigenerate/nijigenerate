// Port of MCP 1.0.1 server. Copyright 2025 Garret Noble; port/licenses/mcp-stdio.txt.
#include "server.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <new>

namespace nijigenerate::providers::mcp::server {
namespace {
struct ResourceCapabilities {
    bool listChanged=false,subscribe=false;
    Json toJSON() const {Object value;value["listChanged"]=listChanged;value["subscribe"]=subscribe;return value;}
};
struct ToolCapabilities {
    bool listChanged=false;
    Json toJSON() const {Object value;value["listChanged"]=listChanged;return value;}
};
struct PromptCapabilities {
    bool listChanged=false;
    Json toJSON() const {Object value;value["listChanged"]=listChanged;return value;}
};
struct ServerCapabilities {
    ResourceCapabilities resources;
    ToolCapabilities tools;
    PromptCapabilities prompts;
    Json toJSON() const {
        Object value;value["resources"]=resources.toJSON();value["tools"]=tools.toJSON();
        value["prompts"]=prompts.toJSON();return value;
    }
};
struct ServerInfo {
    String name,version;
    Json toJSON() const {Object value;value["name"]=jsonString(name);value["version"]=jsonString(version);return value;}
};
String messageOf(const std::exception& error) {
    const auto* original=dynamic_cast<const compat::ExceptionMessage*>(&error);
    return text(original?original->ngExceptionMessage():std::string_view(error.what()));
}
[[noreturn]] void fail(protocol::ErrorCode code,String message) {
    throw protocol::MCPError(static_cast<std::int32_t>(code),message);
}
Json errorResponse(Json id,protocol::ErrorCode code,String message) {
    return protocol::Response::makeError(id,static_cast<std::int32_t>(code),message).toJSON();
}
}
struct MCPServer::State:std::enable_shared_from_this<State> {
    std::shared_ptr<tools::ToolRegistry> toolRegistry;
    std::shared_ptr<resources::ResourceRegistry> resourceRegistry;
    std::shared_ptr<prompts::PromptRegistry> promptRegistry;
    std::weak_ptr<stdio::Transport> transport;
    bool initialized=false;
    ServerInfo serverInfo;
    ServerCapabilities capabilities;
    void initialize(std::shared_ptr<stdio::Transport> owner,String name,String version) {
        transport=owner;
        const auto self=shared_from_this();
        // Transport retains request state, not a raw server object. The reverse
        // edge is weak so stopping and releasing the transport breaks the graph.
        compat::ngRequireObject(owner)->setMessageHandler([self](Json value){self->handleMessage(value);});
        toolRegistry=std::make_shared<tools::ToolRegistry>();
        const auto weak=weak_from_this();
        promptRegistry=std::make_shared<prompts::PromptRegistry>([weak](String) {
            if(auto state=weak.lock())if(auto output=state->transport.lock();output&&state->capabilities.prompts.listChanged) {
                Object value;value["jsonrpc"]=jsonString(protocol::JSONRPC_VERSION);
                value["method"]="notifications/prompts/list_changed";output->sendMessage(value);
            }
        });
        resourceRegistry=std::make_shared<resources::ResourceRegistry>([weak](String uri) {
            if(auto state=weak.lock())if(auto output=state->transport.lock();output&&state->capabilities.resources.listChanged) {
                Object parameters;parameters["uri"]=jsonString(uri);
                Object value;value["jsonrpc"]=jsonString(protocol::JSONRPC_VERSION);
                value["method"]="notifications/resources/updated";value["params"]=parameters;output->sendMessage(value);
            }
        });
        serverInfo={name,version};capabilities={{true,false},{true},{true}};
    }
    void handleMessage(Json message) {
        auto output=transport.lock();
        if(!output)throw stdio::NullMessageHandlerError{};
        try {
            const auto request=protocol::Request::fromJSON(message);
            if(!request.isNotification()) {
                Json response;
                try {
                    if(!initialized&&request.method.view()!="initialize")
                        fail(protocol::ErrorCode::invalidRequest,text("Server not initialized"));
                    response=handleRequest(request);
                } catch(const protocol::MCPError& error) {
                    response=protocol::Response::makeError(request.id,error.code,error.msg,error.details).toJSON();
                } catch(const std::bad_alloc&) {throw;}
                  catch(const std::exception& error) {
                    response=errorResponse(request.id,protocol::ErrorCode::internalError,
                        text("Internal error: "+std::string(messageOf(error).view())));
                }
                output->sendMessage(response);
            } else handleNotification(request);
        } catch(const std::bad_alloc&) {throw;}
          catch(const std::exception& error) {
            // Source `!message.type == JSONType.object` is always false because
            // object is 6; retain the following object-membership access/error.
            const auto* id=protocol::find(message,"id");
            output->sendMessage(errorResponse(id?*id:Json{},protocol::ErrorCode::invalidRequest,
                text("Invalid request: "+std::string(messageOf(error).view()))));
        }
    }
    Json handleRequest(protocol::Request request) {
        const auto method=request.method.view();
        if(method=="initialize") {
            initialized=true;
            Object result;result["protocolVersion"]=jsonString(protocol::PROTOCOL_VERSION);
            result["capabilities"]=capabilities.toJSON();result["serverInfo"]=serverInfo.toJSON();
            return protocol::Response::success(request.id,result).toJSON();
        }
        if(method=="ping")return protocol::Response::success(request.id,Object{}).toJSON();
        if(method=="prompts/list")return protocol::Response::success(request.id,promptRegistry->listPrompts()).toJSON();
        if(method=="prompts/get") {
            const auto* name=protocol::find(request.params,"name");
            if(!name)fail(protocol::ErrorCode::invalidParams,text("Missing prompt name"));
            return protocol::Response::success(request.id,promptRegistry->getPromptContent(name->as_string(),request.params)).toJSON();
        }
        if(method=="tools/list")return protocol::Response::success(request.id,
            compat::ngRequireObject(toolRegistry)->listTools()).toJSON();
        if(method=="tools/call") {
            const auto* name=protocol::find(request.params,"name");
            const auto* arguments=protocol::find(request.params,"arguments");
            if(!name||!arguments)fail(protocol::ErrorCode::invalidParams,text("Missing tool name or arguments"));
            return protocol::Response::success(request.id,
                compat::ngRequireObject(toolRegistry)->getTool(name->as_string())->execute(*arguments)).toJSON();
        }
        if(method=="resources/list")return protocol::Response::success(request.id,resourceRegistry->listResources()).toJSON();
        if(method=="resources/templates/list")return protocol::Response::success(request.id,resourceRegistry->listTemplates()).toJSON();
        if(method=="resources/read") {
            const auto* uri=protocol::find(request.params,"uri");
            if(!uri)fail(protocol::ErrorCode::invalidParams,text("Missing resource URI"));
            const auto content=resourceRegistry->readResource(uri->as_string());
            Object result;result["contents"]=Array{content.toJSON()};
            return protocol::Response::success(request.id,result).toJSON();
        }
        fail(protocol::ErrorCode::methodNotFound,text("Method not found: "+std::string(method)));
    }
    void handleNotification(protocol::Request notification) {
        if(notification.method.view()=="notifications/initialized")return;
    }
};
MCPServer::MCPServer(std::shared_ptr<stdio::Transport> transport,String name,String version)
    :state_(std::make_shared<State>()),transport_(transport) {state_->initialize(transport,name,version);}
MCPServer::MCPServer(String name,String version):state_(std::make_shared<State>()) {
    auto transport=stdio::createStdioTransport();
    const auto state=state_;
    compat::ngRequireObject(transport)->setMessageHandler([state](Json value){state->handleMessage(value);});
    transport_=transport;
    state_->initialize(transport,name,version);
}
Json MCPServer::executeTool(String name,Json arguments){
    return compat::ngRequireObject(state_->toolRegistry)->getTool(name)->execute(arguments);
}
void MCPServer::addTool(String name,String description,schema::SchemaBuilder::Ref schema,tools::ToolHandler handler) {
    compat::ngRequireObject(state_->toolRegistry)->addTool(name,description,schema,handler);
}
void MCPServer::addPrompt(String name,String description,compat::Slice<prompts::PromptArgument> arguments,prompts::PromptHandler handler) {
    state_->promptRegistry->addPrompt({name,description,arguments},handler);
}
resources::ResourceNotifier MCPServer::addResource(String uri,String name,String description,resources::StaticResourceReader reader) {
    return state_->resourceRegistry->addResource(uri,name,description,reader);
}
resources::ResourceNotifier MCPServer::addDynamicResource(String uri,String name,String description,resources::DynamicResourceReader reader) {
    return state_->resourceRegistry->addDynamicResource(uri,name,description,reader);
}
resources::ResourceNotifier MCPServer::addTemplate(String uri,String name,String description,String mimeType,resources::TemplateResourceReader reader) {
    return state_->resourceRegistry->addTemplate(uri,name,description,mimeType,reader);
}
void MCPServer::start(){transport_->run();}
}
