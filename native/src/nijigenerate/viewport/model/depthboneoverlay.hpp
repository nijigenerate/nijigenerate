#pragma once
#include <nijigenerate/compat/slice.hpp>
#include <math/veca.hpp>
#include <memory>

namespace nicxlive::core::nodes { class Node; }
namespace nijigenerate::ext::nodes { class ExDepthBone; class ExDepthRigRoot; }
namespace nijigenerate::viewport::model::depthboneoverlay {
struct DepthBoneOverlayGeometry {
    nicxlive::core::math::Vec3Array lines, parentLines, childLines, points, selectedPoints;
};
DepthBoneOverlayGeometry buildDepthBoneOverlayGeometry(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> selectedBone = nullptr);
void drawDepthBones(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> selectedBone = nullptr);
void depthBoneEffectivePivotSelectionChanged(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> nodes);
void drawDepthBoneEffectivePivots(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<ext::nodes::ExDepthBone> selectedBone = nullptr);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::model::depthboneoverlay::DepthBoneOverlayGeometry> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.model.depthboneoverlay.DepthBoneOverlayGeometry)";
    inline static constexpr char unqualifiedArgument[] = "(DepthBoneOverlayGeometry)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.model.depthboneoverlay.DepthBoneOverlayGeometry";
    inline static constexpr char unqualifiedElement[] = "DepthBoneOverlayGeometry";
};
}
