#pragma once
#include <nodes/defstack.hpp>
#include <param/binding_impl.hpp>
#include <memory>

namespace deformation_interpolation {
// Original private helpers, published together for whole-file source composition.
void benchmarkLinear(std::shared_ptr<nicxlive::core::param::DeformationParameterBinding> binding, bool direct);
nicxlive::core::nodes::Deformation legacyLinear(
    std::shared_ptr<nicxlive::core::param::DeformationParameterBinding> binding,
    nicxlive::core::param::Vec2u left, nicxlive::core::math::Vec2 offset);
void ngDeformationInterpolationMain();
}
