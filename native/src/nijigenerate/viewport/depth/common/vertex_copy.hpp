#pragma once

#include "nijigenerate/compat/slice.hpp"
#include <math/veca.hpp>

namespace nijigenerate::viewport::depth::common::targetview::detail {

inline compat::Slice<nicxlive::core::math::Vec2> copyVertices(const nicxlive::core::math::Vec2Array& vertices) {
    auto result = vertices.toArray();
    if (result.empty()) return {};
    return compat::Slice<nicxlive::core::math::Vec2>{std::move(result)};
}

} // namespace nijigenerate::viewport::depth::common::targetview::detail
