#pragma once
// Selected vibe.http.router literal specialization, Copyright 2012-2015 Sönke Ludwig.
// MIT permission/disclaimer: port/licenses/mcp-http-transport.txt.
#include "http_request_header.hpp"
#include <cstddef>
#include <string_view>

namespace nijigenerate::api::mcp::http_transport::detail {
enum class SourceLiteralRoute {
    mcp, events, protectedResourceMetadata, authorizationServerMetadata, authorize, token
};
namespace route_match_source {
struct Literal { SourceHTTPMethod method; std::string_view path; };
inline constexpr Literal definition(SourceLiteralRoute route) noexcept {
    switch (route) {
    case SourceLiteralRoute::mcp: return {SourceHTTPMethod::POST, "/mcp"};
    case SourceLiteralRoute::events: return {SourceHTTPMethod::GET, "/events"};
    case SourceLiteralRoute::protectedResourceMetadata:
        return {SourceHTTPMethod::GET, "/.well-known/oauth-protected-resource/mcp"};
    case SourceLiteralRoute::authorizationServerMetadata:
        return {SourceHTTPMethod::GET, "/auth/.well-known/oauth-authorization-server"};
    case SourceLiteralRoute::authorize: return {SourceHTTPMethod::GET, "/auth/authorize"};
    case SourceLiteralRoute::token: return {SourceHTTPMethod::POST, "/auth/token"};
    }
    return {SourceHTTPMethod::GET, {}};
}
inline constexpr int hexDigit(unsigned char byte) noexcept {
    if (byte >= '0' && byte <= '9') return byte - '0';
    if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
    return -1;
}
inline constexpr bool unreserved(unsigned byte) noexcept {
    return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')
        || (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.' || byte == '~';
}
inline constexpr unsigned char nextMatchChar(std::string_view text, std::size_t& offset) noexcept {
    const auto byte = static_cast<unsigned char>(text[offset]);
    if (byte == '%' && text.size() - offset > 2) {
        const auto high = hexDigit(static_cast<unsigned char>(text[offset + 1]));
        const auto low = hexDigit(static_cast<unsigned char>(text[offset + 2]));
        if (high >= 0 && low >= 0) {
            const auto decoded = static_cast<unsigned>(high * 16 + low);
            if (unreserved(decoded)) {
                offset += 3;
                return static_cast<unsigned char>(decoded);
            }
        }
    }
    ++offset;
    return byte;
}
} // namespace route_match_source

// Complete predicate for M025's six authored literals, not a general URLRouter.
// encodedPath is the actual retained requestPath.toString byte view, not a
// decoded/normalized URI. No input bytes escape this synchronous operation.
inline constexpr bool sourceHTTPRouteMatches(SourceLiteralRoute route, SourceHTTPMethod method,
    std::string_view encodedPath) noexcept {
    const auto literal = route_match_source::definition(route);
    if (literal.path.empty()) return false;
    // There are no registered HEAD routes in this closed registry, so the
    // source's failed HEAD pass always falls back to the four GET literals.
    if (method == SourceHTTPMethod::HEAD) method = SourceHTTPMethod::GET;
    if (method != literal.method) return false;
    std::size_t input = 0, matched = 0;
    while (input < encodedPath.size()) {
        if (matched == literal.path.size()) return false;
        if (route_match_source::nextMatchChar(encodedPath, input)
            != static_cast<unsigned char>(literal.path[matched])) return false;
        ++matched;
    }
    return matched == literal.path.size();
}
} // namespace nijigenerate::api::mcp::http_transport::detail
