module njc.config;

import std.conv : to;
import std.exception : enforce;
import std.process : environment;
import std.string : startsWith;

struct Endpoint {
    string host = "127.0.0.1";
    ushort port = 8088;
    string path = "/mcp";
    string bearer;
}

struct Options {
    Endpoint endpoint;
    string[] command;
    bool help;
    bool rawJson;
    bool summary;
}

Options parseOptions(string[] args) {
    Options options;
    auto envUrl = environment.get("NIJIGEN_MCP_URL", "");
    if (envUrl.length) applyUrl(options.endpoint, envUrl);
    options.endpoint.bearer = environment.get("NIJIGEN_MCP_BEARER", "");

    for (size_t i = 1; i < args.length; ++i) {
        auto arg = args[i];
        switch (arg) {
            case "-h":
            case "--help":
                options.help = true;
                break;
            case "--url":
                enforce(i + 1 < args.length, "--url requires a value");
                applyUrl(options.endpoint, args[++i]);
                break;
            case "--host":
                enforce(i + 1 < args.length, "--host requires a value");
                options.endpoint.host = args[++i];
                break;
            case "--port":
                enforce(i + 1 < args.length, "--port requires a value");
                auto port = args[++i].to!int;
                enforce(port >= 1 && port <= 65535, "port must be in 1..65535");
                options.endpoint.port = cast(ushort) port;
                break;
            case "--path":
                enforce(i + 1 < args.length, "--path requires a value");
                options.endpoint.path = args[++i];
                if (!options.endpoint.path.startsWith("/")) {
                    options.endpoint.path = "/" ~ options.endpoint.path;
                }
                break;
            case "--bearer":
                enforce(i + 1 < args.length, "--bearer requires a value");
                options.endpoint.bearer = args[++i];
                break;
            case "--json":
                options.rawJson = true;
                break;
            case "--summary":
                options.summary = true;
                break;
            default:
                options.command = args[i .. $].dup;
                i = args.length;
                break;
        }
    }

    if (!options.help) enforce(options.command.length > 0, "missing command; use --help");
    return options;
}

void applyUrl(ref Endpoint endpoint, string url) {
    enum prefix = "http://";
    enforce(url.startsWith(prefix), "--url supports only plain HTTP MCP endpoints");
    auto rest = url[prefix.length .. $];
    auto slash = rest.length;
    foreach (i, ch; rest) {
        if (ch == '/') {
            slash = i;
            break;
        }
    }
    auto authority = rest[0 .. slash];
    endpoint.path = slash < rest.length ? rest[slash .. $] : "/mcp";

    auto colon = authority.length;
    foreach (i, ch; authority) {
        if (ch == ':') {
            colon = i;
            break;
        }
    }
    endpoint.host = authority[0 .. colon].idup;
    if (colon < authority.length) {
        auto port = authority[colon + 1 .. $].to!int;
        enforce(port >= 1 && port <= 65535, "port must be in 1..65535");
        endpoint.port = cast(ushort) port;
    }
    enforce(endpoint.host.length > 0, "endpoint host is empty");
}

string usage() {
    return
`Usage:
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
`;
}
