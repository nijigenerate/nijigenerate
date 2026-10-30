#pragma once
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <compat/system_error.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::io::depthmap_psd::detail {

// The checked LDC 1.41 POD slice-assignment path, after both slices were formed.
// These callers have a dynamic lower bound, not two compile-time slice lengths.
inline void copyPsdPixel(std::uint8_t* destination, const std::uint8_t* source) {
    const auto from = reinterpret_cast<std::uintptr_t>(source);
    const auto to = reinterpret_cast<std::uintptr_t>(destination);
    const auto distance = from > to ? from - to : to - from;
    if (distance < 4) {
        compat::RetainedString message;
        try {
            auto text = "Overlapping arrays in copy: " + std::to_string(4 - distance) +
                " byte(s) overlap of 4";
            message = compat::RetainedString(std::move(text));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<nicxlive::core::d_runtime::OutOfMemoryError>(
                std::source_location::current());
        }
        compat::ngAssertUnreachable(std::move(message),
            compat::RetainedString::fromStaticLiteral("core/internal/util/array.d"), 68);
    }
    std::memcpy(destination, source, 4);
}

} // namespace nijigenerate::io::depthmap_psd::detail
