#include "protocol.hpp"
#include <string>
#include <utility>

namespace nijigenerate::api::acp::protocol {

ACPError::ACPError(std::int32_t errorCode, compat::RetainedString message,
                   compat::RetainedString errorDetails, compat::RetainedString sourceFile,
                   std::size_t sourceLine)
    : detail::ACPErrorFields(errorCode, std::move(errorDetails)),
      compat::ThrowableException(std::move(message), std::move(sourceFile), sourceLine) {}

ACPError::ACPError(ErrorCode errorCode, compat::RetainedString message,
                   compat::RetainedString errorDetails, compat::RetainedString sourceFile,
                   std::size_t sourceLine)
    : ACPError(static_cast<std::int32_t>(errorCode), std::move(message),
               std::move(errorDetails), std::move(sourceFile), sourceLine) {}

core::settings::SettingsJsonValue ACPError::toJSON() const {
    using core::settings::SettingsJsonObject;
    using core::settings::SettingsJsonValue;
    using core::settings::SettingsString;

    // The source AA literal evaluates its values before building the table.
    const auto errorCode = SettingsJsonValue(code);
    const auto errorMessage = SettingsJsonValue(SettingsString(msg));
    SettingsJsonObject error;
    error[SettingsString(compat::RetainedString::fromStaticLiteral("code"))] = errorCode;
    error[SettingsString(compat::RetainedString::fromStaticLiteral("message"))] =
        errorMessage;
    // A non-null empty descriptor still emits an empty JSON string.
    if (!details.isNull()) {
        error[SettingsString(compat::RetainedString::fromStaticLiteral("data"))] =
            SettingsJsonValue(SettingsString(details));
    }
    const auto jsonrpcValue = SettingsJsonValue(SettingsString(JSONRPC_VERSION));
    const auto errorValue = SettingsJsonValue(std::move(error));
    SettingsJsonObject response;
    response[SettingsString(compat::RetainedString::fromStaticLiteral("jsonrpc"))] =
        jsonrpcValue;
    response[SettingsString(compat::RetainedString::fromStaticLiteral("error"))] = errorValue;
    return SettingsJsonValue(std::move(response));
}

} // namespace nijigenerate::api::acp::protocol
