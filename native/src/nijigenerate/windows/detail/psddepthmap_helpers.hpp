#pragma once
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/io/depthsample.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <common/resource_key_map.hpp>
#include <memory>
#include <cfenv>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>
#if defined(_M_X64) || defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#include <xmmintrin.h>
#endif
namespace nijigenerate::windows::psddepthmap::detail::helpers {
template<class T, class... Args> std::shared_ptr<T> allocatePsdDepthObject(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(args)...);
}

template<class Allocate> auto allocatePsdDepthMemory(Allocate allocate) {
    try {
        return allocate();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline compat::SourceCString sourceCString(compat::RetainedString value) {
    compat::SourceCString result{compat::RetainedStringSlice(value)};
    compat::detail::ngSourceCStringPostconditions(value.view(), result.data());
    return result;
}

template<std::integral T> compat::RetainedString decimal(T value) {
    return allocatePsdDepthMemory([&] { return compat::RetainedString(std::to_string(value)); });
}

template<std::size_t N, std::integral T>
compat::RetainedString prefixedDecimal(const char (&prefix)[N], T value) {
    return allocatePsdDepthMemory([&] {
        const auto suffix = std::to_string(value);
        return compat::RetainedString(std::string(prefix, N - 1) + suffix);
    });
}

template<std::size_t N> compat::RetainedString keyedText(const char (&prefix)[N],
    compat::RetainedString path, std::uint64_t uuid) {
    return allocatePsdDepthMemory([&] {
        std::string result(prefix, N - 1);
        result.append(path.view());
        const auto suffix = std::to_string(uuid);
        result.append(suffix);
        return compat::RetainedString(std::move(result));
    });
}
template<std::size_t N> compat::RetainedString tr(const char (&value)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(value));
}
template<class T> T dmin(T a, T b) { return b < a ? b : a; }
template<class T> T dmax(T a, T b) { return a < b ? b : a; }
template<class T> T dclamp(T value, T lower, T upper) {
    return value < lower ? lower : upper < value ? upper : value;
}
inline std::string keyOf(compat::RetainedString value) {
    return allocatePsdDepthMemory([&] { return std::string(value.view()); });
}
template<class K, class T, class Hash, class Equal>
std::shared_ptr<T> lookup(nicxlive::core::common::ResourceKeyMap<K, T, Hash, Equal>& values, const K& key) {
    const auto found = values.find(key);
    return found == values.end() ? std::shared_ptr<T>{} : found.retainValue();
}
template<class T> std::shared_ptr<T> retainedElement(const compat::Slice<T>& values, T* element) {
    // Alias the real slice cell, including its original backing after reload.
    // Process-static backing legitimately needs no owning allocation.
    return std::shared_ptr<T>(values.retentionOwner(), element);
}
template<class Map> Map duplicateCache(const Map& values) {
    Map result;
    for (const auto& [key, value] : values) result[key] = value;
    return result;
}
using io::depthsample::detail::add32;
using io::depthsample::detail::sub32;
using io::depthsample::detail::mul32;
// Windows x64 source conversion: CVTTSS2SI's indefinite result for invalid inputs.
inline int toSourceInt(float value) {
#if defined(_M_X64) || defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
    return _mm_cvtt_ss2si(_mm_set_ss(value));
#else
    if (!std::isfinite(value) || value < -2147483648.0f || value >= 2147483648.0f) {
        (void)std::feraiseexcept(FE_INVALID);
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
#endif
}
template<class... Args> compat::RetainedString format(std::string_view pattern, Args&&... values) {
    using namespace core::translation;
    const std::array<TypedFormatArgument, sizeof...(Args)> arguments{
        ngFormatArgument(std::forward<Args>(values))...};
    auto formatted = ngFormatTyped(pattern, arguments);
    return allocatePsdDepthMemory([&] { return compat::RetainedString(std::move(formatted)); });
}
template<class Pattern, class... Args> requires std::same_as<Pattern, compat::RetainedString>
compat::RetainedString format(Pattern pattern, Args&&... values) {
    return format(pattern.view(), std::forward<Args>(values)...);
}
}
