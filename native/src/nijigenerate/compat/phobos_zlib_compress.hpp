#pragma once
// std.zlib.compress, Copyright The D Language Foundation 2000-2011.
// Boost Software License 1.0; see native/port/licenses/phobos-kra-zip.txt.
#include "const_slice.hpp"
#include "assert_error.hpp"
#include "out_of_memory_error.hpp"
#include <nijigenerate/io/kra/zip.hpp>
#include <compat/source_allocator.hpp>
#include <zlib.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::compat::phobos {

// The existing ubyte[] call family of std.zlib.compress(const(void)[], int).
// The input descriptor retains its real backing throughout the native call.
inline Slice<std::uint8_t> ngZlibCompress(ConstSlice<std::uint8_t> source, int level) {
#ifndef NDEBUG
    if (!(-1 <= level && level <= 9)) throw ngStaticError<AssertError>(
        RetainedString::fromStaticLiteral("Compression level needs to be within [-1, 9]."),
        RetainedString::fromStaticLiteral("std/zlib.d"), 199);
#endif
    auto destinationLength = source.size() + ((source.size() + 1023) / 1024) + 12;
    std::shared_ptr<std::uint8_t[]> output;
    try {
        output = std::allocate_shared_for_overwrite<std::uint8_t[]>(
            nicxlive::core::d_runtime::SourceAllocator<std::uint8_t>{}, destinationLength);
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
    // The selected D binding declares size_t*, while the actual C ABI writes
    // sizeof(uLongf) bytes. Preserve that write, including the untouched bytes
    // on Windows LLP64, without accessing a size_t through an aliased C pointer.
    static_assert(sizeof(uLongf) <= sizeof(destinationLength));
    uLongf nativeLength;
    std::memcpy(&nativeLength, &destinationLength, sizeof(nativeLength));
    const auto status = ::compress2(output.get(), &nativeLength, source.data(),
        static_cast<uLong>(source.size()), level);
    std::memcpy(&destinationLength, &nativeLength, sizeof(nativeLength));
    if (status) {
        output.reset();
        throw io::kra::ZlibException(status);
    }
    auto* data = output.get();
    return Slice<std::uint8_t>::retainedView(output, data, destinationLength);
}

inline Slice<std::uint8_t> ngZlibCompress(ConstSlice<std::uint8_t> source) {
    return ngZlibCompress(source, Z_DEFAULT_COMPRESSION);
}

} // namespace nijigenerate::compat::phobos
