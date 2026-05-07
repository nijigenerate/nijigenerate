module njc.jsonrpc;

import std.conv : to;

string escapeJson(string value) {
    string outp;
    foreach (dchar ch; value) {
        switch (ch) {
            case '"': outp ~= `\"`; break;
            case '\\': outp ~= `\\`; break;
            case '\b': outp ~= `\b`; break;
            case '\f': outp ~= `\f`; break;
            case '\n': outp ~= `\n`; break;
            case '\r': outp ~= `\r`; break;
            case '\t': outp ~= `\t`; break;
            default:
                if (ch < 0x20) {
                    import std.format : format;
                    outp ~= format(`\u%04x`, cast(uint) ch);
                } else {
                    outp ~= ch;
                }
                break;
        }
    }
    return outp;
}

string rpcRequest(long id, string method, string paramsJson) {
    return `{"jsonrpc":"2.0","id":` ~ id.to!string ~ `,"method":"` ~
        escapeJson(method) ~ `","params":` ~ paramsJson ~ `}`;
}
