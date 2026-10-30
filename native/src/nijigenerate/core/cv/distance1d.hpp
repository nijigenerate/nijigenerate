#pragma once

#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/retained_string.hpp"
#include <cstdint>
#include <limits>
#include <span>
#include <tuple>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::core::cv::distancetransform {
namespace detail {

// Only the original primitive array backing requests enter this boundary.
template<class Build> decltype(auto) allocateDistanceArray(Build&& build) {
    try { return std::forward<Build>(build)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class Array> decltype(auto) distanceAt(Array& values, std::size_t index, std::size_t sourceLine) {
    const auto length = values.size();
    if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/cv/distancetransform.d"), sourceLine);
    return values[index];
}
}
inline constexpr float floatINF = std::numeric_limits<float>::max();
using Distance1dResult = std::tuple<compat::Slice<float>, compat::Slice<std::int32_t>>;
Distance1dResult dt1d(std::span<const float> f);
}
