#pragma once
#include <nodes/path_deformer.hpp>

namespace nijigenerate::ext::nodes {
// The actual virtual source property returns the real curve/mesh descriptor.
inline void ngAssignDeformableVertices(nicxlive::core::nodes::Deformable& node,
    const nicxlive::core::math::Vec2Array& value) {
    node.getVertices() = value;
}
inline void ngAppendDeformableVertex(nicxlive::core::nodes::Deformable& node,
    nicxlive::core::math::Vec2 value) {
    node.getVertices().append(value);
}
}
