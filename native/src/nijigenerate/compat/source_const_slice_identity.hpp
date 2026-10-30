#pragma once

#include "const_slice.hpp"
#include <compat/object_metadata.hpp>
#include <type_traits>

namespace nicxlive::core::d_runtime {

// For actual basic values, readonly cells are exactly Source const(T) cells.
// Deep aggregate/class projections require their defining representation; this
// record does not infer them from native top-level const on a retained handle.
template<class T> requires (std::is_arithmetic_v<T> && !std::is_volatile_v<T>)
struct SourceTypeSpellings<::nijigenerate::compat::ConstSlice<T>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() { return ngSourceDynamicArrayTypeSpellings<const T>(); }
};

} // namespace nicxlive::core::d_runtime
