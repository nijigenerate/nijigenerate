#pragma once
// Port of the independent Phobos std/base64.d allocating char-array decoder.
// Copyright Masahiro Nakagawa 2010-. BSL-1.0; see port/licenses/phobos-unicode.txt.
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nijigenerate_tests::regression::detail {

class FixtureBase64Error final : public nijigenerate::compat::ThrowableException {
public:
    FixtureBase64Error(std::string message, unsigned line)
        : ThrowableException(nijigenerate::compat::RetainedString(std::move(message)),
              nijigenerate::compat::RetainedString::fromStaticLiteral("std/base64.d"), line) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "std.base64.Base64Exception"; }
};

inline int fixtureBase64Character(unsigned char value) {
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '+') return 62;
    if (value == '/') return 63;
    if (value == '=') return -1;
    std::string message = "Invalid character: ";
    message.push_back(static_cast<char>(value));
    throw FixtureBase64Error(std::move(message), 1731);
}

// D's negative signed shifts here are defined; avoid native signed-left-shift UB.
inline std::uint32_t fixtureBase64Right(int value, unsigned count) noexcept {
    return value == -1 ? UINT32_MAX : static_cast<std::uint32_t>(value) >> count;
}

inline nijigenerate::compat::Slice<std::uint8_t> decodeRegressionFixtureBase64(std::string_view source) {
    using nijigenerate::compat::Slice;
    const auto capacity = (source.size() / 4) * 3;
    Slice<std::uint8_t> buffer{std::vector<std::uint8_t>(capacity, 0)};
    if (source.empty()) return {};
    if (source.size() % 4 != 0)
        throw FixtureBase64Error("Invalid length of encoded data", 1008);

    std::size_t output = 0;
    for (std::size_t input = 0; input < source.size(); input += 4) {
        const auto first = fixtureBase64Character(static_cast<unsigned char>(source[input]));
        const auto second = fixtureBase64Character(static_cast<unsigned char>(source[input + 1]));
        buffer[output++] = static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(first) << 2) | fixtureBase64Right(second, 4));
        const auto third = fixtureBase64Character(static_cast<unsigned char>(source[input + 2]));
        if (third == -1) break;
        buffer[output++] = static_cast<std::uint8_t>(
            ((static_cast<std::uint32_t>(second) << 4) | fixtureBase64Right(third, 2)) & 0xff);
        const auto fourth = fixtureBase64Character(static_cast<unsigned char>(source[input + 3]));
        if (fourth == -1) break;
        buffer[output++] = static_cast<std::uint8_t>(
            ((static_cast<std::uint32_t>(third) << 6) | static_cast<std::uint32_t>(fourth)) & 0xff);
    }
#ifndef NDEBUG
    auto expected = capacity;
    if (source[source.size() - 2] == '=') expected -= 2;
    else if (source[source.size() - 1] == '=') --expected;
    if (output != expected)
        throw nijigenerate::compat::AssertError{
            "The length of result is different from the expected length", "std/base64.d", 1000};
#endif
    return buffer.slice(0, output);
}
} // namespace nijigenerate_tests::regression::detail
