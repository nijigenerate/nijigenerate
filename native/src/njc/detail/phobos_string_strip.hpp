#pragma once

// Required narrow-string std.string.strip and std.uni.isWhite production paths.
// Copyright The D Language Foundation 2007-. BSL-1.0: phobos.LICENSE.
#include "utf8_replacement.hpp"
#include <cstddef>
#include <string_view>
#include <utility>

namespace njc::detail {
inline bool ngStringWhite(char32_t scalar) {
    return (scalar >= 9 && scalar <= 13) || scalar == 0x20 || scalar == 0x85 || scalar == 0xa0 ||
        scalar == 0x1680 || (scalar >= 0x2000 && scalar <= 0x200a) || scalar == 0x2028 ||
        scalar == 0x2029 || scalar == 0x202f || scalar == 0x205f || scalar == 0x3000;
}

// Return the original byte subrange; consumers retain their original owner.
inline std::pair<std::size_t, std::size_t> ngStripStringBounds(std::string_view bytes) {
    std::size_t first = 0;
    while (first < bytes.size()) {
        auto next = first;
        if (!ngStringWhite(ngNextReplacementScalar(bytes, next))) break;
        first = next;
    }
    bytes = bytes.substr(first);
    std::size_t last = bytes.size();
    auto unit = [&](std::size_t i) { return static_cast<unsigned char>(bytes[i]); };
    // stripRight recognizes encoded whitespace without UTF validity checks.
    while (last) {
        const auto tail = unit(last - 1);
        if (tail <= 0x7f) {
            if (!ngStringWhite(tail)) break;
            --last;
            continue;
        }
        if (last < 2 || (tail & 0xc0) != 0x80) break;
        const auto middle = unit(last - 2);
        const auto low = static_cast<unsigned>(tail & 0x3f);
        if ((middle & 0xe0) == 0xc0) {
            if (!ngStringWhite(low + (static_cast<unsigned>(middle & 0x1f) << 6))) break;
            last -= 2;
            continue;
        }
        if (last < 3 || (middle & 0xc0) != 0x80) break;
        const auto head = unit(last - 3);
        if ((head & 0xf0) != 0xe0 ||
            !ngStringWhite(low + (static_cast<unsigned>(middle & 0x3f) << 6) +
                (static_cast<unsigned>(head & 0xf) << 12))) break;
        last -= 3;
    }
    return {first, first + last};
}
} // namespace njc::detail
