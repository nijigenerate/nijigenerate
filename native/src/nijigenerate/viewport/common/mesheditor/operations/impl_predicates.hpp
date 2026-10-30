#pragma once

#include <nijigenerate/core/math/mesh_declarations.hpp>
#include <math/types.hpp>
#include <concepts>

namespace nijigenerate::viewport::common::mesheditor::operations::impl {

template<class T> requires std::same_as<T, ::nijigenerate::core::math::mesh::MeshVertex*>
inline bool toBool(T vtx) {
    return vtx != nullptr;
}

template<class T> requires std::same_as<T, ::nicxlive::core::math::Vec2>
inline bool toBool(T vtx) {
    (void)vtx;
    return true;
}

} // namespace nijigenerate::viewport::common::mesheditor::operations::impl
