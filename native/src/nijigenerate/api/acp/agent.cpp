#include "agent.hpp"
#include "mcp_protocol.hpp"
#include <nijigenerate/compat/default_random.hpp>
#include <nijigenerate/compat/unicode_strip.hpp>
#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <core/compat/source_mutex.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <common/resource_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <initializer_list>
#include <functional>
#include <limits>
#include <mutex>
#include <new>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

namespace nijigenerate::api::acp::agent {
namespace {
// This boundary is used only for the module's passive byte/descriptor backing.
// Provider calls and actual user handlers are evaluated outside it.
template<class Allocate> decltype(auto) allocateAgentBacking(Allocate&& allocate) {
    try {
        return std::forward<Allocate>(allocate)();
    } catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
using core::settings::SettingsJsonObject;
using core::settings::SettingsString;
using providers::mcp::protocol::Request;
using providers::mcp::protocol::Response;
bool find(const JsonValue& value, std::string_view key) {
    return value.sourceObjectNoRef().if_contains(key) != nullptr;
}
JsonValue get(const JsonValue& value, std::string_view key) { return value.sourceIndexValue(key); }
struct StringHash {
    std::size_t operator()(const String& value) const noexcept {
        return compat::ngPhobosStringHash(value.view());
    }
};
String text(std::string_view value) {
    return allocateAgentBacking([&] { return String(compat::RetainedString(value)); });
}
template<std::size_t N> String text(const char (&value)[N]) noexcept {
    return compat::RetainedString::fromStaticLiteral(value);
}
JsonValue stringValue(String value) { return JsonValue(SettingsString(std::move(value))); }
struct LiteralKey {
    compat::RetainedString text;
    template<std::size_t N> LiteralKey(const char (&value)[N]) noexcept
        : text(compat::RetainedString::fromStaticLiteral(value)) {}
};
JsonValue object(std::initializer_list<std::pair<LiteralKey, JsonValue>> members) {
    SettingsJsonObject result;
    for (const auto& [key, value] : members) result[SettingsString(key.text)] = value;
    return JsonValue(result);
}
std::size_t toSize(const JsonValue& value) {
    const auto number = value.as_int64();
    if (number < 0) throw compat::PhobosConvOverflowException(
        text("Conversion negative overflow").retainedString(), text("std/conv.d").retainedString(), 1581);
    if (static_cast<std::uint64_t>(number) > std::numeric_limits<std::size_t>::max())
        throw compat::PhobosConvOverflowException(
            text("Conversion positive overflow").retainedString(), text("std/conv.d").retainedString(), 1587);
    return static_cast<std::size_t>(number);
}
void requireUnordered(const JsonValue& value) {
    (void)value.sourceObjectRef();
}
}

struct ACPAgentState : std::enable_shared_from_this<ACPAgentState> {
    struct SessionState {
        bool promptActive = false;
        bool cancelRequested = false;
        JsonValue pendingPromptId;
    };
    // The receiver owns the transport; weak state edges do not add extra ownership cycles.
    // The transport's strong receiver delegate is released explicitly by ACPAgent::ngClose.
    std::weak_ptr<transport::stdio::Transport> transport;
    std::weak_ptr<ACPAgent> receiver;
    String name;
    String version;
    bool initialized = false;
    template<class Value> using SessionMap = nicxlive::core::common::ResourceKeyMap<
        String, Value, StringHash, std::equal_to<String>>;
    SessionMap<String> sessionOrder;
    SessionMap<SessionState> sessions;
    std::shared_ptr<::nicxlive::core::d_runtime::SourceMutex> stateMutex;
    ApplyEditHandler onApplyEdit;
    PromptHandler onPrompt;

    std::shared_ptr<SessionState> sessionCell(const String& key) {
        auto current = sessions.find(key);
        if (current == sessions.end()) {
            sessions[key] = SessionState{};
            current = sessions.find(key);
        }
        return current.retainValue();
    }

    std::shared_ptr<transport::stdio::Transport> activeTransport() {
        return compat::ngRequireObject(transport.lock());
    }
    void emitSessionUpdate(String sessionId, JsonValue update) {
        activeTransport()->sendMessage(object({
            {"jsonrpc", stringValue(protocol::JSONRPC_VERSION)},
            {"method", stringValue(protocol::ACP_METHOD_SESSION_UPDATE)},
            {"params", object({{"sessionId", stringValue(sessionId)}, {"update", std::move(update)}})}
        }));
    }
    void sendSessionUpdate(String sessionId, JsonValue update) {
        const auto owner = compat::ngRequireObject(receiver.lock());
        owner->sendSessionUpdate(std::move(sessionId), std::move(update));
    }
    void handleMessage(JsonValue message) {
        try {
            auto request = Request::fromJSON(std::move(message));
            if (!request.isNotification()) activeTransport()->sendMessage(handleRequest(std::move(request)));
            else handleNotification(std::move(request));
        } catch (const compat::ThrowableException& error) {
            activeTransport()->sendMessage(Response::makeError({}, -32600, error.msg).toJSON());
        }
    }
    JsonValue handleRequest(Request request) {
        const auto method = request.method.view();
        if (method == "initialize") {
            initialized = true;
            return Response::success(request.id, object({
                {"protocolVersion", JsonValue(protocol::ACP_PROTOCOL_VERSION)},
                {"agentCapabilities", object({
                    {"loadSession", JsonValue(false)},
                    {"promptCapabilities", object({{"image", JsonValue(false)},
                        {"audio", JsonValue(false)}, {"embeddedContext", JsonValue(false)}})},
                    {"mcpCapabilities", object({{"http", JsonValue(false)}, {"sse", JsonValue(false)}})},
                    {"sessionCapabilities", JsonValue(SettingsJsonObject{})}})},
                {"agentInfo", object({{"name", stringValue(name)}, {"title", stringValue(name)},
                    {"version", stringValue(version)}})},
                {"serverInfo", object({{"name", stringValue(name)}, {"version", stringValue(version)}})}
            })).toJSON();
        }
        if (method == "ping") {
            autorig::json::detail::FirstPassReader reader{text("{}")};
            return Response::success(request.id, reader.parseRoot()).toJSON();
        }
        if (method == "session/new") {
            if (!initialized) return Response::makeError(request.id, -32600, text("Agent not initialized")).toJSON();
            const auto sessionId = newSessionId();
            compat::ngRequireObject(stateMutex)->lock();
            sessions[sessionId] = SessionState{};
            sessionOrder[sessionId] = sessionId;
            compat::ngRequireObject(stateMutex)->unlock();
            return Response::success(request.id, object({{"sessionId", stringValue(sessionId)}})).toJSON();
        }
        if (method == "session/prompt") {
            if (!initialized) return Response::makeError(request.id, -32600, text("Agent not initialized")).toJSON();
            auto params = request.params;
            if (!find(params, "sessionId") || !get(params, "sessionId").is_string())
                return Response::makeError(request.id, -32602, text("Missing sessionId")).toJSON();
            if (!find(params, "prompt") || !get(params, "prompt").is_array())
                return Response::makeError(request.id, -32602, text("Missing prompt")).toJSON();
            String sessionId = get(params, "sessionId").as_string();
            const auto key = sessionId;
            compat::ngRequireObject(stateMutex)->lock();
            auto found = sessions.find(key);
            if (found == sessions.end()) {
                compat::ngRequireObject(stateMutex)->unlock();
                return Response::makeError(request.id, -32602, text("Unknown sessionId")).toJSON();
            }
            if (sessionCell(key)->promptActive) {
                compat::ngRequireObject(stateMutex)->unlock();
                return Response::makeError(request.id, -32600, text("Prompt already active for session")).toJSON();
            }
            sessionCell(key)->promptActive = true;
            sessionCell(key)->cancelRequested = false;
            sessionCell(key)->pendingPromptId = request.id;
            compat::ngRequireObject(stateMutex)->unlock();
            const auto sourceBlocks=get(params,"prompt").as_array();
            auto blocks=allocateAgentBacking([&] { return sourceBlocks.duplicate(); });
            auto promptText = renderPromptText(blocks);
            sendSessionUpdate(sessionId, object({
                {"sessionUpdate", stringValue(text("agent_message_chunk"))},
                {"content", object({{"type", stringValue(text("text"))}, {"text", stringValue(promptText)}})}
            }));
            bool cancelled = false;
            auto reply = buildPromptReply(sessionId, blocks, cancelled);
            if (!reply.empty() && !cancelled) {
                sendSessionUpdate(sessionId, object({
                    {"sessionUpdate", stringValue(text("agent_message_chunk"))},
                    {"content", object({{"type", stringValue(text("text"))}, {"text", stringValue(reply)}})}
                }));
            }
            compat::ngRequireObject(stateMutex)->lock();
            sessionCell(key)->promptActive = false;
            sessionCell(key)->pendingPromptId = JsonValue{};
            cancelled = cancelled || sessionCell(key)->cancelRequested;
            sessionCell(key)->cancelRequested = false;
            compat::ngRequireObject(stateMutex)->unlock();
            return Response::success(request.id,
                object({{"stopReason", stringValue(cancelled ? text("cancelled") : text("end_turn"))}})).toJSON();
        }
        if (method == "workspace/applyEdit") {
            if (!initialized) return Response::makeError(request.id, -32600, text("Agent not initialized")).toJSON();
            if (!onApplyEdit) return Response::makeError(request.id, -32601, text("applyEdit handler not set")).toJSON();
            auto params = request.params;
            if (!find(params, "uri") || !find(params, "edits"))
                return Response::makeError(request.id, -32602, text("Missing uri/edits")).toJSON();
            types::WorkspaceEdit edit;
            edit.uri = get(params, "uri").as_string();
            const auto sourceEdits = get(params, "edits").sourceArrayValue();
            for (auto item : sourceEdits) {
                types::TextEdit textEdit;
                textEdit.range.start.line = toSize(get(get(get(item, "range"), "start"), "line"));
                textEdit.range.start.character = toSize(get(get(get(item, "range"), "start"), "character"));
                textEdit.range.end.line = toSize(get(get(get(item, "range"), "end"), "line"));
                textEdit.range.end.character = toSize(get(get(get(item, "range"), "end"), "character"));
                textEdit.newText = get(item, "newText").as_string();
                edit.edits.append(std::move(textEdit));
            }
            const auto applyHandler = onApplyEdit;
            const bool applied = applyHandler(edit);
            return Response::success(request.id, object({{"applied", JsonValue(applied)}})).toJSON();
        }
        if (method == "editor/userMessage") {
            if (!initialized) return Response::makeError(request.id, -32600, text("Agent not initialized")).toJSON();
            auto params = request.params;
            const String message = find(params, "text") && get(params, "text").is_string()
                ? String(get(params, "text").as_string()) : text("");
            return Response::success(request.id, object({{"received", stringValue(message)}})).toJSON();
        }
        const auto message=allocateAgentBacking([&] {
            return String(compat::RetainedString("Unknown method: "+std::string(method)));
        });
        return Response::makeError(request.id,-32601,message).toJSON();
    }
    void handleNotification(Request notification) {
        if (notification.method.view() != "session/cancel") return;
        auto params = notification.params;
        if (find(params, "sessionId") && get(params, "sessionId").is_string()) {
            const String sessionId = get(params, "sessionId").as_string();
            compat::ngRequireObject(stateMutex)->lock();
            if (sessions.contains(sessionId)) sessionCell(sessionId)->cancelRequested = true;
            compat::ngRequireObject(stateMutex)->unlock();
        }
    }
    String newSessionId() {
        const auto value=compat::ngRandomUlongExcludingMax();
        return allocateAgentBacking([&] {
            return String(compat::RetainedString("sess_"+std::to_string(value)));
        });
    }
    String renderPromptText(PromptBlocks blocks) {
        compat::Slice<String> parts;
        for (auto block : blocks) {
            if (!block.is_object()) continue;
            requireUnordered(block);
            if (!find(block, "type")) continue;
            const String kind = get(block, "type").as_string();
            if (kind.view() == "text" && find(block, "text") && get(block, "text").is_string()) {
                parts.append(get(block, "text").as_string());
                continue;
            }
            if ((kind.view() == "resource_link" || kind.view() == "resourceLink") && find(block, "uri") &&
                get(block, "uri").is_string()) {
                const String uri = get(block, "uri").as_string();
                const auto resourceText=allocateAgentBacking([&] {
                    std::string bytes;
                    bytes.reserve(sizeof("[resource] ")-1+uri.size());
                    bytes.append("[resource] ");
                    bytes.append(uri.view());
                    return String(compat::RetainedString(std::move(bytes)));
                });
                parts.append(resourceText);
                continue;
            }
            if (kind.view() == "resource" && find(block, "resource") && get(block, "resource").is_object()) {
                auto resource = get(block, "resource");
                requireUnordered(resource);
                if (find(resource, "uri") && get(resource, "uri").is_string()) {
                    const String uri = get(resource, "uri").as_string();
                    const auto resourceText=allocateAgentBacking([&] {
                        std::string bytes;
                        bytes.reserve(sizeof("[resource] ")-1+uri.size());
                        bytes.append("[resource] ");
                        bytes.append(uri.view());
                        return String(compat::RetainedString(std::move(bytes)));
                    });
                    parts.append(resourceText);
                    continue;
                }
            }
        }
        std::string joined;
        std::size_t length = parts.empty() ? 0 : parts.size() - 1;
        for (const auto part : parts) length += part.size();
        allocateAgentBacking([&] {
            joined.reserve(length);
            for (std::size_t i=0;i<parts.size();++i) {
                if(i)joined+='\n';
                joined+=parts[i].view();
            }
        });
        const auto stripped = compat::ngStripUnicode(joined);
        if (stripped.empty()) return text("Received prompt.");
        return allocateAgentBacking([&] {
            std::string result;
            result.reserve(sizeof("Received prompt:\n")-1+stripped.size());
            result.append("Received prompt:\n");
            result.append(stripped);
            return String(compat::RetainedString(std::move(result)));
        });
    }
    String buildPromptReply(String sessionId, PromptBlocks blocks, bool& cancelled) {
        cancelled = false;
        const auto self = shared_from_this();
        CancellationHandler isCancelled = [self, owner = receiver.lock(), key = sessionId] {
            compat::ngRequireObject(self->stateMutex)->lock();
            bool result = false;
            compat::ngRunWithScopeExit([&] {
                const auto found = self->sessions.find(key);
                result = found != self->sessions.end() && found->second.cancelRequested;
            }, [&] { compat::ngRequireObject(self->stateMutex)->unlock(); });
            return result;
        };
        if (isCancelled()) { cancelled = true; return text(""); }
        if (onPrompt) {
            const auto promptHandler = onPrompt;
            auto result = promptHandler(sessionId, blocks, isCancelled);
            if (isCancelled()) { cancelled = true; return text(""); }
            return result;
        }
        return text("Prompt processed.");
    }
};

} // namespace nijigenerate::api::acp::agent

namespace nicxlive::core::d_runtime {
// This actual record represents the private declaration inside Source ACPAgent;
// ACPAgentState is only the preexisting native lifetime adapter, not its origin.
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::agent::ACPAgentState::SessionState> {
    inline static constexpr bool isNativeObject=false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.api.acp.agent.ACPAgent.SessionState)"),
            RetainedString::fromStaticLiteral("(SessionState)"),
            RetainedString::fromStaticLiteral("nijigenerate.api.acp.agent.ACPAgent.SessionState"),
            RetainedString::fromStaticLiteral("SessionState")};
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::api::acp::agent {
ACPAgent::ACPAgent()
    : state_(std::allocate_shared<ACPAgentState>(
          nicxlive::core::d_runtime::SourceAllocator<ACPAgentState>{})),
      onApplyEdit(state_->onApplyEdit), onPrompt(state_->onPrompt) {
    using namespace nicxlive::core::d_runtime;
    using Names=SourceObjectTypeNames<ACPAgent>;
    ngPrepareSourceTypeSpellings<ACPAgent>();
    ngRegisterObjectTypeNames<ACPAgent>(compat::RetainedString::fromStaticLiteral(Names::className),
        compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ACPAgent),
        compat::RetainedString::fromStaticLiteral("nijigenerate.api.acp.agent.ACPAgent"));
}
compat::RetainedString ACPAgent::toString() {
    const auto lifetime = weak_from_this().lock();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t ACPAgent::toHash() {
    const auto lifetime=weak_from_this().lock();
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool ACPAgent::opEquals(std::shared_ptr<ACPAgent> other) {
    const auto lifetime=weak_from_this().lock();
    return other && dynamic_cast<const void*>(other.get()) == dynamic_cast<const void*>(this);
}
int ACPAgent::opCmp(std::shared_ptr<ACPAgent>) {
    const auto lifetime = weak_from_this().lock();
    const auto className=nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message=allocateAgentBacking([&] {
        std::string bytes="need opCmp for class ";
        bytes+=className.view();
        return compat::RetainedString(std::move(bytes));
    });
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
void ACPAgent::initialize(std::shared_ptr<transport::stdio::Transport> transport, String name, String version) {
    const auto self = shared_from_this();
    state_->receiver = self;
    transport_ = transport;
    state_->transport = transport;
    state_->name = std::move(name);
    state_->version = std::move(version);
    state_->stateMutex=std::allocate_shared<::nicxlive::core::d_runtime::SourceMutex>(
        nicxlive::core::d_runtime::SourceAllocator<::nicxlive::core::d_runtime::SourceMutex>{});
    compat::ngRequireObject(transport)->setMessageHandler([self](JsonValue message) {
        self->state_->handleMessage(std::move(message));
    });
}
void ACPAgent::initialize(String name, String version) {
    initialize(transport::stdio::createStdioTransport(), std::move(name), std::move(version));
}
std::shared_ptr<ACPAgent> ACPAgent::ngCreate(std::shared_ptr<transport::stdio::Transport> transport,
    String name, String version) {
    return ngCreateDerived<ACPAgent>(std::move(transport),std::move(name),std::move(version));
}
std::shared_ptr<ACPAgent> ACPAgent::ngCreate(String name, String version) {
    return ngCreateDerived<ACPAgent>(std::move(name),std::move(version));
}
void ACPAgent::ngClose() {
    // Keep this alive while removing the transport's strong receiver reference.
    const auto self = shared_from_this();
    const auto release = [&] {
        transport_->setMessageHandler({});
        state_->onApplyEdit = {};
        state_->onPrompt = {};
    };
    compat::ngRunWithScopeExit([&] { compat::ngRequireObject(transport_)->close(); }, release);
}
void ACPAgent::start() {
    const auto lifetime = weak_from_this().lock();
    transport_->run();
}
void ACPAgent::sendStatus(types::StatusNotification notification) {
    const auto lifetime = weak_from_this().lock();
    transport_->sendMessage(object({{"jsonrpc", stringValue(protocol::JSONRPC_VERSION)},
        {"method", stringValue(protocol::ACP_METHOD_STATUS)},
        {"params", object({{"title", stringValue(notification.title)},
            {"message", stringValue(notification.message)},
            {"level", JsonValue(static_cast<std::int32_t>(notification.level))}})}}));
}
void ACPAgent::sendSessionUpdate(String sessionId, JsonValue update) {
    const auto lifetime = weak_from_this().lock();
    state_->emitSessionUpdate(std::move(sessionId), std::move(update));
}
}
