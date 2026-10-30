#pragma once
// Used UTF-8 std.file.readText profile, std/file.d505-590.
// Copyright The D Language Foundation 2007-2011. BSL-1.0;
// see port/licenses/phobos-psd-file.txt and phobos-unicode.txt.
#include <compat/phobos_file_read.hpp>
#include "phobos_unicode_cursor.hpp"
#include "phobos_unicode_exception.hpp"
#include "retained_string.hpp"
#include <cstddef>
#include <utility>

namespace nijigenerate::compat::phobos {

inline RetainedString ngReadUtf8Text(RetainedString name) {
    const auto bytes = ::nicxlive::core::d_runtime::ngReadBinaryFile(std::move(name));
    const auto byte = [&](std::size_t index) { return bytes[index]; };
    // getBOM tests the longer UTF-32 signatures before the UTF-16 prefixes.
    if (bytes.size() >= 4 && ((byte(0) == 0 && byte(1) == 0 && byte(2) == 0xfe && byte(3) == 0xff) ||
            (byte(0) == 0xff && byte(1) == 0xfe && byte(2) == 0 && byte(3) == 0)))
        throw UTFException(RetainedString::fromStaticLiteral("UTF-8 requested. BOM is for UTF-32"),
            RetainedString::fromStaticLiteral("std/file.d"), 532);
    if (bytes.size() >= 2 && ((byte(0) == 0xfe && byte(1) == 0xff) ||
            (byte(0) == 0xff && byte(1) == 0xfe)))
        throw UTFException(RetainedString::fromStaticLiteral("UTF-8 requested. BOM is for UTF-16"),
            RetainedString::fromStaticLiteral("std/file.d"), 530);
    const auto* data = reinterpret_cast<const char*>(bytes.data());
    const auto text = data ? RetainedString::retainedView(bytes.retentionOwner(), data, bytes.size())
        : RetainedString{};
    // validate examines the entire file; its UTF-8 BOM is not stripped.
    detail::UnicodeCursor<RetainedString, char> cursor(text);
    while (!cursor.empty()) {
        (void)cursor.front();
        cursor.popFront();
    }
    return text;
}

} // namespace nijigenerate::compat::phobos
