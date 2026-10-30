#pragma once
#include <nijigenerate/core/settings_value.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <cstddef>
#include <cstdint>
#include <source_location>
#include <string_view>
#include <nijigenerate/compat/throwable.hpp>

namespace nijigenerate::providers::mcp::protocol {
using JsonValue = core::settings::SettingsJsonValue;
using String = compat::RetainedStringSlice;
inline const String PROTOCOL_VERSION = compat::RetainedString::fromStaticLiteral("2024-11-05");
inline const String JSONRPC_VERSION = compat::RetainedString::fromStaticLiteral("2.0");
enum class ErrorCode : std::int32_t {
    parseError = -32700, invalidRequest = -32600, methodNotFound = -32601,
    invalidParams = -32602, internalError = -32603
};
class MCPError : public compat::ThrowableException {
public:
    std::int32_t code;
    String details;
    MCPError(std::int32_t code, String message, String details = {},
        String file = compat::RetainedString(std::source_location::current().file_name()),
        std::size_t line = std::source_location::current().line());
    JsonValue toJSON() const;
    std::string_view ngThrowableTypeName() const noexcept override { return "mcp.protocol.MCPError"; }
};
JsonValue* find(JsonValue& value, std::string_view key);
const JsonValue* find(const JsonValue& value, std::string_view key);
JsonValue& get(JsonValue& value, std::string_view key);
const JsonValue& get(const JsonValue& value, std::string_view key);
struct Request {
    String jsonrpc = JSONRPC_VERSION;
    String method;
    JsonValue params;
    JsonValue id;
    Request() = default;
    Request(String method, JsonValue params = {}, JsonValue id = {});
    static Request fromJSON(JsonValue json);
    JsonValue toJSON() const;
    bool isNotification() const { return id.is_null(); }
};
struct Response {
    String jsonrpc = JSONRPC_VERSION;
    JsonValue id;
    JsonValue result;
    JsonValue errorValue;
    static Response success(JsonValue id, JsonValue result);
    static Response makeError(JsonValue id, std::int32_t code, String message, String details = {});
    JsonValue toJSON() const;
};
}
