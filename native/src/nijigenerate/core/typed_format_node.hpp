#pragma once
#include "typed_format.hpp"
#include <nodes/node.hpp>

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<nicxlive::core::nodes::Node>> {
    inline static constexpr char typeName[] = "Node";
    static compat::RetainedString toString(const std::shared_ptr<nicxlive::core::nodes::Node>& value) {
        if (!value) return compat::RetainedString::fromStaticLiteral("null");
        return value->toString();
    }
};

} // namespace nijigenerate::core::translation
