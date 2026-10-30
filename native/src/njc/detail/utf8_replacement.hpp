#pragma once

// Required replacement decoder from installed LDC 1.41 Phobos std.utf.
// Copyright The D Language Foundation 2000-2012. BSL-1.0: phobos.LICENSE.
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace njc::detail {

// Caller supplies a nonempty suffix. Invalid continuations are consumed with
// their starter, matching byUTF!dchar's default UseReplacementDchar.yes branch.
inline char32_t ngNextReplacementScalar(std::string_view value, std::size_t& position) {
    auto first = static_cast<unsigned char>(value[position++]);
    if (first < 0x80) return first;
    if ((first & 0xc0) != 0xc0) return 0xfffd;
    std::uint32_t scalar = first;
    first = static_cast<unsigned char>(first << 1);
    constexpr std::uint32_t masks[] = {0x7f, 0x7ff, 0xffff, 0x1fffff};
    for (std::size_t i = 1; i <= 3; ++i) {
        if (position == value.size()) return 0xfffd;
        const auto next = static_cast<unsigned char>(value[position++]);
        if ((next & 0xc0) != 0x80) return 0xfffd;
        scalar = (scalar << 6) | (next & 0x3f);
        first = static_cast<unsigned char>(first << 1);
        if (!(first & 0x80)) {
            scalar &= masks[i];
            if (!(scalar & ~masks[i - 1]) || (scalar >= 0xd800 && scalar <= 0xdfff) || scalar > 0x10ffff)
                return 0xfffd;
            return static_cast<char32_t>(scalar);
        }
    }
    return 0xfffd;
}

} // namespace njc::detail
