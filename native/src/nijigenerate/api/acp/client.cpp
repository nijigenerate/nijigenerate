#include "client.hpp"
#include "protocol.hpp"
#include "mcp_protocol.hpp"
#include "detail/client_storage.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/exception_message.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/thread_exception.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/source_condition.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/utils/crashdump.hpp>
#include <chrono>
#include <new>
#include <stdexcept>
#include <system_error>

namespace nijigenerate::api::acp::client {
namespace {
using core::settings::SettingsJsonObject;
using providers::mcp::protocol::Request;
using namespace std::chrono_literals;
using detail::allocateClientStorage;
using detail::concatenateClientBytes;
using detail::concatenateClientText;
using detail::retainClientText;
using ClientException = compat::ThrowableException;
ClientException sourceException(compat::RetainedString message, std::size_t line) {
    return ClientException(std::move(message),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/api/acp/client.d"), line);
}
JsonValue* find(JsonValue& value, std::string_view key) {
    return value.sourceObjectRef().if_contains(key);
}
JsonValue get(const JsonValue& value, std::string_view key) {
    return value.sourceIndexValue(key);
}
template<std::size_t N> core::settings::SettingsString literal(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
std::string text(String value) { return detail::copyClientText(value.view()); }
JsonValue parse(String value) {
    // Match default std.json.parseJSON: parseValue also diagnoses empty input.
    autorig::json::detail::FirstPassReader reader(value);
    JsonValue result;
    reader.parseValue(result);
    return result;
}
template<std::size_t N> JsonValue parse(const char (&value)[N]) {
    return parse(compat::RetainedString::fromStaticLiteral(value));
}
std::string json(JsonValue value) { return text(compat::ngJsonStringify(value)); }
String stringValue(const JsonValue& value) {
    return value.as_string();
}
std::string base64(std::string_view bytes) try {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        auto value = std::uint32_t(static_cast<unsigned char>(bytes[i])) << 16;
        if (i + 1 < bytes.size()) value |= std::uint32_t(static_cast<unsigned char>(bytes[i + 1])) << 8;
        if (i + 2 < bytes.size()) value |= static_cast<unsigned char>(bytes[i + 2]);
        result += alphabet[value >> 18]; result += alphabet[(value >> 12) & 63];
        result += i + 1 < bytes.size() ? alphabet[(value >> 6) & 63] : '=';
        result += i + 2 < bytes.size() ? alphabet[value & 63] : '=';
    }
    return result;
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}
String parseFailureMessage(std::string_view payload, String error) {
    const auto encoded = base64(payload);
    const auto head = detail::duplicateClientText(std::string_view(encoded).substr(0, 600));
    const auto tail = encoded.size() > 600
        ? detail::duplicateClientText(std::string_view(encoded).substr(encoded.size() - 600))
        : compat::RetainedString::emptyLiteral();
    const auto length = allocateClientStorage([&] { return std::to_string(payload.size()); });
    auto result = concatenateClientText({"JSON parse failed: ", error.view(), " | raw.len=", length,
        " | raw.b64.head=", head.view()});
    if (!tail.empty()) result = concatenateClientText({result.view(), " | raw.b64.tail=", tail.view()});
    return result;
}
compat::ConstSlice<std::uint8_t> bytes(String value) {
    if (value.isNull()) return {};
    const auto backing = value.retainedString();
    if (const auto owner = backing.retentionOwner()) {
        return compat::ConstSlice<std::uint8_t>::retainedView(owner,
            reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
    }
    auto descriptor = std::allocate_shared<String>(nicxlive::core::d_runtime::SourceAllocator<String>{}, value);
    return compat::ConstSlice<std::uint8_t>::retainedView(descriptor,
        reinterpret_cast<const std::uint8_t*>(descriptor->data()), descriptor->size());
}
String byteString(compat::Slice<std::uint8_t> value) {
    if (value.isNull()) return {};
    if (const auto owner = value.retentionOwner()) {
        return compat::RetainedString::retainedView(owner,
            reinterpret_cast<const char*>(value.data()), value.size());
    }
    auto descriptor = std::allocate_shared<compat::Slice<std::uint8_t>>(
        nicxlive::core::d_runtime::SourceAllocator<compat::Slice<std::uint8_t>>{}, value);
    return compat::RetainedString::retainedView(descriptor,
        reinterpret_cast<const char*>(descriptor->data()), descriptor->size());
}
}
struct ACPClient::NativeThread {
    std::thread worker;
    std::exception_ptr failure;
    ~NativeThread() {
        // A daemon worker retains its receiver and this real native handle.
        if (worker.joinable()) worker.detach();
    }
    void join() {
        if (worker.joinable()) detail::ngJoinClientThread(worker);
        // Preserve the failed join outcome until close successfully clears it.
        if (failure) std::rethrow_exception(failure);
    }
};
ACPClient::ACPClient(std::shared_ptr<StdioTransport> transport) : transport_(std::move(transport)) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ACPClient),
        compat::RetainedString::fromStaticLiteral("nijigenerate.api.acp.client.ACPClient"));
}
ACPClient::ACPClient(compat::Slice<String> command, String cwd) : pipes_(detail::ngLaunchClient(command, cwd)) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ACPClient),
        compat::RetainedString::fromStaticLiteral("nijigenerate.api.acp.client.ACPClient"));
    transport_ = std::allocate_shared<StdioTransport>(SourceAllocator<StdioTransport>{}, pipes_.output, pipes_.input);
    ownsProcess_=true;
}
std::shared_ptr<ACPClient> ACPClient::ngCreate(std::shared_ptr<StdioTransport> transport) {
    return ngCreateDerived<ACPClient>(std::move(transport));
}
std::shared_ptr<ACPClient> ACPClient::ngCreate(compat::Slice<String> command, String cwd) {
    return ngCreateDerived<ACPClient>(std::move(command), std::move(cwd));
}
ACPClient::~ACPClient() = default;
compat::RetainedString ACPClient::toString() {
    const auto lifetime = weak_from_this().lock();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t ACPClient::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool ACPClient::opEquals(std::shared_ptr<ACPClient> other) {
    return other && dynamic_cast<const void*>(other.get()) == dynamic_cast<const void*>(this);
}
int ACPClient::opCmp(std::shared_ptr<ACPClient>) {
    const auto lifetime = weak_from_this().lock();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto errorMessage = concatenateClientText({"need opCmp for class ", name.view()});
    throw compat::ThrowableException(errorMessage,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
String ACPClient::getInitError() { return initError_; }
JsonValue ACPClient::getInitResult() { return initResult_; }
detail::Pid ACPClient::getPid() { return pipes_.pid; }
int ACPClient::getStdoutFd() { return stdoutFd(); }
std::ptrdiff_t ACPClient::findByteIndex(compat::ConstSlice<std::uint8_t> value, std::uint8_t needle) {
    const auto keepAlive=shared_from_this();
    return detail::findByteIndex(value, needle);
}
compat::Slice<std::uint8_t> ACPClient::truncateAtNull(compat::Slice<std::uint8_t> value) {
    const auto keepAlive=shared_from_this();
    return detail::truncateAtNull(value);
}
String ACPClient::sanitizeJsonPayload(String value) { return detail::sanitizeJsonPayload(value); }
String ACPClient::escapeJsonString(String value) { return detail::escapeJsonString(value); }
bool ACPClient::tryDecodeEscapedJsonString(String payload, JsonValue& result) {
    const auto keepAlive=shared_from_this();
    result = {};
    if (payload.empty()) return false;
    auto candidate = payload;
    const auto value = payload.view();
    if (value[0] == '\\') {
        if (value.size() < 2) return false;
        if (value[1] == '{' || value[1] == '[') {}
        else if (value[1] == '"') {
            if (value.size() < 3 || (value[2] != '{' && value[2] != '[')) return false;
        } else return false;
        candidate = concatenateClientText({"\"", payload.view(), "\""});
    } else if (value[0] != '"') return false;
    JsonValue temporary;
    try { temporary = parse(candidate); }
    catch (const std::bad_alloc&) { throw; }
    catch (const compat::ThrowableException&) { return false; }
    if (!temporary.is_string()) return false;
    try { result = parse(stringValue(temporary)); return true; }
    catch (const std::bad_alloc&) { throw; }
    catch (const compat::ThrowableException&) { return false; }
}
void ACPClient::logParseFailure(String payload, String error) {
    const auto keepAlive=shared_from_this();
    log(parseFailureMessage(payload.view(), error));
}
void ACPClient::logParseFailureBytes(compat::ConstSlice<std::uint8_t> payload, String error) {
    const auto keepAlive=shared_from_this();
    const auto raw = payload.empty() ? std::string_view{} :
        std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size());
    log(parseFailureMessage(raw, error));
}
ACPClient::PollResult ACPClient::decodeJson(String payload) {
    const auto keepAlive=shared_from_this();
    PollResult result;
    if (payload.empty()) return result;
    try {
        auto value = parse(payload);
        log(concatenateClientText({"recv: ", payload.view()}));
        result.result = value; result.hasValue = true;
    } catch (const std::bad_alloc&) { throw; }
    catch (const compat::ThrowableException& error) {
        JsonValue decoded;
        if (tryDecodeEscapedJsonString(payload, decoded)) {
            const auto decodedText = json(decoded);
            log(concatenateClientText({"recv: ", decodedText}));
            result.result = decoded; result.hasValue = true; return result;
        }
        bool recovered = false;
        try {
            auto sanitized = sanitizeJsonPayload(payload);
            auto value = parse(sanitized);
            log(concatenateClientText({"recv: ", sanitized.view()}));
            result.result = value; result.hasValue = true; recovered = true;
        } catch (const std::bad_alloc&) { throw; }
        catch (const compat::ThrowableException&) {}
        if (!recovered) {
            auto stderrText = drainStderr();
            const auto errorText = error.msg;
            const auto failure = concatenateClientText({"JSON parse failed: ", errorText.view()});
            logParseFailure(payload,error.msg);
            if (!stderrText.empty()) log(concatenateClientText({"stderr: ", stderrText.view()}));
            const auto snapshot = stateSnapshot();
            log(concatenateClientText({"state: ", snapshot.view()}));
            result.error = failure; result.hasValue = true;
        }
    }
    return result;
}
void ACPClient::setDebugStdout(bool value) { debugStdout_ = value; }
void ACPClient::setDebugEcho(bool value) { debugEcho_ = value; }
void ACPClient::setLogger(compat::SharedDelegate<void(String)> value) {
    const auto keepAlive=shared_from_this();
    std::lock_guard lock(callbackMutex_);
    logger_ = value;
}
void ACPClient::setCancelCheck(compat::SharedDelegate<bool()> value) {
    const auto keepAlive=shared_from_this();
    std::lock_guard lock(callbackMutex_);
    cancelCheck_ = value;
}
void ACPClient::log(String value) {
    const auto keepAlive=shared_from_this();
    compat::SharedDelegate<void(String)> logger;
    { std::lock_guard lock(callbackMutex_); logger = logger_; }
    if (logger) logger(value);
}
void ACPClient::setSendMode(String mode) {
    const auto keepAlive=shared_from_this();
    sendMode_ = mode.view() == "content-length" ? compat::RetainedString::fromStaticLiteral("content-length") :
        compat::RetainedString::fromStaticLiteral("line");
}
void ACPClient::sendRawLine(String raw) {
    const auto keepAlive=shared_from_this();
    try {
        pipes_.input.write(concatenateClientBytes({raw.view(), "\n"})); pipes_.input.flush();
        if (debugEcho_) log(concatenateClientText({"[stdin-raw] ", raw.view()}));
    } catch (const std::bad_alloc&) { throw; }
    catch (const compat::ThrowableException& error) {
        auto stderrText = drainStderr();
        const auto errorText = error.msg;
        auto failure = concatenateClientText({"sendRaw failed: ", errorText.view()});
        if (!stderrText.empty()) failure = concatenateClientText({failure.view(), " | stderr: ", stderrText.view()});
        const auto snapshot = stateSnapshot();
        failure = concatenateClientText({failure.view(), " | state: ", snapshot.view()});
        log(failure); throw sourceException(failure, 313);
    }
}
String ACPClient::sendPrompt(String value) {
    const auto keepAlive=shared_from_this();
    if (sessionId_.empty()) newSession();
    const auto next = nextId_++;
    const auto id = retainClientText(allocateClientStorage([&] { return std::to_string(next); }));
    const auto escaped=escapeJsonString(value);
    const auto raw = concatenateClientText({"{\"jsonrpc\":\"2.0\",\"id\":", id.view(),
        ",\"method\":\"session/prompt\",\"params\":{\"sessionId\":\"", sessionId_.view(),
        "\",\"prompt\":[{\"type\":\"text\",\"text\":\"", escaped.view(), "\"}]}}"});
    sendRawLine(raw); return id;
}
void ACPClient::cancelPrompt() {
    const auto keepAlive=shared_from_this();
    if (sessionId_.empty()) return;
    sendRawLine(concatenateClientText({
        "{\"jsonrpc\":\"2.0\",\"method\":\"session/cancel\",\"params\":{\"sessionId\":\"",
        sessionId_.view(), "\"}}"}));
}
void ACPClient::sendPermissionResponse(String id, String option, bool cancelled) {
    const auto keepAlive=shared_from_this();
    if (id.empty() || id.view() == "null") throw sourceException(
        compat::RetainedString::fromStaticLiteral("permission response missing request id"), 360);
    compat::RetainedString raw;
    if (cancelled) raw=concatenateClientText({"{\"jsonrpc\":\"2.0\",\"id\":", id.view(),
        ",\"result\":{\"outcome\":{\"outcome\":\"cancelled\"}}}"});
    else {
        if (option.empty()) throw sourceException(
            compat::RetainedString::fromStaticLiteral("permission response missing option id"), 367);
        const auto escaped=escapeJsonString(option);
        raw=concatenateClientText({"{\"jsonrpc\":\"2.0\",\"id\":", id.view(),
            ",\"result\":{\"outcome\":{\"outcome\":\"selected\",\"optionId\":\"", escaped.view(), "\"}}}"});
    }
    sendRawLine(raw);
}
void ACPClient::sendPermissionResponse(String id, bool granted) {
    const auto keepAlive=shared_from_this();
    sendPermissionResponse(id, granted ? literal("allow-once") : literal(""), !granted);
}
void ACPClient::startReader() {
    const auto keepAlive=shared_from_this();
    inboundMutex_ = std::allocate_shared<nicxlive::core::d_runtime::SourceMutex>(
        SourceAllocator<nicxlive::core::d_runtime::SourceMutex>{});
    inboundCondition_ = std::allocate_shared<nicxlive::core::d_runtime::SourceCondition>(
        SourceAllocator<nicxlive::core::d_runtime::SourceCondition>{}, inboundMutex_);
    readerRunning_ = true;
    auto thread = std::allocate_shared<NativeThread>(SourceAllocator<NativeThread>{});
    reader_ = thread;
    try {
        thread->worker = std::thread([self = shared_from_this(), thread] {
            try {
                core::thread_context::ManagedThreadScope sourceThread;
                utils::crashdump::installNativeCrashDumpThreadHandler();
                self->readerLoop();
            }
            catch (...) { thread->failure = std::current_exception(); }
        });
    } catch (const std::system_error&) {
        compat::ngOnThreadError(compat::RetainedString::fromStaticLiteral("Error creating thread"));
    } catch (const std::bad_alloc&) {
        // This fixed native thunk captures only shared owners; it never invokes
        // the user callback synchronously in the launching constructor.
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
void ACPClient::startStderrReader() {
    const auto keepAlive=shared_from_this();
    stderrRunning_ = true;
    auto thread = std::allocate_shared<NativeThread>(SourceAllocator<NativeThread>{});
    stderrThread_ = thread;
    try {
        thread->worker = std::thread([self = shared_from_this(), thread] {
            try {
                core::thread_context::ManagedThreadScope sourceThread;
                utils::crashdump::installNativeCrashDumpThreadHandler();
                self->stderrLoop();
            }
            catch (...) { thread->failure = std::current_exception(); }
        });
    } catch (const std::system_error&) {
        compat::ngOnThreadError(compat::RetainedString::fromStaticLiteral("Error creating thread"));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::Slice<ACPClient::PollResult> ACPClient::takeInbound() {
    const auto keepAlive=shared_from_this();
    compat::ngRequireObject(inboundMutex_)->lock();
    auto result = inboundQueue_.duplicate();
    inboundQueue_.resize(0);
    compat::ngRequireObject(inboundMutex_)->unlock();
    return result;
}
JsonValue ACPClient::initialize() {
    const auto keepAlive=shared_from_this();
    const auto params=buildInitParams();
    const auto id=JsonValue(std::uint64_t(nextId_++));
    send(Request(protocol::ACP_METHOD_INITIALIZE,params,id).toJSON());
    return waitResponse();
}
void ACPClient::newSession() {
    const auto keepAlive=shared_from_this();
    const auto next = nextId_++;
    const auto id = retainClientText(allocateClientStorage([&] { return std::to_string(next); }));
    const auto cwd = detail::ngCurrentDirectory();
    const auto servers = currentMcpServers();
    const auto escapedDirectory = escapeJsonString(cwd);
    const auto serverText = json(servers);
    sendRawLine(concatenateClientText({"{\"jsonrpc\":\"2.0\",\"id\":", id.view(),
        ",\"method\":\"session/new\",\"params\":{\"cwd\":\"", escapedDirectory.view(),
        "\",\"mcpServers\":", serverText, "}}"}));
    auto response = waitResponse();
    auto result = get(response, "result");
    if (result.is_object() && find(result, "sessionId")) {
        sessionId_ = stringValue(get(result, "sessionId"));
        log(concatenateClientText({"session/new -> ", sessionId_.view()}));
    } else if (find(response, "error")) {
        const auto errorText = json(get(response, "error"));
        log(concatenateClientText({"session/new error: ", errorText}));
        throw sourceException(concatenateClientText({"session/new error: ", errorText}), 435);
    } else throw sourceException(compat::RetainedString::fromStaticLiteral("session/new missing sessionId"), 437);
}
void ACPClient::initializeAsync() {
    const auto keepAlive=shared_from_this();
    initPending_ = true; initResult_ = {}; initError_ = {};
    const auto params=buildInitParams();
    const auto id=JsonValue(std::uint64_t(nextId_++));
    send(Request(protocol::ACP_METHOD_INITIALIZE,params,id).toJSON());
}
bool ACPClient::pollInitialize() {
    const auto keepAlive=shared_from_this();
    if (!initPending_) return true;
    auto value = popInbound(); if (!value.hasValue) return false;
    if (!value.error.empty()) initError_ = value.error; else initResult_ = value.result;
    initPending_ = false; return true;
}
JsonValue ACPClient::buildInitParams() {
    const auto keepAlive=shared_from_this();
    SettingsJsonObject info; info[literal("name")] = literal("nijigenerate"); info[literal("version")] = literal("0.0.0");
    SettingsJsonObject fs; fs[literal("readTextFile")] = false; fs[literal("writeTextFile")] = false;
    SettingsJsonObject capabilities; capabilities[literal("fs")] = JsonValue(fs); capabilities[literal("terminal")] = false;
    SettingsJsonObject result; result[literal("protocolVersion")] = JsonValue(protocol::ACP_PROTOCOL_VERSION);
    result[literal("clientCapabilities")] = JsonValue(capabilities); result[literal("clientInfo")] = JsonValue(info);
    return JsonValue(result);
}
JsonValue ACPClient::currentMcpServers() {
    const auto keepAlive=shared_from_this();
#ifdef NIJIGENERATE_HAVE_MCP
    using core::settings::incSettingsGet;
    const auto enabled = incSettingsGet<bool>("MCP.Enabled", false);
    auto host = incSettingsGet<core::settings::SettingsString>("MCP.Host", "127.0.0.1");
    const auto port = incSettingsGet<std::int32_t>("MCP.Port", 8088);
    if (!(enabled || !host.empty())) return parse("[]");
    if (host.empty()) host = literal("127.0.0.1");
    const auto portText = allocateClientStorage([&] { return std::to_string(port); });
    const auto url = concatenateClientText({"http://", host.view(), ":", portText, "/mcp"});
    const auto name = incSettingsGet<core::settings::SettingsString>("MCP.Name", "nijigenerate");
    const auto raw = incSettingsGet<core::settings::SettingsString>("MCP.Headers", "[]");
    JsonValue headers;
    try { headers = parse(raw); }
    catch (const std::bad_alloc&) { throw; }
    catch (const compat::ThrowableException&) { headers = parse("[]"); }
    if (!headers.is_array()) headers = parse("[]");
    const auto escapedName = escapeJsonString(name);
    const auto escapedUrl = escapeJsonString(url);
    const auto headerText = json(headers);
    return parse(concatenateClientText({"[{\"type\":\"http\",\"name\":\"", escapedName.view(),
        "\",\"url\":\"", escapedUrl.view(), "\",\"headers\":", headerText, "}]"}));
#else
    return parse("[]");
#endif
}
void ACPClient::ping() {
    const auto keepAlive=shared_from_this();
    const auto params=parse("{}");
    const auto id=JsonValue(std::uint64_t(nextId_++));
    send(Request(protocol::ACP_METHOD_PING,params,id).toJSON());
    waitResponse();
}
void ACPClient::close() {
    auto retained = shared_from_this();
    readerRunning_ = false; stderrRunning_ = false;
    if (ownsProcess_ && pipes_.input.isOpen()) pipes_.input.close();
    if (ownsProcess_ && pipes_.output.isOpen()) pipes_.output.close();
    if (ownsProcess_ && pipes_.error.isOpen()) pipes_.error.close();
    if (inboundCondition_) inboundCondition_->notifyAll();
    if (reader_) {
        reader_->join();
        reader_.reset();
    }
    if (stderrThread_) {
        stderrThread_->join();
        stderrThread_.reset();
    }
}
void ACPClient::send(JsonValue value) {
    const auto keepAlive=shared_from_this();
    const auto payload = json(value);
    try {
        if (sendMode_.view() == "content-length") {
            const auto length = allocateClientStorage([&] { return std::to_string(payload.size()); });
            pipes_.input.write(concatenateClientBytes({"Content-Length: ", length, "\r\n\r\n", payload}));
        } else pipes_.input.write(concatenateClientBytes({payload, "\n"}));
        pipes_.input.flush();
        if (debugEcho_) log(concatenateClientText({"[stdin] ", payload}));
    } catch (const std::bad_alloc&) { throw; }
    catch (const compat::ThrowableException& error) {
        const auto stderrText = drainStderr();
        const auto errorText = error.msg;
        auto failure = concatenateClientText({dynamic_cast<const providers::mcp::stdio::ErrnoException*>(&error)
            ? "send failed (errno): " : "send failed: ", errorText.view()});
        if (!stderrText.empty()) failure = concatenateClientText({failure.view(), " | stderr: ", stderrText.view()});
        const auto snapshot = stateSnapshot();
        failure = concatenateClientText({failure.view(), " | state: ", snapshot.view()});
        const auto sourceLine = dynamic_cast<const providers::mcp::stdio::StdioException*>(&error) ? 568 :
            dynamic_cast<const providers::mcp::stdio::ErrnoException*>(&error) ? 575 : 582;
        log(failure); throw sourceException(failure, sourceLine);
    }
}
String ACPClient::stateSnapshot() {
    const auto keepAlive=shared_from_this();
    const auto flag = [](bool value) { return value ? "true" : "false"; };
    const auto input = pipes_.input.isOpen(), output = pipes_.output.isOpen();
    const auto outputEof = pipes_.output.eof(), error = pipes_.error.isOpen(), errorEof = pipes_.error.eof();
    const auto pidText = pipes_.pid.toString();
    return concatenateClientText({"pid=", pidText, " stdin(open=", flag(input), ") stdout(open=",
        flag(output), ",eof=", flag(outputEof), ") stderr(open=", flag(error), ",eof=", flag(errorEof), ")"});
}
int ACPClient::stdoutFd() {
    const auto keepAlive=shared_from_this();
    return detail::ngClientStdoutFd(pipes_.output);
}
ACPClient::PollResult ACPClient::tryPollOnce() { return popInbound(); }
ACPClient::PollResult ACPClient::popInbound() {
    const auto keepAlive=shared_from_this();
    PollResult result;
    compat::ngRequireObject(inboundMutex_)->lock();
    if (!inboundQueue_.empty()) { result = inboundQueue_[0]; inboundQueue_ = inboundQueue_.slice(1, inboundQueue_.size()); }
    compat::ngRequireObject(inboundMutex_)->unlock();
    return result;
}
void ACPClient::readerLoop() {
    const auto keepAlive=shared_from_this();
    while (readerRunning_) {
        try {
            String chunk;
            if (!readStdoutChunk(chunk)) {
                if (!readerRunning_) break;
                std::this_thread::sleep_for(10ms); continue;
            }
            if (debugEcho_ && !chunk.empty()) {
                const auto encoded = base64(chunk.view());
                log(concatenateClientText({"[stdout-raw] ", encoded}));
            }
            if (!chunk.empty()) appendChunkBytes(bytes(chunk));
        } catch (const std::bad_alloc&) { throw; }
        catch (const compat::ThrowableException& error) {
            const auto errorText = error.msg;
            log(concatenateClientText({"reader exit: ", errorText.view()})); break;
        }
    }
}
void ACPClient::appendChunkBytes(compat::ConstSlice<std::uint8_t> value) {
    const auto keepAlive=shared_from_this();
    if (!value.empty()) {
        // Commit the whole source array append after its real byte allocation.
        // A failed allocation must not leave a partially appended chunk.
        const auto previous = readBuffer_;
        const auto updated = allocateClientStorage([&] {
            std::vector<std::uint8_t> combined;
            combined.reserve(previous.size()+value.size());
            if (!previous.empty()) combined.insert(combined.end(),previous.begin(),previous.end());
            combined.insert(combined.end(),value.begin(),value.end());
            return compat::Slice<std::uint8_t>(std::move(combined));
        });
        readBuffer_=updated;
    }
    parseReadBuffer();
}
void ACPClient::parseReadBuffer() {
    const auto keepAlive=shared_from_this();
    while (!readBuffer_.empty()) {
        const auto index = findByteIndex(readBuffer_, '\n');
        if (index < 0) break;
        auto line = readBuffer_.slice(0, static_cast<std::size_t>(index));
        if (!line.empty() && line[line.size() - 1] == '\r') line = line.slice(0, line.size() - 1);
        readBuffer_ = readBuffer_.slice(static_cast<std::size_t>(index) + 1, readBuffer_.size());
        line = truncateAtNull(line);
        if (line.empty()) continue;
        auto result = decodeJson(byteString(line));
        if (!result.error.empty()) logParseFailureBytes(line, result.error);
        enqueueResult(result);
    }
}
void ACPClient::enqueueResult(PollResult result) {
    const auto keepAlive=shared_from_this();
    if (!result.hasValue) return;
    compat::ngRequireObject(inboundMutex_)->lock();
    inboundQueue_.append(result);
    compat::ngRequireObject(inboundCondition_)->notify();
    compat::ngRequireObject(inboundMutex_)->unlock();
}
JsonValue ACPClient::waitResponse() {
    const auto keepAlive=shared_from_this();
    try {
        for (;;) {
            compat::SharedDelegate<bool()> cancel;
            { std::lock_guard lock(callbackMutex_); cancel = cancelCheck_; }
            if (cancel && cancel()) throw sourceException(compat::RetainedString::fromStaticLiteral("cancelled"), 685);
            auto value = popInbound();
            if (value.hasValue) {
                if (!value.error.empty()) throw sourceException(value.error.retainedString(), 689);
                return value.result;
            }
            compat::ngRequireObject(inboundMutex_)->lock();
            compat::ngRequireObject(inboundCondition_)->wait(
                std::chrono::duration_cast<nicxlive::core::d_runtime::SourceDuration>(50ms));
            compat::ngRequireObject(inboundMutex_)->unlock();
        }
    } catch (const std::bad_alloc&) { throw; }
    catch (const compat::ThrowableException& error) {
        const auto stderrText = drainStderr();
        const auto errorText = error.msg;
        auto failure = dynamic_cast<const providers::mcp::stdio::StdioException*>(&error)
            ? concatenateClientText({"Agent stream closed: ", errorText.view()})
            : errorText;
        if (!stderrText.empty()) failure = concatenateClientText({failure.view(), " | stderr: ", stderrText.view()});
        const auto snapshot = stateSnapshot();
        failure = concatenateClientText({failure.view(), " | state: ", snapshot.view()});
        const auto sourceLine = dynamic_cast<const providers::mcp::stdio::StdioException*>(&error) ? 702 : 709;
        log(failure); throw sourceException(failure, sourceLine);
    }
}
String ACPClient::drainStderr() {
    const auto keepAlive=shared_from_this();
    std::string result;
    for (;;) {
        String chunk;
        if (!readStderrChunk(chunk) || chunk.empty()) break;
        detail::appendClientText(result, chunk.view());
        if (debugEcho_) log(concatenateClientText({"[stderr] ", chunk.view()}));
    }
    return result.empty() ? String{} : String(retainClientText(std::move(result)));
}
void ACPClient::stderrLoop() {
    const auto keepAlive=shared_from_this();
    while (stderrRunning_) {
        try {
            String chunk;
            if (!readStderrChunk(chunk)) {
                if (!stderrRunning_) break;
                std::this_thread::sleep_for(10ms); continue;
            }
            if (!chunk.empty()) log(concatenateClientText({"[stderr] ", chunk.view()}));
        } catch (const std::bad_alloc&) { throw; }
        catch (const compat::ThrowableException&) { break; }
    }
}
bool ACPClient::readStdoutChunk(String& chunk) {
    const auto keepAlive=shared_from_this();
    chunk = {};
    return detail::ngReadClientChunk(pipes_.output, chunk);
}
bool ACPClient::readStderrChunk(String& chunk) {
    const auto keepAlive=shared_from_this();
    chunk = {};
    return detail::ngReadClientChunk(pipes_.error, chunk);
}
}
