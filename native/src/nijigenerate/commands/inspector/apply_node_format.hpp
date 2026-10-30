#pragma once

#include "apply_node.hpp"
#include <nijigenerate/core/typed_format.hpp>

namespace nijigenerate::core::translation {

// Names and order match the original declaration in commands/inspector/apply_node.d.
// Keep metadata separate from the accepted command enum and its dispatch registry.
template<> struct FormatEnumTraits<commands::inspector::apply_node::InspectorNodeApplyCommand> {
    using Enum = commands::inspector::apply_node::InspectorNodeApplyCommand;
    static constexpr std::string_view name = "InspectorNodeApplyCommand";
    static constexpr std::array<FormatEnumMember<Enum>, 58> members{{
        {Enum::TranslationX, "TranslationX"},
        {Enum::TranslationY, "TranslationY"},
        {Enum::TranslationZ, "TranslationZ"},
        {Enum::RotationX, "RotationX"},
        {Enum::RotationY, "RotationY"},
        {Enum::RotationZ, "RotationZ"},
        {Enum::ScaleX, "ScaleX"},
        {Enum::ScaleY, "ScaleY"},
        {Enum::PixelSnap, "PixelSnap"},
        {Enum::TogglePixelSnap, "TogglePixelSnap"},
        {Enum::ZSort, "ZSort"},
        {Enum::LockToRoot, "LockToRoot"},
        {Enum::ToggleLockToRoot, "ToggleLockToRoot"},
        {Enum::PinToMesh, "PinToMesh"},
        {Enum::TogglePinToMesh, "TogglePinToMesh"},
        {Enum::OffsetX, "OffsetX"},
        {Enum::OffsetY, "OffsetY"},
        {Enum::ViewportOrigin, "ViewportOrigin"},
        {Enum::CompositeTint, "CompositeTint"},
        {Enum::CompositeScreenTint, "CompositeScreenTint"},
        {Enum::CompositeBlendingMode, "CompositeBlendingMode"},
        {Enum::CompositeOpacity, "CompositeOpacity"},
        {Enum::CompositeThreshold, "CompositeThreshold"},
        {Enum::PartTint, "PartTint"},
        {Enum::PartScreenTint, "PartScreenTint"},
        {Enum::PartEmissionStrength, "PartEmissionStrength"},
        {Enum::PartBlendingMode, "PartBlendingMode"},
        {Enum::PartOpacity, "PartOpacity"},
        {Enum::PartMaskAlphaThreshold, "PartMaskAlphaThreshold"},
        {Enum::PartAutoResizedMesh, "PartAutoResizedMesh"},
        {Enum::TogglePartAutoResizedMesh, "TogglePartAutoResizedMesh"},
        {Enum::MeshGroupDynamic, "MeshGroupDynamic"},
        {Enum::ToggleMeshGroupDynamic, "ToggleMeshGroupDynamic"},
        {Enum::MeshGroupTranslateChildren, "MeshGroupTranslateChildren"},
        {Enum::ToggleMeshGroupTranslateChildren, "ToggleMeshGroupTranslateChildren"},
        {Enum::GridDeformDynamic, "GridDeformDynamic"},
        {Enum::ToggleGridDeformDynamic, "ToggleGridDeformDynamic"},
        {Enum::PathDeformDynamic, "PathDeformDynamic"},
        {Enum::TogglePathDeformDynamic, "TogglePathDeformDynamic"},
        {Enum::PathDeformPhysicsEnabled, "PathDeformPhysicsEnabled"},
        {Enum::TogglePathDeformPhysicsEnabled, "TogglePathDeformPhysicsEnabled"},
        {Enum::PathDeformCurveType, "PathDeformCurveType"},
        {Enum::PathDeformGravity, "PathDeformGravity"},
        {Enum::PathDeformRestoreConstant, "PathDeformRestoreConstant"},
        {Enum::PathDeformDamping, "PathDeformDamping"},
        {Enum::PathDeformInputScale, "PathDeformInputScale"},
        {Enum::PathDeformPropagateScale, "PathDeformPropagateScale"},
        {Enum::SimplePhysicsModelType, "SimplePhysicsModelType"},
        {Enum::SimplePhysicsMapMode, "SimplePhysicsMapMode"},
        {Enum::SimplePhysicsLocalOnly, "SimplePhysicsLocalOnly"},
        {Enum::ToggleSimplePhysicsLocalOnly, "ToggleSimplePhysicsLocalOnly"},
        {Enum::SimplePhysicsGravity, "SimplePhysicsGravity"},
        {Enum::SimplePhysicsLength, "SimplePhysicsLength"},
        {Enum::SimplePhysicsFrequency, "SimplePhysicsFrequency"},
        {Enum::SimplePhysicsAngleDamping, "SimplePhysicsAngleDamping"},
        {Enum::SimplePhysicsLengthDamping, "SimplePhysicsLengthDamping"},
        {Enum::SimplePhysicsOutputScaleX, "SimplePhysicsOutputScaleX"},
        {Enum::SimplePhysicsOutputScaleY, "SimplePhysicsOutputScaleY"},
    }};
};

} // namespace nijigenerate::core::translation

