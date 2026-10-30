// Port of MCP 1.0.1 protocol. Copyright 2025 Garret Noble; port/licenses/mcp-stdio.txt.
#include "mcp_protocol.hpp"
#include <string>
#include <utility>

namespace nijigenerate::providers::mcp::protocol {
using core::settings::SettingsJsonObject;
using core::settings::SettingsString;
namespace {
template<std::size_t N> SettingsString literal(const char (&value)[N]) noexcept {
    return compat::RetainedString::fromStaticLiteral(value);
}
[[noreturn]] void invalidRequest(String message, std::size_t line) {
    throw MCPError(static_cast<std::int32_t>(ErrorCode::invalidRequest), message, {},
        literal("mcp/protocol.d"), line);
}
}
MCPError::MCPError(std::int32_t code, String message, String details, String file, std::size_t line)
    : compat::ThrowableException(message.retainedString(), file.retainedString(), line),
      code(code), details(std::move(details)) {}
JsonValue MCPError::toJSON() const {
    SettingsJsonObject error;
    error[literal("code")] = JsonValue(code);
    error[literal("message")] = JsonValue(SettingsString(msg));
    if (!details.isNull()) error[literal("data")] = JsonValue(details.retainedString());
    SettingsJsonObject response;
    response[literal("jsonrpc")] = JsonValue(JSONRPC_VERSION.retainedString());
    response[literal("error")] = JsonValue(error);
    return JsonValue(response);
}
JsonValue* find(JsonValue& value, std::string_view key) { return value.as_object().if_contains(key); }
const JsonValue* find(const JsonValue& value, std::string_view key) {
    return value.as_object().if_contains(key);
}
JsonValue& get(JsonValue& value, std::string_view key) {
    if (auto* member = find(value, key)) return *member;
    throw core::settings::SettingsJsonError("Key not found: " + std::string(key));
}
const JsonValue& get(const JsonValue& value, std::string_view key) {
    if (const auto* member = find(value, key)) return *member;
    throw core::settings::SettingsJsonError("Key not found: " + std::string(key));
}
Request::Request(String method, JsonValue params, JsonValue id)
    : method(std::move(method)), params(std::move(params)), id(std::move(id)) {}
Request Request::fromJSON(JsonValue json) {
    Request request;
    if (!json.sourceObjectNoRef().if_contains("jsonrpc") ||
        json.sourceIndexValue("jsonrpc").as_string().view() != JSONRPC_VERSION.view())
        invalidRequest(literal("Invalid JSON-RPC version"), 124);
    if (!json.sourceObjectNoRef().if_contains("method") ||
        !json.sourceIndexValue("method").is_string())
        invalidRequest(literal("Missing or invalid method"), 130);
    request.method = json.sourceIndexValue("method").as_string();
    if (json.sourceObjectNoRef().if_contains("params")) {
        if (!json.sourceIndexValue("params").is_object() &&
            !json.sourceIndexValue("params").is_array())
            invalidRequest(literal("Parameters must be object or array"), 139);
        request.params = json.sourceIndexValue("params");
    }
    if (json.sourceObjectNoRef().if_contains("id")) {
        if (!json.sourceIndexValue("id").is_string() &&
            !json.sourceIndexValue("id").is_int64() &&
            !json.sourceIndexValue("id").is_null())
            invalidRequest(literal("Invalid request ID type"), 150);
        request.id = json.sourceIndexValue("id");
    }
    return request;
}
JsonValue Request::toJSON() const {
    SettingsJsonObject value;
    value[literal("jsonrpc")] = JsonValue(JSONRPC_VERSION.retainedString());
    value[literal("method")] = JsonValue(method.retainedString());
    if (!params.is_null()) value[literal("params")] = params;
    if (!id.is_null()) value[literal("id")] = id;
    return JsonValue(value);
}
Response Response::success(JsonValue id, JsonValue result) {
    Response response;
    response.id = std::move(id);
    response.result = std::move(result);
    return response;
}
Response Response::makeError(JsonValue id, std::int32_t code, String message, String details) {
    Response response;
    response.id = std::move(id);
    SettingsJsonObject error;
    error[literal("code")] = JsonValue(code);
    error[literal("message")] = JsonValue(message.retainedString());
    if (!details.isNull()) error[literal("data")] = JsonValue(details.retainedString());
    response.errorValue = JsonValue(error);
    return response;
}
JsonValue Response::toJSON() const {
    SettingsJsonObject value;
    value[literal("jsonrpc")] = JsonValue(JSONRPC_VERSION.retainedString());
    value[literal("id")] = id;
    if (!errorValue.is_null()) value[literal("error")] = errorValue;
    else value[literal("result")] = result;
    return JsonValue(value);
}
}
