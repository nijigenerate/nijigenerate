#pragma once
// Selected whole vibe.http.status, vibe-http 1.3.1.
// Copyright 2012 Sonke Ludwig. Author: Jan Kruger.
// MIT license: port/licenses/mcp-http-transport.txt.
#include <nijigenerate/compat/retained_string.hpp>
#include <cstdint>

namespace nijigenerate::api::mcp::http_transport::detail {

enum class SourceHTTPStatus : std::int32_t {
    continue_ = 100,
    switchingProtocols = 101,
    ok = 200,
    created = 201,
    accepted = 202,
    nonAuthoritativeInformation = 203,
    noContent = 204,
    resetContent = 205,
    partialContent = 206,
    multipleChoices = 300,
    movedPermanently = 301,
    found = 302,
    seeOther = 303,
    notModified = 304,
    useProxy = 305,
    temporaryRedirect = 307,
    badRequest = 400,
    unauthorized = 401,
    paymentRequired = 402,
    forbidden = 403,
    notFound = 404,
    methodNotAllowed = 405,
    notAcceptable = 406,
    proxyAuthenticationRequired = 407,
    requestTimeout = 408,
    conflict = 409,
    gone = 410,
    lengthRequired = 411,
    preconditionFailed = 412,
    requestEntityTooLarge = 413,
    requestURITooLarge = 414,
    unsupportedMediaType = 415,
    rangeNotSatisfiable = 416,
    expectationFailed = 417,
    tooManyRequests = 429,
    unavailableForLegalReasons = 451,
    internalServerError = 500,
    notImplemented = 501,
    badGateway = 502,
    serviceUnavailable = 503,
    gatewayTimeout = 504,
    httpVersionNotSupported = 505,
    processing = 102,
    multiStatus = 207,
    unprocessableEntity = 422,
    locked = 423,
    failedDependency = 424,
    insufficientStorage = 507,
};

inline compat::RetainedString sourceHTTPStatusText(std::int32_t code) noexcept {
    switch (static_cast<SourceHTTPStatus>(code)) {
    case SourceHTTPStatus::continue_: return compat::RetainedString::fromStaticLiteral("Continue");
    case SourceHTTPStatus::switchingProtocols: return compat::RetainedString::fromStaticLiteral("Switching Protocols");
    case SourceHTTPStatus::ok: return compat::RetainedString::fromStaticLiteral("OK");
    case SourceHTTPStatus::created: return compat::RetainedString::fromStaticLiteral("Created");
    case SourceHTTPStatus::accepted: return compat::RetainedString::fromStaticLiteral("Accepted");
    case SourceHTTPStatus::nonAuthoritativeInformation:
        return compat::RetainedString::fromStaticLiteral("Non-Authoritative Information");
    case SourceHTTPStatus::noContent: return compat::RetainedString::fromStaticLiteral("No Content");
    case SourceHTTPStatus::resetContent: return compat::RetainedString::fromStaticLiteral("Reset Content");
    case SourceHTTPStatus::partialContent: return compat::RetainedString::fromStaticLiteral("Partial Content");
    case SourceHTTPStatus::multipleChoices: return compat::RetainedString::fromStaticLiteral("Multiple Choices");
    case SourceHTTPStatus::movedPermanently: return compat::RetainedString::fromStaticLiteral("Moved Permanently");
    case SourceHTTPStatus::found: return compat::RetainedString::fromStaticLiteral("Found");
    case SourceHTTPStatus::seeOther: return compat::RetainedString::fromStaticLiteral("See Other");
    case SourceHTTPStatus::notModified: return compat::RetainedString::fromStaticLiteral("Not Modified");
    case SourceHTTPStatus::useProxy: return compat::RetainedString::fromStaticLiteral("Use Proxy");
    case SourceHTTPStatus::temporaryRedirect: return compat::RetainedString::fromStaticLiteral("Temporary Redirect");
    case SourceHTTPStatus::badRequest: return compat::RetainedString::fromStaticLiteral("Bad Request");
    case SourceHTTPStatus::unauthorized: return compat::RetainedString::fromStaticLiteral("Unauthorized");
    case SourceHTTPStatus::paymentRequired: return compat::RetainedString::fromStaticLiteral("Payment Required");
    case SourceHTTPStatus::forbidden: return compat::RetainedString::fromStaticLiteral("Forbidden");
    case SourceHTTPStatus::notFound: return compat::RetainedString::fromStaticLiteral("Not Found");
    case SourceHTTPStatus::methodNotAllowed: return compat::RetainedString::fromStaticLiteral("Method Not Allowed");
    case SourceHTTPStatus::notAcceptable: return compat::RetainedString::fromStaticLiteral("Not Acceptable");
    case SourceHTTPStatus::proxyAuthenticationRequired:
        return compat::RetainedString::fromStaticLiteral("Proxy Authentication Required");
    case SourceHTTPStatus::requestTimeout: return compat::RetainedString::fromStaticLiteral("Request Time-out");
    case SourceHTTPStatus::conflict: return compat::RetainedString::fromStaticLiteral("Conflict");
    case SourceHTTPStatus::gone: return compat::RetainedString::fromStaticLiteral("Gone");
    case SourceHTTPStatus::lengthRequired: return compat::RetainedString::fromStaticLiteral("Length Required");
    case SourceHTTPStatus::preconditionFailed: return compat::RetainedString::fromStaticLiteral("Precondition Failed");
    case SourceHTTPStatus::requestEntityTooLarge: return compat::RetainedString::fromStaticLiteral("Request Entity Too Large");
    case SourceHTTPStatus::requestURITooLarge: return compat::RetainedString::fromStaticLiteral("Request-URI Too Large");
    case SourceHTTPStatus::unsupportedMediaType: return compat::RetainedString::fromStaticLiteral("Unsupported Media Type");
    case SourceHTTPStatus::rangeNotSatisfiable:
        return compat::RetainedString::fromStaticLiteral("Requested range not satisfiable");
    case SourceHTTPStatus::expectationFailed: return compat::RetainedString::fromStaticLiteral("Expectation Failed");
    case SourceHTTPStatus::unavailableForLegalReasons:
        return compat::RetainedString::fromStaticLiteral("Unavailable For Legal Reasons");
    case SourceHTTPStatus::internalServerError: return compat::RetainedString::fromStaticLiteral("Internal Server Error");
    case SourceHTTPStatus::notImplemented: return compat::RetainedString::fromStaticLiteral("Not Implemented");
    case SourceHTTPStatus::badGateway: return compat::RetainedString::fromStaticLiteral("Bad Gateway");
    case SourceHTTPStatus::serviceUnavailable: return compat::RetainedString::fromStaticLiteral("Service Unavailable");
    case SourceHTTPStatus::gatewayTimeout: return compat::RetainedString::fromStaticLiteral("Gateway Time-out");
    case SourceHTTPStatus::httpVersionNotSupported:
        return compat::RetainedString::fromStaticLiteral("HTTP Version not supported");
    case SourceHTTPStatus::multiStatus: return compat::RetainedString::fromStaticLiteral("Multi-Status");
    case SourceHTTPStatus::unprocessableEntity: return compat::RetainedString::fromStaticLiteral("Unprocessable Entity");
    case SourceHTTPStatus::locked: return compat::RetainedString::fromStaticLiteral("Locked");
    case SourceHTTPStatus::failedDependency: return compat::RetainedString::fromStaticLiteral("Failed Dependency");
    case SourceHTTPStatus::insufficientStorage: return compat::RetainedString::fromStaticLiteral("Insufficient Storage");
    case SourceHTTPStatus::processing: return compat::RetainedString::fromStaticLiteral("Processing");
    default: break;
    }
    if (code >= 600) return compat::RetainedString::fromStaticLiteral("Unknown");
    if (code >= 500) return compat::RetainedString::fromStaticLiteral("Unknown server error");
    if (code >= 400) return compat::RetainedString::fromStaticLiteral("Unknown error");
    if (code >= 300) return compat::RetainedString::fromStaticLiteral("Unknown redirection");
    if (code >= 200) return compat::RetainedString::fromStaticLiteral("Unknown success");
    if (code >= 100) return compat::RetainedString::fromStaticLiteral("Unknown information");
    return compat::RetainedString::fromStaticLiteral("Unknown");
}

inline constexpr bool sourceJustifiesConnectionClose(std::int32_t status) noexcept {
    switch (static_cast<SourceHTTPStatus>(status)) {
    case SourceHTTPStatus::requestEntityTooLarge:
    case SourceHTTPStatus::requestURITooLarge:
    case SourceHTTPStatus::requestTimeout: return true;
    default: return false;
    }
}

inline constexpr bool sourceIsSuccessCode(SourceHTTPStatus status) noexcept {
    const auto code = static_cast<std::int32_t>(status);
    return code >= 200 && code < 300;
}

} // namespace nijigenerate::api::mcp::http_transport::detail
