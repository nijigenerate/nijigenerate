#pragma once

#include "byte_view.hpp"
#include <compat/object_metadata.hpp>

namespace nicxlive::core::d_runtime {

// Image.data<T> casts the retained bytes to the declared T[] without changing
// their backing. ByteView stores that array; it is not an Object or Mir Slice.
template<class T> struct SourceTypeSpellings<::nijigenerate::compat::ByteView<T>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() { return ngSourceDynamicArrayTypeSpellings<T>(); }
};

} // namespace nicxlive::core::d_runtime
