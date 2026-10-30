#pragma once
#include "typed_format.hpp"
#include <math/types.hpp>
#include <array>

namespace nijigenerate::core::translation {
template<> struct FormatSourceTypeName<nicxlive::core::math::Vec2> {
    // inmath.linalg.Vector!(float, 2): the alias names the actual template
    // instance; DMD's type printer spells its int dimension without a suffix.
    static std::string get() { return detail::copyFormatTypeName("Vector!(float, 2)"); }
};
template<> struct FormatObjectTraits<nicxlive::core::math::Vec2> {
    inline static constexpr char typeName[] = "Vector!(float, 2)";
    static compat::RetainedString toString(const nicxlive::core::math::Vec2& value) {
        // Evaluate Vector.toString only after the outer specifier is accepted.
        std::array<float, 2> elements{value.x, value.y};
        const std::array<TypedFormatArgument, 1> arguments{ngFormatFixedArrayReferenceArgument(elements)};
        return detail::retainFormatMessage(ngFormatTyped("%s", arguments));
    }
};
}
