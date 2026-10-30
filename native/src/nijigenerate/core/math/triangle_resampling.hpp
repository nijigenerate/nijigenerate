#pragma once
#include <math/veca.hpp>
#include <memory>
#include <type_traits>
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "vertex.hpp"
#include <param/binding.hpp>
#include <nodes/deformable.hpp>

namespace nicxlive::core::param { class DeformationParameterBinding; }

namespace nicxlive::core::nodes {
class GridDeformer;
class PathDeformer;
class Drawable;
}

namespace nijigenerate::core::math::triangle {
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    ::nicxlive::core::param::DeformationParameterBinding* binding,
    ::nicxlive::core::param::DeformationParameterBinding* sourceBinding,
    ::nicxlive::core::param::Vec2u index, bool flipHorz = false);
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    ::nicxlive::core::math::Vec2Array vertices,
    ::nicxlive::core::param::DeformationParameterBinding* binding,
    ::nicxlive::core::param::Vec2u index, bool flipHorz = false);
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    ::nicxlive::core::math::Vec2Array vertices, ::nicxlive::core::nodes::Drawable* drawable,
    ::nicxlive::core::nodes::Deformation deform, bool flipHorz = false);
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    ::nicxlive::core::math::Vec2Array vertices, ::nicxlive::core::nodes::PathDeformer* path,
    ::nicxlive::core::nodes::Deformation deform, bool flipHorz = false);
// Source value parameters retain descriptor snapshots; working duplicates belong to the body.
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    ::nicxlive::core::math::Vec2Array vertices, ::nicxlive::core::nodes::GridDeformer* deformable,
    ::nicxlive::core::nodes::Deformation deform, bool flipHorz = false);
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    compat::Slice<::nicxlive::core::math::Vec2> vertices, ::nicxlive::core::nodes::GridDeformer* deformable,
    ::nicxlive::core::nodes::Deformation deform, bool flipHorz = false);

template<class Array>
::nicxlive::core::math::Vec2Array toVec2Array(Array vertices) {
    using Value = std::remove_cvref_t<decltype(vertices[std::size_t{}])>;
    static_assert(std::is_same_v<Value, ::nicxlive::core::math::Vec2> ||
        std::is_same_v<Value, ::nijigenerate::core::math::mesh::MeshVertex*>,
        "Unsupported vertex array type");
    ::nicxlive::core::math::Vec2Array result(vertices.size());
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const auto value = vertices[i];
        auto destination = result[i];
        if constexpr (requires { value->position; }) destination = compat::ngRequireObject(value)->position;
        else destination = value;
    }
    return result;
}
inline ::nicxlive::core::math::Vec2Array toVec2Array(::nicxlive::core::math::Vec2Array vertices) {
    return vertices;
}

template<class Array, class Target>
requires (std::is_convertible_v<Target*, ::nicxlive::core::nodes::Drawable*> ||
    std::is_convertible_v<Target*, ::nicxlive::core::nodes::PathDeformer*> ||
    std::is_convertible_v<Target*, ::nicxlive::core::nodes::GridDeformer*>)
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    Array vertices, Target* target, ::nicxlive::core::nodes::Deformation deform,
    bool flipHorz = false) {
    // Select the actual typed body even when Target is a native derived class.
    // Forwarding Target* unchanged would recursively select this exact-match wrapper.
    if constexpr (std::is_convertible_v<Target*, ::nicxlive::core::nodes::Drawable*>)
        return deformByDeformationBinding(toVec2Array(vertices),
            static_cast<::nicxlive::core::nodes::Drawable*>(target), deform, flipHorz);
    else if constexpr (std::is_convertible_v<Target*, ::nicxlive::core::nodes::PathDeformer*>)
        return deformByDeformationBinding(toVec2Array(vertices),
            static_cast<::nicxlive::core::nodes::PathDeformer*>(target), deform, flipHorz);
    else
        return deformByDeformationBinding(toVec2Array(vertices),
            static_cast<::nicxlive::core::nodes::GridDeformer*>(target), deform, flipHorz);
}
template<class Array>
std::shared_ptr<::nicxlive::core::nodes::Deformation> deformByDeformationBinding(
    Array vertices, ::nicxlive::core::param::DeformationParameterBinding* binding,
    ::nicxlive::core::param::Vec2u index, bool flipHorz = false) {
    return deformByDeformationBinding(toVec2Array(vertices), binding, index, flipHorz);
}
}
