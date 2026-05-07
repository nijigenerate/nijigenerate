module njc.http_transport;

import std.algorithm.searching : canFind;
import std.conv : to;
import std.exception : enforce;
import std.json : JSONValue, parseJSON;
import std.socket : InternetAddress, TcpSocket;
import std.string : indexOf, representation, splitLines, strip, toLower;

import njc.config : Endpoint;

final class HttpTransport {
    private Endpoint endpoint;

    this(Endpoint endpoint) {
        this.endpoint = endpoint;
    }

    JSONValue postJson(string body) {
        auto sock = new TcpSocket();
        scope(exit) sock.close();
        sock.connect(new InternetAddress(endpoint.host, endpoint.port));

        sendAll(sock, buildRequest(body));
        auto response = readAll(sock);
        return parseJsonResponse(response);
    }

    private string buildRequest(string body) {
        auto request =
            "POST " ~ endpoint.path ~ " HTTP/1.1\r\n" ~
            "Host: " ~ endpoint.host ~ ":" ~ endpoint.port.to!string ~ "\r\n" ~
            "Content-Type: application/json\r\n" ~
            "Accept: application/json\r\n" ~
            "Content-Length: " ~ body.length.to!string ~ "\r\n" ~
            "Connection: close\r\n";
        if (endpoint.bearer.length) {
            request ~= "Authorization: Bearer " ~ endpoint.bearer ~ "\r\n";
        }
        return request ~ "\r\n" ~ body;
    }
}

private void sendAll(TcpSocket sock, string request) {
    auto bytes = cast(const(ubyte)[]) request.representation;
    while (bytes.length) {
        auto sent = sock.send(bytes);
        enforce(sent > 0, "failed to send request");
        bytes = bytes[sent .. $];
    }
}

private ubyte[] readAll(TcpSocket sock) {
    ubyte[] data;
    ubyte[8192] buffer;
    for (;;) {
        auto n = sock.receive(buffer[]);
        if (n <= 0) break;
        data ~= buffer[0 .. cast(size_t) n];
    }
    enforce(data.length > 0, "empty response from MCP server");
    return data;
}

private JSONValue parseJsonResponse(const(ubyte)[] data) {
    auto raw = cast(string) data;
    auto sep = raw.indexOf("\r\n\r\n");
    enforce(sep >= 0, "invalid HTTP response");

    auto header = raw[0 .. cast(size_t) sep];
    auto responseBody = raw[cast(size_t) sep + 4 .. $];
    auto statusLine = header.splitLines[0];
    enforce(statusLine.canFind(" 200 ") || statusLine.canFind(" 204 "),
        "MCP server returned " ~ statusLine);

    if (header.toLower.canFind("transfer-encoding: chunked")) {
        responseBody = decodeChunked(responseBody);
    }
    if (responseBody.strip.length == 0) {
        return parseJSON(`{"jsonrpc":"2.0","result":null}`);
    }
    return parseJSON(responseBody);
}

private string decodeChunked(string body) {
    string outp;
    size_t pos;
    for (;;) {
        auto lineEnd = body.indexOf("\r\n", pos);
        enforce(lineEnd >= 0, "invalid chunked response");
        auto sizeText = body[pos .. cast(size_t) lineEnd].strip;
        auto size = parseHexSize(sizeText);
        pos = cast(size_t) lineEnd + 2;
        if (size == 0) break;
        enforce(pos + size + 2 <= body.length, "truncated chunked response");
        outp ~= body[pos .. pos + size];
        pos += size + 2;
    }
    return outp;
}

private size_t parseHexSize(string text) {
    size_t value;
    foreach (ch; text) {
        if (ch == ';') break;
        uint digit;
        if (ch >= '0' && ch <= '9') digit = ch - '0';
        else if (ch >= 'a' && ch <= 'f') digit = 10 + ch - 'a';
        else if (ch >= 'A' && ch <= 'F') digit = 10 + ch - 'A';
        else throw new Exception("invalid chunk size");
        value = value * 16 + digit;
    }
    return value;
}
