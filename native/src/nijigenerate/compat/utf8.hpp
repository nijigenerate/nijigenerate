#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace nijigenerate::compat {

struct Utf8Scalar { char32_t value; std::size_t byteOffset; };

// Input is borrowed only during the call; returned scalar records own their values.
inline std::vector<Utf8Scalar> ngUtf8Scalars(std::string_view text) {
    std::vector<Utf8Scalar> result;
    for (std::size_t offset = 0; offset < text.size();) {
        const auto start = offset;
        const auto first = static_cast<std::uint8_t>(text[offset++]);
        std::uint32_t scalar = first;
        std::uint32_t minimum = 0;
        std::size_t continuation = 0;
        if (first < 0x80) {}
        else if (first >= 0xc2 && first <= 0xdf) { scalar &= 0x1f; minimum = 0x80; continuation = 1; }
        else if (first >= 0xe0 && first <= 0xef) { scalar &= 0x0f; minimum = 0x800; continuation = 2; }
        else if (first >= 0xf0 && first <= 0xf4) { scalar &= 0x07; minimum = 0x10000; continuation = 3; }
        else throw std::invalid_argument("Invalid UTF-8 leading byte");
        if (continuation > text.size() - offset) throw std::invalid_argument("Truncated UTF-8 scalar");
        for (std::size_t i = 0; i < continuation; ++i) {
            const auto byte = static_cast<std::uint8_t>(text[offset++]);
            if ((byte & 0xc0) != 0x80) throw std::invalid_argument("Invalid UTF-8 continuation byte");
            scalar = (scalar << 6) | (byte & 0x3f);
        }
        if (scalar < minimum || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff))
            throw std::invalid_argument("Invalid UTF-8 scalar value");
        result.push_back({static_cast<char32_t>(scalar), start});
    }
    return result;
}

inline std::filesystem::path ngPathFromUtf8(std::string_view text) {
    (void)ngUtf8Scalars(text);
    if (text.find('\0') != std::string_view::npos) throw std::invalid_argument("NUL in OS path");
    std::u8string owned;
    owned.reserve(text.size());
    for (unsigned char byte : text) owned.push_back(static_cast<char8_t>(byte));
    return std::filesystem::path(owned);
}

inline std::string ngPathToUtf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    std::string result(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    (void)ngUtf8Scalars(result);
    if (result.find('\0') != std::string::npos) throw std::invalid_argument("NUL in OS path");
    return result;
}

} // namespace nijigenerate::compat
