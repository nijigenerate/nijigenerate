#pragma once
#include "typed_format.hpp"
#include <math/types.hpp>

namespace nijigenerate::core::translation {
template<> struct FormatSourceTypeName<nicxlive::core::math::Vec3> {
    // inmath.linalg.Vector!(float, 3): the alias names the actual template
    // instance; DMD's type printer spells its int dimension without a suffix.
    static std::string get() { return detail::copyFormatTypeName("Vector!(float, 3)"); }
};
template<> struct FormatObjectTraits<nicxlive::core::math::Vec3> {
    inline static constexpr char typeName[] = "Vector!(float, 3)";
    static compat::RetainedString toString(const nicxlive::core::math::Vec3& value) {
        // Evaluate Vector.toString only after the outer specifier is accepted.
        const compat::Slice<float> elements{value.x, value.y, value.z};
        const std::array<TypedFormatArgument, 1> arguments{ngFormatArgument(elements)};
        return detail::retainFormatMessage(ngFormatTyped("%s", arguments));
    }
};
}
