#pragma once
#include "document.hpp"
#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <bit>
#include <string>
#include <unordered_map>

namespace nijigenerate::io::kra::detail {
inline std::int32_t wrap(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
inline std::int32_t add(std::int32_t a, std::int32_t b) { return wrap(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }
inline std::int32_t sub(std::int32_t a, std::int32_t b) { return wrap(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b)); }
inline std::int32_t mul(std::int32_t a, std::int32_t b) { return wrap(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
[[noreturn]] inline void failInteger(std::string_view suffix, std::size_t line) {
    const auto file = compat::RetainedString::fromStaticLiteral("std/conv.d");
    if (suffix.empty())
        throw compat::PhobosConvException{compat::RetainedString::fromStaticLiteral(
            "Unexpected end of input when converting from type string to type int"), file, line};
    const auto length = compat::phobos::ngUnexpectedScalarLength(suffix);
    const auto shown = suffix.front() == '\n' ? std::string("\\n") : std::string(suffix.substr(0, length));
    throw compat::PhobosConvException{compat::RetainedString(
        "Unexpected '" + shown + "' when converting from type string to type int"), file, line};
}
inline std::int32_t integer(std::string_view text) {
    bool negative = false;
    std::size_t i = 0;
    if (!text.empty() && (text[0] == '+' || text[0] == '-')) { negative = text[0] == '-'; ++i; }
    if (i == text.size()) failInteger({}, 2547);
    const auto firstDigit = i;
    std::uint32_t value = 0;
    const std::uint32_t limit = negative ? 2147483648u : 2147483647u;
    for (; i < text.size(); ++i) {
        const auto digit = static_cast<std::uint32_t>(static_cast<unsigned char>(text[i])) - '0';
        if (digit > 9) failInteger(text.substr(i), i == firstDigit ? 2547 : 2013);
        if (value > limit / 10 || (value == limit / 10 && digit > limit % 10))
            throw compat::PhobosConvOverflowException{
                compat::RetainedString::fromStaticLiteral("Overflow in integral conversion"),
                compat::RetainedString::fromStaticLiteral("std/conv.d"), 2527};
        value = value * 10 + digit;
    }
    return wrap(negative ? 0u - value : value);
}
inline compat::Slice<std::uint8_t> allocate(std::int32_t count) {
    if (count < 0) throw compat::ngStaticError<compat::OutOfMemoryError>(
        compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    compat::Slice<std::uint8_t> result; result.resize(static_cast<std::size_t>(count)); return result;
}
bool parseLayerData(compat::Slice<std::uint8_t> bytes, Layer& layer);
void cropLayer(compat::Slice<std::uint8_t> bytes, Layer& layer);
}
