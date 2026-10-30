#pragma once

// Required default-options string branch of current Phobos std.json.
// Copyright Jeremie Pelletier 2008-2009. BSL-1.0: phobos-rig-json.txt.
#include <string>
#include <string_view>

namespace nijigenerate::autorig::deterministic::contracts::detail {

inline void appendJsonString(std::string& output, std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    output += '"';
    // Default JSONOptions.none iterates bytes, without UTF decoding.
    for (const unsigned char byte : value) {
        switch (byte) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '/': output += "\\/"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (byte < 0x20 || byte == 0x7f) {
                output += "\\u00";
                output += hex[byte >> 4];
                output += hex[byte & 15];
            } else output += static_cast<char>(byte);
        }
    }
    output += '"';
}

} // namespace nijigenerate::autorig::deterministic::contracts::detail
