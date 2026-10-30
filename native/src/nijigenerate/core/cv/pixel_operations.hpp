#pragma once

#include <bit>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include <nijigenerate/compat/source_reference.hpp>

namespace nijigenerate::core::cv::contours {
// Struct mappings specialize this producer with actual Source comparison and
// implicit-assignment bodies (including evaluation of their alias-this member).
// Missing mappings are hard template errors, never a Source predicate result.
template<class T, class = void> struct ContourPixelOperations;

template<class T>
struct ContourPixelOperations<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    static bool nonzero(T value) { return value != 0; }
    static std::int32_t assignInt(T value)
        requires (std::is_integral_v<T> && sizeof(T) <= sizeof(std::int32_t)) {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
    }
};

template<class T>
struct ContourPixelOperations<T, std::enable_if_t<std::is_enum_v<T>>> {
    using Base = std::underlying_type_t<T>;
    static bool nonzero(T value) {
        return ContourPixelOperations<Base>::nonzero(static_cast<Base>(value));
    }
    static std::int32_t assignInt(T value)
        requires requires(Base base) { ContourPixelOperations<Base>::assignInt(base); } {
        return ContourPixelOperations<Base>::assignInt(static_cast<Base>(value));
    }
};

// Byte-backed MIR references denote the actual Source scalar, including a
// readonly scalar. Load that cell once per authored comparison/assignment;
// do not search native conversions or create a detached pixel model.
template<compat::SourceByteReferenceElement T>
struct ContourPixelOperations<compat::SourceReference<T>> {
    using Value = typename compat::SourceReference<T>::value_type;
    static bool nonzero(const compat::SourceReference<T>& value) {
        return ContourPixelOperations<Value>::nonzero(value.load());
    }
    static std::int32_t assignInt(const compat::SourceReference<T>& value)
        requires requires(Value scalar) { ContourPixelOperations<Value>::assignInt(scalar); } {
        return ContourPixelOperations<Value>::assignInt(value.load());
    }
};

// A defining struct with Source `alias member this` can derive its operation
// specialization from this body using the actual mapped data/getter member.
// std::invoke preserves the returned reference and evaluates that member once;
// no synthetic scalar object or native conversion search substitutes for it.
template<auto Member> struct ContourAliasThisPixelOperations {
    template<class T> static bool nonzero(T&& value) {
        using Alias = std::remove_cvref_t<decltype(std::invoke(Member, std::forward<T>(value)))>;
        return ContourPixelOperations<Alias>::nonzero(std::invoke(Member, std::forward<T>(value)));
    }
    template<class T> static std::int32_t assignInt(T&& value) {
        using Alias = std::remove_cvref_t<decltype(std::invoke(Member, std::forward<T>(value)))>;
        return ContourPixelOperations<Alias>::assignInt(std::invoke(Member, std::forward<T>(value)));
    }
};
}

