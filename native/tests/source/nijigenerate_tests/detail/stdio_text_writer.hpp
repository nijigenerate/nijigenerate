#pragma once

// Original string/char/size_t writeln instantiations, through the one real File.
// Phobos std.stdio and unsignedToTempString: The D Language Foundation,
// Sean Kelly 2005-2009 and Walter Bright, BSL-1.0.
// Full license: port/licenses/phobos-unicode.txt.
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace nijigenerate_tests::regression::detail {
using nijigenerate::providers::mcp::stdio::SharedFile;
using nijigenerate::providers::mcp::stdio::FileTextWriter;
using nijigenerate::providers::mcp::stdio::standardOutput;
using nijigenerate::providers::mcp::stdio::standardError;

inline void writeLine(SharedFile& file, std::initializer_list<std::string_view> pieces) {
    file.withTextWriter([&](FileTextWriter& writer) {
        for (const auto piece : pieces) writer.put(piece);
        writer.put(static_cast<unsigned char>('\n'));
    });
}

inline void putUnsigned(FileTextWriter& writer, std::size_t value) {
    static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
    std::array<char, 64> digits;
    auto first = digits.size();
    auto number = static_cast<std::uint64_t>(value);
    do {
        std::uint32_t digit;
        if (number < 10) {
            digit = static_cast<std::uint32_t>(number);
            number = 0;
        } else {
            digit = static_cast<std::uint32_t>(number % 10);
            number /= 10;
        }
        digits[--first] = static_cast<char>('0' + digit);
    } while (number);
    writer.put(std::string_view(digits.data() + first, digits.size() - first));
}

inline void writeUnsignedLine(SharedFile& file, std::string_view prefix, std::size_t value,
    std::string_view suffix = {}) {
    file.withTextWriter([&](FileTextWriter& writer) {
        writer.put(prefix);
        putUnsigned(writer, value);
        if (!suffix.empty()) writer.put(suffix);
        writer.put(static_cast<unsigned char>('\n'));
    });
}
} // namespace nijigenerate_tests::regression::detail
