#pragma once
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/vec3u.hpp"
#include <math/veca.hpp>
#include <tuple>
#include <functional>
#include "mesh_vertices.hpp"

namespace nijigenerate::core::math::triangle {
using Triangulation = std::tuple<::nicxlive::core::math::Vec2Array, compat::Slice<compat::Vec3u>>;
Triangulation triangulatePositions(std::size_t count,
    const std::function<::nicxlive::core::math::Vec2(std::size_t)>& position,
    ::nicxlive::core::math::Vec4 bounds);
// Actual vec2[] specialization used by the original AutoMesh callers.
Triangulation triangulate(compat::Slice<::nicxlive::core::math::Vec2> vertices,
    ::nicxlive::core::math::Vec4 bounds);
Triangulation triangulate(mesh::MeshVertexArray vertices, ::nicxlive::core::math::Vec4 bounds);
}
