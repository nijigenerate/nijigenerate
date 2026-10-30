#include "config.hpp"
#include "detail/port_conversion.hpp"
#include "detail/environment_windows.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <string>
#include <string_view>

namespace njc::config {
using nijigenerate::compat::RetainedString;
using nijigenerate::compat::ngArrayAt;
using nijigenerate::compat::ngArraySlice;
namespace {
template<std::size_t N> void require(bool condition, const char (&message)[N], std::size_t sourceLine) {
    if (!condition) {
        const auto copiedMessage = detail::ngConfigAllocation([&] {
            if constexpr (N > 1) return RetainedString(std::string_view(message, N - 1));
            else return RetainedString::fromStaticLiteral("Enforcement failed");
        });
        throw ConfigException(copiedMessage,
            RetainedString::fromStaticLiteral("source/njc/config.d"), sourceLine);
    }
}
std::uint16_t portNumber(Text text, std::size_t sourceLine) {
    const auto port = detail::ngPortInteger(text.view());
    require(port >= 1 && port <= 65535, "port must be in 1..65535", sourceLine);
    return static_cast<std::uint16_t>(port);
}
}

// Failure payload adapted from Phobos std.utf, Copyright The D Language
// Foundation 2000-2012. Boost Software License 1.0; see detail/phobos.LICENSE.
ConversionUtfException::ConversionUtfException(std::string_view suffix, bool truncated, std::source_location source)
    : UTFException(truncated ? RetainedString::fromStaticLiteral("Attempted to decode past the end of a string")
        : RetainedString::fromStaticLiteral("Invalid UTF-8 sequence"),
        detail::ngInvalidSequenceLength(suffix), {}, source) {
    len = idx;
    for (std::size_t i = 0; i < len; ++i) sequence[i] = static_cast<unsigned char>(suffix[i]);
}

ConversionUtfException::ConversionUtfException(std::uint16_t unit, const char* message, std::source_location source)
    : UTFException(detail::ngConfigAllocation([&] { return RetainedString(message); }), {}, source) {
    sequence[0] = unit;
    len = 1;
}

Options parseOptions(nijigenerate::compat::Slice<Text> args) {
    Options options;
    const auto url = detail::ngEnvironmentValue("NIJIGEN_MCP_URL");
    if (!url.empty()) applyUrl(options.endpoint, url);
    options.endpoint.bearer = detail::ngEnvironmentValue("NIJIGEN_MCP_BEARER");
    for (std::size_t i = 1; i < args.size(); ++i) {
        const auto argument = ngArrayAt(args, i).view();
        if (argument == "-h" || argument == "--help") options.help = true;
        else if (argument == "--url") {
            require(i + 1 < args.size(), "--url requires a value", 37);
            applyUrl(options.endpoint, ngArrayAt(args, ++i));
        } else if (argument == "--host") {
            require(i + 1 < args.size(), "--host requires a value", 41);
            options.endpoint.host = ngArrayAt(args, ++i);
        } else if (argument == "--port") {
            require(i + 1 < args.size(), "--port requires a value", 45);
            options.endpoint.port = portNumber(ngArrayAt(args, ++i), 47);
        } else if (argument == "--path") {
            require(i + 1 < args.size(), "--path requires a value", 51);
            options.endpoint.path = ngArrayAt(args, ++i);
            if (!options.endpoint.path.view().starts_with('/'))
                options.endpoint.path = detail::ngConfigAllocation([&] {
                    return RetainedString("/" + std::string(options.endpoint.path.view()));
                });
        } else if (argument == "--bearer") {
            require(i + 1 < args.size(), "--bearer requires a value", 58);
            options.endpoint.bearer = ngArrayAt(args, ++i);
        } else if (argument == "--json") options.rawJson = true;
        else if (argument == "--summary") options.summary = true;
        else { options.command = ngArraySlice(args, i, args.size()).duplicate(); break; }
    }
    if (!options.help) require(!options.command.empty(), "missing command; use --help", 74);
    return options;
}

void applyUrl(Endpoint& endpoint, Text url) {
    constexpr std::string_view prefix = "http://";
    require(url.view().starts_with(prefix), "--url supports only plain HTTP MCP endpoints", 80);
    const auto rest = url.slice(prefix.size(), url.size());
    auto slash = rest.view().find('/');
    if (slash == std::string_view::npos) slash = rest.size();
    const auto authority = rest.slice(0, slash);
    endpoint.path = slash < rest.size() ? rest.slice(slash, rest.size())
        : Text(RetainedString::fromStaticLiteral("/mcp"));
    auto colon = authority.view().find(':');
    if (colon == std::string_view::npos) colon = authority.size();
    endpoint.host = colon == 0 ? Text{} : detail::ngConfigAllocation([&] {
        return Text(RetainedString(authority.view().substr(0, colon)));
    });
    if (colon < authority.size()) endpoint.port = portNumber(authority.slice(colon + 1, authority.size()), 102);
    require(!endpoint.host.empty(), "endpoint host is empty", 105);
}

Text usage() {
    return RetainedString::fromStaticLiteral(R"(Usage:
  njc [connection options] tools list
  njc [connection options] tools call <name> [--json <object>]
  njc [connection options] resources list
  njc [connection options] resources templates
  njc [connection options] resources read <uri>
  njc [connection options] find <selector>
  njc [connection options] read <uuid-or-uri>
  njc [connection options] rpc <method> [--json <params>]

Connection options:
  --host <host>       MCP host, default 127.0.0.1
  --port <port>       MCP port, default 8088
  --path <path>       MCP HTTP path, default /mcp
  --url <url>         HTTP MCP endpoint; NIJIGEN_MCP_URL is also honored
  --bearer <token>    Authorization bearer token; NIJIGEN_MCP_BEARER is also honored
  --json              Alias for the default raw JSON-RPC output
  --summary           Print a human-readable summary for list/template commands
)");
}
} // namespace njc::config
