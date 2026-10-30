#pragma once
// URL/InetPath parsing from vibe.inet.url and vibe.core.path (MIT).
// Copyright 2012-2017 Sönke Ludwig. See port/licenses/mcp-http-transport.txt.
#include <nijigenerate/core/settings_string.hpp>
#include <nijigenerate/compat/phobos_unsigned_conversion.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include "http_status.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::api::mcp::http_transport::detail {
using UrlString = core::settings::SettingsString;

class PathValidationException : public compat::ThrowableException {
public:
    template<std::size_t N> explicit PathValidationException(const char (&message)[N])
        : ThrowableException(compat::RetainedString::fromStaticLiteral(message),
              compat::RetainedString::fromStaticLiteral("vibe/core/path.d"), 1037) {}
    std::string_view ngThrowableTypeName() const noexcept override {
        return "vibe.core.path.PathValidationException";
    }
};

struct RequestUrl {
    UrlString schema, host, username, password, path, query, anchor;
    std::uint16_t explicitPort = 0;
};

// The HTTP parser's used HTTPStatusException constructor receives an explicit
// counted message and source location from enforceHTTP.
class HTTPStatusException : public compat::ThrowableException {
    int status_;
public:
    UrlString debugMessage;
    HTTPStatusException(int status, compat::RetainedString message, std::size_t line,
        compat::RetainedString file = compat::RetainedString::fromStaticLiteral("vibe/http/internal/http1/server.d"))
        : ThrowableException(message.empty() ? sourceHTTPStatusText(status) : std::move(message),
              std::move(file), line), status_(status) {}
    int status() const noexcept { return status_; }
    std::string_view ngThrowableTypeName() const noexcept override { return "vibe.http.common.HTTPStatusException"; }
};

template<std::size_t N> [[noreturn]] void urlFailure(const char (&message)[N], std::size_t line) {
    throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral(message),
        compat::RetainedString::fromStaticLiteral("vibe/inet/url.d"), line);
}
[[noreturn]] inline void urlFailure(std::string message, std::size_t line) {
    throw compat::ThrowableException(compat::RetainedString(std::move(message)),
        compat::RetainedString::fromStaticLiteral("vibe/inet/url.d"), line);
}
[[noreturn]] inline void unexpectedPort(std::string_view suffix, std::string_view type,
    std::size_t line = 2547) {
    if (suffix.empty())
        throw compat::phobos::ConvException(compat::RetainedString(
            "Unexpected end of input when converting from type string to type " + std::string(type)),
            compat::RetainedString::fromStaticLiteral("std/conv.d"), line);
    const auto length = compat::phobos::ngUnexpectedScalarLength(suffix);
    const auto shown = suffix.front() == '\n' ? std::string("\\n") : std::string(suffix.substr(0, length));
    throw compat::phobos::ConvException(compat::RetainedString(
        "Unexpected '" + shown + "' when converting from type string to type " + std::string(type)),
        compat::RetainedString::fromStaticLiteral("std/conv.d"), line);
}
inline std::uint16_t urlPort(std::string_view text) {
    // Phobos parse!ushort first parses uint, narrows, then to!ushort checks
    // remaining input. Overflow must precede an invalid trailing character.
    if (text.empty() || text.front() < '0' || text.front() > '9') unexpectedPort(text, "uint");
    std::uint32_t value = 0;
    std::size_t consumed = 0;
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    for (; consumed < text.size(); ++consumed) {
        const auto ch = static_cast<unsigned char>(text[consumed]);
        if (ch < '0' || ch > '9') break;
        const auto digit = static_cast<unsigned>(ch - '0');
        if (value > maximum / 10 || (value == maximum / 10 && digit > maximum % 10))
            throw compat::phobos::ConvOverflowException(
                compat::RetainedString::fromStaticLiteral("Overflow in integral conversion"),
                compat::RetainedString::fromStaticLiteral("std/conv.d"), 2527);
        value = value * 10 + digit;
    }
    if (value > std::numeric_limits<std::uint16_t>::max()) throw compat::phobos::ConvOverflowException(
        compat::RetainedString::fromStaticLiteral("Overflow in integral conversion"),
        compat::RetainedString::fromStaticLiteral("std/conv.d"), 2449);
    if (consumed != text.size()) unexpectedPort(text.substr(consumed), "ushort", 2013);
    return static_cast<std::uint16_t>(value);
}
inline void validateRequestHost(std::string_view host, unsigned version) {
    const auto missingHost = [](std::size_t line) {
        throw HTTPStatusException(400,
            compat::RetainedString::fromStaticLiteral("Missing Host header."), line);
    };
    if (host.empty() && version > 10) missingHost(179);
    if (host.starts_with('[')) {
        const auto end = host.find(']');
        if (end == host.npos || end == 0)
            throw compat::ThrowableException(
                compat::RetainedString::fromStaticLiteral("Missing closing ']' for IPv6 address."),
                compat::RetainedString::fromStaticLiteral("vibe/http/internal/http1/server.d"), 182);
        host.remove_prefix(end + 1);
    } else if (!host.empty()) {
        auto colon = host.find(':');
        if (colon == host.npos) colon = host.size();
        if (colon == 0) missingHost(188);
        host.remove_prefix(colon);
    }
    if (host.starts_with(':')) (void)urlPort(host.substr(1));
}
inline std::uint64_t requestContentLength(std::string_view text) {
    try {
        std::size_t digits = 0;
        while (digits < text.size() && text[digits] >= '0' && text[digits] <= '9') ++digits;
        if (!digits) compat::phobos::ngThrowUnsignedConversion(text);
        const auto result = compat::phobos::ngUnsignedDecimal64(text.substr(0, digits));
        // parse!ulong leaves a suffix for the following enforceBadRequest, unlike
        // to!ulong which would throw ConvException for that remaining input.
        if (digits != text.size())
            throw HTTPStatusException(400,
                compat::RetainedString::fromStaticLiteral("Invalid content-length"), 222);
        return result;
    } catch (compat::phobos::ConvOverflowException& failure) {
        // Preserve the actual canonical exception object and its TraceInfo;
        // this caller supplies the selected parse!ulong source origin.
        failure.file = compat::RetainedString::fromStaticLiteral("std/conv.d");
        failure.line = 2527;
        throw;
    } catch (compat::phobos::ConvException& failure) {
        failure.file = compat::RetainedString::fromStaticLiteral("std/conv.d");
        failure.line = 2547;
        throw;
    }
}
inline std::uint64_t requestChunkLength(std::string_view text) {
    if (text.empty()) throw compat::phobos::ConvException(
        compat::RetainedString::fromStaticLiteral("s must not be empty in integral parse"),
        compat::RetainedString::fromStaticLiteral("std/conv.d"), 2865);
    std::uint64_t value = 0;
    std::size_t consumed = 0;
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    for (const auto ch : text) {
        unsigned digit;
        if (ch >= '0' && ch <= '9') digit = unsigned(ch - '0');
        else if (ch >= 'a' && ch <= 'f') digit = unsigned(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F') digit = unsigned(ch - 'A' + 10);
        else break;
        if (value > maximum / 16 || (value == maximum / 16 && digit > maximum % 16))
            throw compat::phobos::ConvOverflowException(
                compat::RetainedString::fromStaticLiteral("Overflow in integral conversion"),
                compat::RetainedString::fromStaticLiteral("std/conv.d"), 2905);
        value = value * 16 + digit;
        ++consumed;
    }
    if (!consumed) unexpectedPort(text, "ulong", 2915);
    // parse!ulong(line, 16) consumes a prefix; readChunk ignores the remainder.
    return value;
}
inline void validateInternetPath(std::string_view path) {
    const auto hex = [](unsigned char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
    };
    for (std::size_t i = 0; i < path.size(); ++i) {
        const auto ch = static_cast<unsigned char>(path[i]);
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) continue;
        switch (ch) {
        case '-': case '.': case '_': case '~':
        case '!': case '$': case '&': case '\'': case '(': case ')':
        case '*': case '+': case ',': case ';': case '=': case ':': case '@': case '/': break;
        case '%':
            if (path.size() - i < 3)
                throw PathValidationException("Unterminated percent encoding sequence in internet path.");
            for (unsigned j = 0; j < 2; ++j)
                if (!hex(static_cast<unsigned char>(path[++i])))
                    throw PathValidationException("Invalid percent encoding sequence in internet path.");
            break;
        default: throw PathValidationException("Invalid character in internet path.");
        }
    }
}
inline RequestUrl parseRequestUrl(UrlString input) {
    RequestUrl result;
    auto rest = input;
    if (rest.empty()) urlFailure("Empty URL.", 185);
    if (rest.view().front() != '/') {
        const auto colon = rest.view().find(':');
        if (colon == rest.view().npos || colon == 0) urlFailure("No schema in URL:" + std::string(rest.view()), 188);
        result.schema = rest.slice(0, colon);
        const auto first = static_cast<unsigned char>(result.schema.view().front());
        if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z')))
            urlFailure(std::string("Schema must start with an alphabetical char, found: ") + char(first), 190);
        rest = rest.slice(colon + 1, rest.size());
        const bool requiresHost = rest.view().starts_with("//");
        if (requiresHost) rest = rest.slice(2, rest.size());
        auto slash = rest.view().find('/');
        if (slash == rest.view().npos) slash = rest.size();
        const auto at = rest.view().substr(0, slash).find('@');
        std::size_t hostStart = 0;
        if (at != rest.view().npos) {
            hostStart = at + 1;
            const auto userColon = rest.view().substr(0, at).find(':');
            if (userColon != rest.view().npos) {
                result.username = rest.slice(0, userColon);
                result.password = rest.slice(userColon + 1, at);
            } else result.username = rest.slice(0, at);
            if (result.username.empty()) urlFailure("Empty user name in URL.", 213);
        }
        result.host = rest.slice(hostStart, slash);
        const auto findPort = [&](UrlString host) {
            const auto port = host.view().find(':');
            if (port != host.view().npos && port > 0) {
                if (port == host.size() - 1) urlFailure("Empty port in URL.", 222);
                result.explicitPort = urlPort(host.view().substr(port + 1));
            }
            return port;
        };
        if (result.host.view().starts_with('[')) {
            const auto end = result.host.view().find(']');
            if (end != result.host.view().npos && end > 0) {
                findPort(result.host.slice(end, result.host.size()));
                result.host = result.host.slice(1, end);
            }
        } else {
            const auto port = findPort(result.host);
            if (port != result.host.view().npos && port > 0) result.host = result.host.slice(0, port);
        }
        if (requiresHost && result.schema.view() != "file" && result.host.empty())
            urlFailure("Empty server name in URL.", 246);
        rest = rest.slice(slash, rest.size());
    }
    if (const auto anchor = rest.view().find('#'); anchor != rest.view().npos) {
        result.anchor = rest.slice(anchor + 1, rest.size());
        rest = rest.slice(0, anchor);
    }
    if (const auto query = rest.view().find('?'); query != rest.view().npos) {
        result.query = rest.slice(query + 1, rest.size());
        rest = rest.slice(0, query);
    }
    validateInternetPath(rest.view());
    result.path = rest;
    return result;
}
} // namespace nijigenerate::api::mcp::http_transport::detail
