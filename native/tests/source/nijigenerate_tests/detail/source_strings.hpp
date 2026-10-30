#pragma once
// Used char-array concatenation and finite string-array join, BSL-1.0.
// Digital Mars 2000-2019; The D Language Foundation and Phobos contributors.
// Originals: core/internal/array/concatenation.d20-106, std/array.d2212-2287.
// Full BSL-1.0 text is retained in port/licenses/phobos-unicode.txt.
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <common/shared_array.hpp>
#include <bit>
#include <cstring>
#include <initializer_list>
#include <new>
#include <source_location>
#include <span>
#include <stdexcept>

namespace nijigenerate_tests::regression::detail {

// std.array.d2733-2755/2891-2940: the authored char-string "\\" -> "/" call.
inline nijigenerate::compat::RetainedString replaceSourcePathSeparators(
    nijigenerate::compat::RetainedString subject) {
    auto match = subject.view().find('\\');
    if (match == std::string_view::npos) return subject;
    nicxlive::core::common::SharedArray<char> output;
    const auto put = [&](std::string_view bytes) {
        output.appendRange(std::span<const char>(bytes.data(), bytes.size()));
    };
    put(subject.view().substr(0, match));
    put("/");
    auto remainder = subject.view().substr(match + 1);
    for (;;) {
        match = remainder.find('\\');
        if (match == std::string_view::npos) { put(remainder); break; }
        put(remainder.substr(0, match));
        put("/");
        remainder.remove_prefix(match + 1);
    }
    return nijigenerate::compat::RetainedString::retainedView(
        output.retentionOwner(), output.data(), output.size());
}

// std.string.d2616-2697, char[] and the authored default No.keepTerminator.
inline nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> splitSourceLines(
    nijigenerate::compat::RetainedString text) {
    nijigenerate::compat::Slice<nijigenerate::compat::RetainedString> result;
    const auto append = [&](std::size_t first, std::size_t last) {
        auto line = text.subslice(first, last);
        try { result.append(line); }
        catch (const std::bad_alloc&) {
            throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
                std::source_location::current());
        } catch (const std::length_error&) {
            throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
                std::source_location::current());
        }
    };
    std::size_t first = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto byte = static_cast<unsigned char>(text.data()[index]);
        std::size_t width = 0;
        switch (byte) {
        case '\v': case '\f': case '\n': width = 1; break;
        case '\r':
            width = index + 1 < text.size() && text.data()[index + 1] == '\n' ? 2 : 1;
            break;
        case 0xe2:
            if (index + 2 < text.size() && static_cast<unsigned char>(text.data()[index + 1]) == 0x80 &&
                (static_cast<unsigned char>(text.data()[index + 2]) == 0xa8 ||
                 static_cast<unsigned char>(text.data()[index + 2]) == 0xa9)) width = 3;
            break;
        case 0xc2:
            if (index + 1 < text.size() && static_cast<unsigned char>(text.data()[index + 1]) == 0x85) width = 2;
            break;
        default: break;
        }
        if (width) {
            append(first, index);
            first = index + width;
            index += width - 1;
        }
    }
    if (first != text.size()) append(first, text.size());
    return result;
}

// std.algorithm.searching.d802-850 counts decoded characters, not byte offsets.
inline std::ptrdiff_t countUntilSourceString(nijigenerate::compat::RetainedStringSlice haystack,
    nijigenerate::compat::RetainedStringSlice needle) {
    using Cursor = nijigenerate::compat::phobos::detail::UnicodeCursor<
        nijigenerate::compat::RetainedStringSlice, char>;
    if (needle.empty()) return 0;
    std::ptrdiff_t count = 0;
    while (!haystack.empty()) {
        const auto first = Cursor(haystack).front();
        const auto needleFirst = Cursor(needle).front();
        const auto width = first < 0x80 ? 1u : first < 0x800 ? 2u : first < 0x10000 ? 3u : 4u;
        haystack = haystack.slice(width, haystack.size());
        if (first == needleFirst) {
            const auto needleWidth = needleFirst < 0x80 ? 1u : needleFirst < 0x800 ? 2u :
                needleFirst < 0x10000 ? 3u : 4u;
            const auto tail = needle.slice(needleWidth, needle.size());
            // startsWith's same-encoding array specialization compares code units.
            if (haystack.view().starts_with(tail.view())) return count;
        }
        ++count;
    }
    return -1;
}

inline nicxlive::core::common::SharedArray<char> allocateSourceStringBytes(std::size_t length) {
    try {
        return nicxlive::core::common::SharedArray<char>(length,
            std::bit_cast<char>(std::uint8_t{0xff}));
    } catch (const std::bad_alloc&) {
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
            std::source_location::current());
    } catch (const std::length_error&) {
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
            std::source_location::current());
    }
}

inline nijigenerate::compat::RetainedString concatenateSourceStrings(
    std::initializer_list<nijigenerate::compat::RetainedString> froms) {
    std::size_t totalLength = 0;
    for (const auto& from : froms) totalLength += from.size();
    if (!totalLength) return {};
    auto result = allocateSourceStringBytes(totalLength);
    auto* output = result.data();
    for (const auto& from : froms) {
        if (from.size()) {
            std::memcpy(output, from.data(), from.size());
            output += from.size();
        }
    }
    return nijigenerate::compat::RetainedString::retainedView(
        result.retentionOwner(), result.data(), result.size());
}

inline nijigenerate::compat::RetainedString joinSourceStrings(
    std::span<const nijigenerate::compat::RetainedString> strings,
    nijigenerate::compat::RetainedString separator) {
    if (strings.empty()) return {};
    std::size_t length = 0;
    std::size_t rangeLength = 0;
    for (const auto& string : strings) { length += string.size(); ++rangeLength; }
    if (!rangeLength) return {};
    length += (rangeLength - 1) * separator.size();
    auto result = allocateSourceStringBytes(length);
    std::size_t written = 0;
    for (const auto byte : strings.front().view()) result[written++] = byte;
    for (std::size_t index = 1; index < strings.size(); ++index) {
        for (const auto byte : separator.view()) result[written++] = byte;
        for (const auto byte : strings[index].view()) result[written++] = byte;
    }
#ifndef NDEBUG
    if (written != result.size())
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::AssertError>(
            nijigenerate::compat::RetainedString::fromStaticLiteral("Assertion failure"),
            nijigenerate::compat::RetainedString::fromStaticLiteral("std/array.d"), 2267);
#endif
    if (!length) return {};
    return nijigenerate::compat::RetainedString::retainedView(
        result.retentionOwner(), result.data(), result.size());
}

} // namespace nijigenerate_tests::regression::detail
