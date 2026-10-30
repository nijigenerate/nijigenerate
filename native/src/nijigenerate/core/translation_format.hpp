#pragma once

#include <algorithm>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nijigenerate::core::translation {

// Measured D std.format subset for string arguments: %s, %% and one-based %N$s.
// Numeric formatting and width/precision modifiers require their own typed adapters.
inline std::string ngFormatText(std::string_view pattern, std::span<const std::string> arguments) {
    std::string result;
    std::size_t nextArgument = 0;
    for (std::size_t offset = 0; offset < pattern.size();) {
        if (pattern[offset] != '%') { result += pattern[offset++]; continue; }
        ++offset;
        if (offset == pattern.size()) throw std::invalid_argument("Incomplete format specifier");
        if (pattern[offset] == '%') { result += '%'; ++offset; continue; }
        auto argument = nextArgument;
        if (pattern[offset] >= '0' && pattern[offset] <= '9') {
            std::size_t position = 0;
            while (offset < pattern.size() && pattern[offset] >= '0' && pattern[offset] <= '9') {
                const auto digit = static_cast<std::size_t>(pattern[offset++] - '0');
                if (position > (std::numeric_limits<std::size_t>::max() - digit) / 10)
                    throw std::invalid_argument("Format argument position overflow");
                position = position * 10 + digit;
            }
            if (!position || offset == pattern.size() || pattern[offset++] != '$')
                throw std::invalid_argument("Unsupported format modifier");
            argument = position - 1;
        }
        if (offset == pattern.size() || pattern[offset++] != 's')
            throw std::invalid_argument("Unsupported string format specifier");
        if (argument >= arguments.size()) throw std::invalid_argument("Missing format argument");
        result += arguments[argument];
        nextArgument = std::max(nextArgument, argument + 1);
    }
    if (nextArgument != arguments.size()) throw std::invalid_argument("Unused format arguments");
    return result;
}

} // namespace nijigenerate::core::translation
