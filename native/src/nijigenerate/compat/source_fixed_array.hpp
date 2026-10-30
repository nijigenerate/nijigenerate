#pragma once

#include <compat/object_metadata.hpp>
#include <array>
#include <charconv>
#include <cstddef>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {
template<class T> struct SourceFixedArray : std::false_type {};
template<class T, std::size_t N> struct SourceFixedArray<std::array<T, N>> : std::true_type {
    using Element = T;
    inline static constexpr std::size_t length = N;
};
}

namespace nicxlive::core::d_runtime {
template<class T, std::size_t N> struct SourceTypeSpellings<std::array<T, N>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        const auto qualified = ngSourceTypeSpelling<T>(SourceTypeSpelling::QualifiedElement);
        const auto unqualified = ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedElement);
        char count[std::numeric_limits<std::size_t>::digits10 + 2];
        const auto end = std::to_chars(count, count + sizeof(count), N).ptr;
        try {
            const auto suffix = std::string("[") + std::string(count, end) + "]";
            auto q = std::string(qualified.view()) + suffix;
            auto u = std::string(unqualified.view()) + suffix;
            return {RetainedString("(" + q + ")"), RetainedString("(" + u + ")"),
                RetainedString(std::move(q)), RetainedString(std::move(u))};
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        }
    }
};
}
