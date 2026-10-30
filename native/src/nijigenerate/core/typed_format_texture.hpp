#pragma once
#include "typed_format.hpp"
#include <texture.hpp>

namespace nijigenerate::core::translation {
template<> struct FormatObjectTraits<std::shared_ptr<nicxlive::core::Texture>> {
    inline static constexpr char typeName[] = "Texture";
    static compat::RetainedString toString(const std::shared_ptr<nicxlive::core::Texture>& value) {
        if (!value) return compat::RetainedString::fromStaticLiteral("null");
        return value->toString();
    }
};
}
