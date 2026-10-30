#pragma once
#include <array>
#include <string_view>
namespace nijigenerate::compat {
// The current std.uni.isWhite set, matched as canonical UTF-8 byte sequences.
// Invalid UTF-8 decodes to a non-whitespace replacement in std.string.strip.
inline std::string_view ngStripUnicode(std::string_view input) {
    static constexpr std::array<std::string_view, 25> white{
        "\t", "\n", "\v", "\f", "\r", " ", "\xc2\x85", "\xc2\xa0", "\xe1\x9a\x80",
        "\xe2\x80\x80", "\xe2\x80\x81", "\xe2\x80\x82", "\xe2\x80\x83", "\xe2\x80\x84",
        "\xe2\x80\x85", "\xe2\x80\x86", "\xe2\x80\x87", "\xe2\x80\x88", "\xe2\x80\x89",
        "\xe2\x80\x8a", "\xe2\x80\xa8", "\xe2\x80\xa9", "\xe2\x80\xaf", "\xe2\x81\x9f",
        "\xe3\x80\x80"};
    for (;;) {
        bool removed = false;
        for (const auto bytes : white) {
            if (input.starts_with(bytes)) { input.remove_prefix(bytes.size()); removed = true; break; }
        }
        if (!removed) break;
    }
    for (;;) {
        bool removed = false;
        for (const auto bytes : white) {
            if (input.ends_with(bytes)) { input.remove_suffix(bytes.size()); removed = true; break; }
        }
        if (!removed) return input;
    }
}

}

