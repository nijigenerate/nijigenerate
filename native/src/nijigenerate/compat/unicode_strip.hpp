#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace nijigenerate::compat {
// std.string.stripLeft uses replacement decoding; only canonical whitespace
// prefixes are removed. stripRight separately decodes terminal 2/3-byte units.
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
    const auto isWhite = [](std::uint32_t value) {
        return (value >= 0x09 && value <= 0x0d) || value == 0x20 || value == 0x85 ||
            value == 0xa0 || value == 0x1680 || (value >= 0x2000 && value <= 0x200a) ||
            value == 0x2028 || value == 0x2029 || value == 0x202f || value == 0x205f || value == 0x3000;
    };
    while (!input.empty()) {
        const auto last = static_cast<unsigned char>(input.back());
        if (last <= 0x7f) {
            if (!isWhite(last)) break;
            input.remove_suffix(1);
            continue;
        }
        if (input.size() < 2 || (last & 0xc0) != 0x80) break;
        const std::uint32_t low = last & 0x3f;
        const auto second = static_cast<unsigned char>(input[input.size() - 2]);
        if ((second & 0xe0) == 0xc0) {
            if (!isWhite(low + (std::uint32_t(second & 0x1f) << 6))) break;
            input.remove_suffix(2);
            continue;
        }
        if (input.size() < 3 || (second & 0xc0) != 0x80) break;
        const auto third = static_cast<unsigned char>(input[input.size() - 3]);
        if ((third & 0xf0) != 0xe0 || !isWhite(low + (std::uint32_t(second & 0x3f) << 6) +
                (std::uint32_t(third & 0x0f) << 12))) break;
        input.remove_suffix(3);
    }
    return input;
}

}

