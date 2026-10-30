// Copyright 2021, Inochi2D Project. BSD-2-Clause: native/port/licenses/psd-d.txt.
#include "nijigenerate/io/psd/io.hpp"
#include "nijigenerate/compat/retained_string_slice.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <cstdio>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nijigenerate::io::psd::utils {

compat::Slice<std::uint8_t> read(RetainedFile& file, std::size_t length) {
    compat::Slice<std::uint8_t> bytes;
    if (length) {
        try {
            bytes = compat::Slice<std::uint8_t>{std::vector<std::uint8_t>(length)};
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    const auto received = file.rawRead({bytes.data(), bytes.size()});
    return bytes.slice(0, received.size());
}

compat::RetainedString readStr(RetainedFile& file, std::uint32_t length) {
    auto bytes = read(file, length);
    if (bytes.isNull()) return {};
    return compat::RetainedString::retainedView(bytes.retentionOwner(),
        reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

compat::RetainedString peekStr(RetainedFile& file, std::uint32_t length) {
    auto value = readStr(file, length);
    // D negates cast(int)length in 32 bits before widening to the seek offset.
    const auto displacement = std::bit_cast<std::int32_t>(std::uint32_t{0} - length);
    file.seek(displacement, SEEK_CUR);
    return value;
}

compat::RetainedString readPascalStr(RetainedFile& file, std::uint32_t readTo) {
    const std::uint32_t length = readValue<std::uint8_t>(file);
    const std::uint32_t extra = readTo > length ? readTo - length : 0;
    if (length == 0) {
        if (extra > 0) skip(file, extra);
        return compat::RetainedString::emptyLiteral();
    }
    auto value = readStr(file, length);
    if (extra > 0) skip(file, extra);
    return compat::RetainedStringSlice{value}.slice(0, length).retainedString();
}

compat::Slice<std::uint8_t> peek(RetainedFile& file, std::ptrdiff_t length) {
    auto result = read(file, static_cast<std::size_t>(length));
    const auto displacement = std::bit_cast<std::ptrdiff_t>(std::size_t{0} - static_cast<std::size_t>(length));
    file.seek(displacement, SEEK_CUR);
    return result;
}

void skip(RetainedFile& file, std::ptrdiff_t length) {
    file.seek(length, SEEK_CUR);
}
} // namespace nijigenerate::io::psd::utils
