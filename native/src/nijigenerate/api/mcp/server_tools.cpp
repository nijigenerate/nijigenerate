#include "server_registration.hpp"
#include "server_schema.hpp"
#include "server.hpp"
#include "helpers.hpp"
#include "task.hpp"
#include "server_log.hpp"
#include "server_state.hpp"
#include "server_internal.hpp"
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <nijigenerate/commands/concrete_catalog.hpp>
#include <nijigenerate/commands/all_command_maps.hpp>
#include <nijigenerate/commands/fields.hpp>
#include <common/resource_key_map.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::api::mcp::server::detail {
using namespace providers::mcp;
namespace {
template<std::size_t N> String literal(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
struct RegisteredNameHash {
    std::size_t operator()(const String& value) const noexcept {
        return compat::ngPhobosStringHash(value.view());
    }
};
using RegisteredNames=nicxlive::core::common::ResourceKeyMap<String,bool,
    RegisteredNameHash,core::selector::SelectorStringEqual>;
template<class T> struct McpAAKeyPattern {
    using type=void;
};
template<class K,class V,class Hash,class Equal>
struct McpAAKeyPattern<core::selector::SelectorMap<K,V,Hash,Equal>> {
    using type=K;
};
template<class AliasBinding> requires requires { AliasBinding::get(); }
struct _McpKeyTypeOfAA : McpAAKeyPattern<std::remove_cvref_t<decltype(AliasBinding::get())>> {};

[[maybe_unused]] std::shared_ptr<commands::Command> resolveCommandByString(String id) {
    std::shared_ptr<commands::Command> result;
    commands::ngForEachCommandMap([&](auto& sourceMap) {
        const auto map=sourceMap;
        for(const auto& entry:map) {
            const auto key=entry.first;
            const auto value=entry.second;
            using Key=std::remove_cvref_t<decltype(key)>;
            const auto commandId=commands::ngCommandIdFromKey(key);
            const auto keyType=commands::ngCommandKeyTypeName<Key>();
            const auto keyName=commands::ngCommandKeyName(key);
            const auto legacy=allocateMcpText([&] {return std::string(keyType)+"."+keyName;});
            if(commandId==id.view()||legacy==id.view()) result=value;
        }
    });
    return result;
}
[[maybe_unused]] compat::RetainedString listTypedMembers() {
    using K=commands::automesh::config::AutoMeshTypedCommand;
    return allocateMcpText([] {
        std::string result;
        for(auto member:commands::detail::CommandMapKeyMetadata<K>::members) {result+=member.name;result+=',';}
        return result.empty()?compat::RetainedString{}:compat::RetainedString(std::move(result));
    });
}
bool nameCharacter(unsigned char c) {
    return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c==0xaa||c==0xb2||c==0xb3||
        c==0xb5||c==0xb9||c==0xba||(c>=0xbc&&c<=0xbe)||(c>=0xc0&&c<=0xd6)||
        (c>=0xd8&&c<=0xf6)||c>=0xf8||c=='_'||c=='-';
}
String safeName(std::string_view input) {
    return allocateMcpText([&]() -> String {
        std::string result(input);
        for(auto& c:result) if(!nameCharacter(static_cast<unsigned char>(c))) c='_';
        if(result.empty()) return {};
        return compat::RetainedString(std::move(result));
    });
}
struct EnumAlias {
    bool present=false;
    std::string_view value;
};
template<class E> constexpr EnumAlias enumAlias(std::string_view name) {
    if constexpr(requires { E::descriptionChoices; }) {
        auto remaining=E::descriptionChoices;
        while(!remaining.empty()) {
            const auto end=remaining.find('|');
            const auto item=remaining.substr(0,end);
            const auto equal=item.find('=');
            if(equal!=std::string_view::npos&&item.substr(0,equal)==name&&item.substr(equal+1)!=name)
                return {true,item.substr(equal+1)};
            if(end==std::string_view::npos) break;
            remaining.remove_prefix(end+1);
        }
    }
    return {};
}
template<class F,commands::CommandLiteral Extra> String fieldSuffix() {
    // Source enum desc and its literal suffix expression are evaluated at CTFE.
    static constexpr auto bytes=[] {
        constexpr auto base=F::fieldDesc;
        constexpr auto extra=Extra.view();
        std::array<char,base.size()+(base.empty()?0:2)+extra.size()+1> result{};
        std::size_t index=0;
        for(char value:base) result[index++]=value;
        if(!base.empty()) { result[index++]=';';result[index++]=' '; }
        for(char value:extra) result[index++]=value;
        return result;
    }();
    return compat::RetainedString::fromStaticView({bytes.data(),bytes.size()-1});
}
template<class F> String fieldEnumDescription() {
    using E=windows::command_browser::detail::BrowserEnumTraits<typename F::Type>;
    static constexpr auto bytes=[] {
        constexpr auto base=F::fieldDesc;
        constexpr auto length=[] {
            std::size_t count=F::fieldDesc.size()+(F::fieldDesc.empty()?0:1)+2;
            bool first=true;
            for(const auto& member:E::members) {
                if(!first) ++count;
                first=false;
                count+=member.first.size();
                const auto alias=enumAlias<E>(member.first);
                if(alias.present) count+=1+alias.value.size();
            }
            return count;
        }();
        std::array<char,length+1> result{};
        std::size_t index=0;
        for(char value:base) result[index++]=value;
        if(!base.empty()) result[index++]=' ';
        result[index++]='(';
        bool first=true;
        for(const auto& member:E::members) {
            if(!first) result[index++]='|';
            first=false;
            for(char value:member.first) result[index++]=value;
            const auto alias=enumAlias<E>(member.first);
            if(alias.present) {
                result[index++]='=';
                for(char value:alias.value) result[index++]=value;
            }
        }
        result[index++]=')';
        return result;
    }();
    return compat::RetainedString::fromStaticView({bytes.data(),bytes.size()-1});
}
template<class F> McpSchema::Ref fieldSchema() {
    using T=typename F::Type;
    const String description=compat::RetainedString::fromStaticView(F::fieldDesc);
    if constexpr(std::same_as<T,bool>) return McpSchema::boolean()->setDescription(description);
    else if constexpr(windows::command_browser::detail::isBrowserEnum<T>) {
        using E=windows::command_browser::detail::BrowserEnumTraits<T>;
        compat::Slice<String> names;
        for(auto [name,value]:E::members) {
            names.append(compat::RetainedString::fromStaticView(name));
            const auto alias=enumAlias<E>(name);
            if(alias.present) names.append(compat::RetainedString::fromStaticView(alias.value));
        }
        return McpSchema::enum_(names)->setDescription(fieldEnumDescription<F>());
    } else if constexpr(std::is_integral_v<T>) return McpSchema::integer()->setDescription(description);
    else if constexpr(std::is_floating_point_v<T>) return McpSchema::number()->setDescription(description);
    else if constexpr(helpers::detail::stringField<T>) return McpSchema::string_()->setDescription(description);
    else if constexpr(std::same_as<T,compat::Slice<compat::RetainedString>> || std::same_as<T,compat::Slice<String>>)
        return McpSchema::array(McpSchema::string_())->setDescription(fieldSuffix<F,"string[]">());
    else if constexpr(std::same_as<T,Json>) {
        static_assert(F::jsonSchema!=commands::CommandJsonSchema::unspecified);
        return jsonArgumentSchema(F::jsonSchema,description);
    } else if constexpr(std::same_as<T,compat::Vec2u>)
        return McpSchema::array(McpSchema::integer())->setDescription(fieldSuffix<F,"vec2u [x,y]">());
    else if constexpr(std::same_as<T,nicxlive::core::math::Vec3>)
        return McpSchema::array(McpSchema::number())->setDescription(fieldSuffix<F,"vec3 [x,y,z]">());
    else if constexpr(std::same_as<T,compat::Slice<float>>)
        return McpSchema::array(McpSchema::number())->setDescription(fieldSuffix<F,"float[]">());
    else if constexpr(std::same_as<T,std::array<float,2>>)
        return McpSchema::array(McpSchema::number())->setDescription(fieldSuffix<F,"float[2] [x,y]">());
    else if constexpr(std::same_as<T,std::array<float,3>>)
        return McpSchema::array(McpSchema::number())->setDescription(fieldSuffix<F,"float[3] [x,y,z]">());
    else if constexpr(std::same_as<T,compat::Slice<std::uint16_t>>)
        return McpSchema::array(McpSchema::integer())->setDescription(fieldSuffix<F,"ushort[]">());
    else if constexpr(std::same_as<T,std::array<std::uint32_t,2>>)
        return McpSchema::array(McpSchema::integer())->setDescription(fieldSuffix<F,"uint[2] [x,y]">());
    else if constexpr(requires {typename T::element_type;}) {
        using E=typename T::element_type;
        if constexpr(std::derived_from<E,core::selector::Resource>)
            return McpSchema::integer()->setDescription(fieldSuffix<F,"UUID of Resource">());
        else if constexpr(std::derived_from<E,nicxlive::core::nodes::Node>)
            return McpSchema::integer()->setDescription(fieldSuffix<F,"UUID of Node">());
        else if constexpr(std::derived_from<E,nicxlive::core::param::Parameter>)
            return McpSchema::integer()->setDescription(fieldSuffix<F,"UUID of Parameter">());
        else return McpSchema::string_()->setDescription(fieldSuffix<F,"Unsupported type; pass as string">());
    } else return McpSchema::string_()->setDescription(fieldSuffix<F,"Unsupported type; pass as string">());
}
template<class T> constexpr std::string_view fieldLogType() {
    if constexpr(std::same_as<T,bool>) return "bool";
    else if constexpr(windows::command_browser::detail::isBrowserEnum<T>) return "enum";
    else if constexpr(std::is_integral_v<T>) return "int";
    else if constexpr(std::is_floating_point_v<T>) return "number";
    else if constexpr(helpers::detail::stringField<T>) return "string";
    else if constexpr(std::same_as<T,compat::Slice<compat::RetainedString>>||std::same_as<T,compat::Slice<String>>) return "string[]";
    else if constexpr(std::same_as<T,Json>) return "json";
    else if constexpr(std::same_as<T,compat::Vec2u>) return "vec2u";
    else if constexpr(std::same_as<T,nicxlive::core::math::Vec3>) return "vec3";
    else if constexpr(std::same_as<T,compat::Slice<float>>||std::same_as<T,compat::Slice<std::uint16_t>>) return "float[]";
    else if constexpr(std::same_as<T,std::array<float,2>>) return "float[2]";
    else if constexpr(std::same_as<T,std::array<float,3>>) return "float[3]";
    else if constexpr(std::same_as<T,std::array<std::uint32_t,2>>) return "uint[2]";
    else if constexpr(requires {typename T::element_type;}) {
        using E=typename T::element_type;
        if constexpr(std::derived_from<E,core::selector::Resource>) return "ResourceUUID";
        else if constexpr(std::derived_from<E,nicxlive::core::nodes::Node>) return "NodeUUID";
        else if constexpr(std::derived_from<E,nicxlive::core::param::Parameter>) return "ParameterUUID";
        else return "string(unknown)";
    } else return "string(unknown)";
}
template<class F> String fieldLogEntry() {
    static constexpr auto bytes=[] {
        constexpr auto name=F::fieldName;
        constexpr auto type=fieldLogType<typename F::Type>();
        std::array<char,name.size()+1+type.size()+1> result{};
        std::size_t index=0;
        for(char value:name) result[index++]=value;
        result[index++]=':';
        for(char value:type) result[index++]=value;
        return result;
    }();
    return compat::RetainedString::fromStaticView({bytes.data(),bytes.size()-1});
}
String joinParameterLog(compat::Slice<String> entries) {
    // std.array.join first measures the saved range, then allocates/copies once.
    if(entries.empty()) return {};
    std::size_t length=0;
    for(const auto& entry:entries) length+=entry.size();
    length+=(entries.size()-1)*2;
    return allocateMcpText([&]() -> String {
        std::string result(length,'\0');
        std::size_t offset=0;
        bool first=true;
        for(const auto& entry:entries) {
            if(!first) { result[offset++]=',';result[offset++]=' '; }
            first=false;
            const auto bytes=entry.view();
            std::copy(bytes.begin(),bytes.end(),result.begin()+offset);
            offset+=bytes.size();
        }
        return compat::RetainedString(std::move(result));
    });
}
template<class K> String mapLogType() {
    static constexpr auto bytes=[] {
        constexpr std::string_view key=commands::detail::CommandMapKeyMetadata<K>::typeName;
        constexpr std::string_view prefix="Command[";
        std::array<char,prefix.size()+key.size()+2> result{};
        std::size_t index=0;
        for(char value:prefix) result[index++]=value;
        for(char value:key) result[index++]=value;
        result[index]=']';
        return result;
    }();
    return compat::RetainedString::fromStaticView({bytes.data(),bytes.size()-1});
}
Json _mcpUnwrapDirectToolResult(Json resultJson) {
    if(resultJson.is_object()&&resultJson.sourceObjectNoRef().if_contains("result")) {
        auto direct=resultJson.sourceIndexValue("result");
        if(direct.is_object()&&direct.sourceObjectNoRef().if_contains("mcpDirectToolResult")&&
                direct.sourceObjectNoRef().if_contains("content")) {
            direct.sourceObjectRef().erase("mcpDirectToolResult");
            return direct;
        }
    }
    return resultJson;
}
Json encodeResult(std::shared_ptr<commands::CommandResult> result) {
    result=compat::ngRequireObject(result)->waitForCompletion();
    if(!compat::ngRequireObject(result)->succeeded) mcpLog("[MCP] command failed: %s",result->message);
    return _mcpUnwrapDirectToolResult(helpers::commandResultToJsonRuntime(result));
}
}
void registerTools(providers::mcp::server::MCPServer& server) {
    RegisteredNames registered;
    auto visit=[&]<class C>(auto key,std::shared_ptr<commands::Command> instance,std::string id,
            const String& originalDescription,std::size_t& mapAdded) {
        const String newBaseName=allocateMcpText([&] {return compat::RetainedString(std::move(id));});
        const compat::Slice<String> baseNames{newBaseName};
        const auto executionContextSchema=contextSchema();
        auto input=McpSchema::object()->setDescription(literal(
            "Input for nijigenerate Command tool. Omit context to use active app state; other parameters map to command-specific arguments."))
            ->addProperty(literal("context"),executionContextSchema->optional())->allowAdditional(true);
        compat::Slice<String> parameterLog;
        using Storage=typename decltype(commands::detail::actualExCommandStorage(static_cast<C*>(nullptr)))::type;
        if constexpr(std::is_enum_v<decltype(key)> && !std::is_void_v<C> && !std::is_void_v<Storage>) {
            std::apply([&](const auto&... fields) {([&] {
                using F=std::remove_cvref_t<decltype(fields)>;
                input->addProperty(compat::RetainedString::fromStaticView(F::fieldName),fieldSchema<F>());
                parameterLog.append(fieldLogEntry<F>());
            }(),...);},commands::CommandFields<Storage>::descriptors);
        }
        for(const auto originalName:baseNames) {
            const auto name=safeName(originalName.view());
            auto capturedName=name;
            if(registered.contains(capturedName)) {
                const auto className=safeName(commands::ngCommandSourceClassName(instance).view());
                capturedName=allocateMcpText([&] {
                    return compat::RetainedString(std::string(capturedName.view())+"_"+std::string(className.view()));
                });
                std::size_t n=1;
                while(registered.contains(capturedName)) {
                    capturedName=allocateMcpText([&] {
                        return compat::RetainedString(std::string(capturedName.view())+"_"+std::to_string(n));
                    });
                    ++n;
                }
            }
            if(!_mcpValidToolName(capturedName.view())) {
                mcpLog("[MCP][WARN] skip tool with invalid sanitized name: '%s' (base='%s')",
                    capturedName,originalName);
                continue;
            }
            registered[capturedName]=true;
            ++mapAdded;
            const String toolName=capturedName;
            mcpLog("[MCP] addTool: %s params=[%s]",toolName,joinParameterLog(parameterLog));
            const auto description=allocateMcpText([&] {return compat::RetainedString("Action: "+(originalDescription.empty()?"Run command "+std::string(toolName.view()):
                std::string(originalDescription.view()))+
                "\n\nThis endpoint may mutate app state or perform side effects.\n\nInput: optional 'context' object. Omit context to use active app state. Use context.parameterValue for parameter-axis key values.");});
            server.addTool(toolName,description,input,[instance,toolName](Json payload) {
                mcpLog("[MCP] call %s: %s",toolName,compat::ngJsonStringify(payload));
                using Result=std::shared_ptr<commands::CommandResult>;
                auto result=task::ngRunInMainThread<Result>(task::MainThreadAction<Result>([instance,toolName,payload]() -> Result {
                    const bool owned=task::ngMcpExternalCommandsBlocked();
                    const bool presentation=commands::ngIsPresentationCommand(instance);
                    if(owned&&!commands::ngCommandAllowedInCurrentContext(instance))
                        return createMcpObject<commands::CommandResult>(false,compat::RetainedString::fromStaticLiteral(
                            "AutoRig is running; external commands are blocked"));
                    if(owned&&std::dynamic_pointer_cast<commands::puppet::view::CaptureLiveScreenshotCommand>(instance)&&payload.is_object()) {
                        auto context=payload.sourceObjectRef().if_contains("context");
                        if(context&&context->is_object()&&context->sourceObjectRef().if_contains("parameterValue"))
                            return createMcpObject<commands::CommandResult>(false,compat::RetainedString::fromStaticLiteral(
                                "AutoRig is running; parameterized screenshots are blocked"));
                    }
                    auto context=owned?createMcpObject<commands::Context>():helpers::buildContextFromPayload(payload);
                    if(owned&&presentation) context->puppet(project::incActivePuppet());
                    if constexpr(!std::is_void_v<C> && !std::is_void_v<Storage>)
                        if(auto concrete=std::dynamic_pointer_cast<C>(instance)) helpers::applyPayloadToInstance(concrete,payload);
                    if(owned) return commands::ngRunCommand(instance,context);
                    ngMcpPrepareActionScopeForCurrentMode(toolName);
                    Result output;
                    compat::ngRunWithScopeExit([&] {
                        if(instance&&instance->runnable(context)) {
                            if constexpr(!std::is_void_v<C>) {
                                if(auto concrete=std::dynamic_pointer_cast<C>(instance))
                                    output=_mcpRunCommandInstance(concrete,context,toolName);
                                else output=_mcpRunCommandInstance(instance,context,toolName);
                            } else output=_mcpRunCommandInstance(instance,context,toolName);
                        } else output=createMcpObject<commands::CommandResult>(false,
                            compat::RetainedString::fromStaticLiteral("Command not runnable"));
                    }, [] {
                        ngMcpFinishActionBoundary();
                    });
                    return output;
                }));
                return encodeResult(result);
            });
        }
    };
    const auto registerMap=[&]<class AliasBinding>() {
        auto& table=AliasBinding::get();
        std::size_t mapAdded=0;
        const auto iterationTable=table;
        for(auto [key,value]:iterationTable) {
            // Preserve the source descriptor and value captures across virtual callbacks.
            if(!compat::ngRequireObject(value)->mcpExposed()) continue;
            const auto description=value->description();
            auto selected=[&]<class C>(auto selectedKey,std::shared_ptr<commands::Command> instance,std::string id) {
                visit.template operator()<C>(selectedKey,instance,std::move(id),description,mapAdded);
            };
            commands::ngVisitConcreteCommand(key,value,selected);
        }
#if defined(MCP_LOG) || defined(NIJIGENERATE_MCP_LOG)
        using Key=typename _McpKeyTypeOfAA<AliasBinding>::type;
        const auto keyName=commands::ngCommandKeyTypeName<Key>();
        mcpLog("[MCP] addTool summary: AA=%s key=%s added=%s",
            mapLogType<Key>(),compat::RetainedString::fromStaticView(keyName),mapAdded);
#endif
    };
    using Maps=commands::AllCommandMaps;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (registerMap.template operator()<std::tuple_element_t<I,Maps>>(),...);
    }(std::make_index_sequence<std::tuple_size_v<Maps>>{});
}
}
