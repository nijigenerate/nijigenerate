#pragma once

#include <math/types.hpp>
#include "nijigenerate/compat/retained_string.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>

#include <cstddef>
#include <limits>
#include <span>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>

namespace nijigenerate::commands::model::set_deform_binding::detail {

template<class ReadValue>
inline bool ngResolveVec2ArgImpl(std::size_t count, ReadValue readValue,
    nicxlive::core::math::Vec2 fallback, nicxlive::core::math::Vec2& result,
    compat::RetainedString name, compat::RetainedString& error) {
    // D initializes out parameters before entering the function body.
    result = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    error.reset();
    if (count == 0) {
        result = fallback;
        return true;
    }
    if (count != 2) {
        try {
            error = compat::RetainedString(std::string(name.view()) + " must be [x,y]");
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return false;
    }
    result = {readValue(0), readValue(1)};
    return true;
}

inline bool ngResolveVec2Arg(std::span<const float> values, nicxlive::core::math::Vec2 fallback,
    nicxlive::core::math::Vec2& result, compat::RetainedString name, compat::RetainedString& error) {
    return ngResolveVec2ArgImpl(values.size(), [values](std::size_t i) { return values[i]; },
        fallback, result, name, error);
}

// A D slice can alias the result's two coordinates. Access members directly;
// pointer arithmetic across the C++ struct's distinct members is not an array view.
inline bool ngResolveVec2Arg(nicxlive::core::math::Vec2& values, nicxlive::core::math::Vec2 fallback,
    nicxlive::core::math::Vec2& result, compat::RetainedString name, compat::RetainedString& error) {
    return ngResolveVec2ArgImpl(2, [&values](std::size_t i) { return i == 0 ? values.x : values.y; },
        fallback, result, name, error);
}

}
