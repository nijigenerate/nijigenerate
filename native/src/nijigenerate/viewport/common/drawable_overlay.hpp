#pragma once
#include <nodes/drawable.hpp>

namespace nijigenerate::viewport::common {
// Consume the real SDK virtual source methods; no second overlay geometry body.
inline nicxlive::core::math::Mat4 ngMeshOverlayMatrix(nicxlive::core::nodes::Drawable& drawable) {
    return drawable.meshOverlayMatrix();
}
inline nicxlive::core::math::Vec2Array ngMeshOverlayPoints(nicxlive::core::nodes::Drawable& drawable) {
    return drawable.meshOverlayPoints();
}
}
