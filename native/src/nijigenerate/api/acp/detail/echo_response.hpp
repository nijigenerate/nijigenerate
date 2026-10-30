#pragma once

#if defined(ACP_ECHO_TOOL)
#include <nijigenerate/api/acp/protocol_values.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace nijigenerate::api::acp::echo_agent::detail {
using core::settings::SettingsJsonValue;
using core::settings::SettingsJsonObject;
using core::settings::SettingsString;

template<std::size_t N> inline SettingsString literal(const char (&value)[N]) noexcept {
    return compat::RetainedString::fromStaticLiteral(value);
}

inline SettingsJsonValue makeResp(SettingsJsonValue id, SettingsJsonValue result) {
    SettingsJsonObject object;
    object[literal("jsonrpc")] = SettingsJsonValue(literal("2.0"));
    object[literal("id")] = id;
    object[literal("result")] = result;
    return SettingsJsonValue(object);
}

inline SettingsJsonValue handle(SettingsJsonValue request) {
    if (!request.is_object()) return SettingsJsonValue(nullptr);
    const auto method = request.sourceIndexValue("method").as_string();
    SettingsJsonValue id(nullptr);
    if (request.sourceObjectNoRef().if_contains("id")) id = request.sourceIndexValue("id");
    if (method.view() == "initialize") {
        SettingsJsonObject result;
        result[literal("protocolVersion")] = SettingsJsonValue(protocol::ACP_PROTOCOL_VERSION);
        SettingsJsonObject serverInfo;
        serverInfo[literal("name")] = SettingsJsonValue(literal("EchoAgent"));
        serverInfo[literal("version")] = SettingsJsonValue(literal("0.0.1"));
        result[literal("serverInfo")] = SettingsJsonValue(serverInfo);
        return makeResp(id, SettingsJsonValue(result));
    }
    SettingsJsonValue params(nullptr);
    if (request.sourceObjectNoRef().if_contains("params")) params = request.sourceIndexValue("params");
    SettingsJsonObject result;
    result[literal("echo")] = params;
    return makeResp(id, SettingsJsonValue(result));
}
} // namespace nijigenerate::api::acp::echo_agent::detail
#endif
