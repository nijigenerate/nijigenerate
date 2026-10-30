#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace nijigenerate::commands::automesh::config::detail {

constexpr std::optional<std::string> _sanitizeId(std::string_view value) {
    if (value.empty()) return std::nullopt;
    std::string result;
    for (const unsigned char character : value) {
        const bool alphaNumeric = (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9');
        result += alphaNumeric ? static_cast<char>(character) : '_';
    }
    return result;
}

} // namespace nijigenerate::commands::automesh::config::detail
