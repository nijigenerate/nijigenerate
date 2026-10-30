#pragma once
#include <cstdint>
#include <concepts>
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::inspector::apply_node {

enum class InspectorNodeApplyCommand : std::int32_t {
    TranslationX = 0,
    TranslationY = 1,
    TranslationZ = 2,
    RotationX = 3,
    RotationY = 4,
    RotationZ = 5,
    ScaleX = 6,
    ScaleY = 7,
    PixelSnap = 8,
    TogglePixelSnap = 9,
    ZSort = 10,
    LockToRoot = 11,
    ToggleLockToRoot = 12,
    PinToMesh = 13,
    TogglePinToMesh = 14,
    OffsetX = 15,
    OffsetY = 16,
    ViewportOrigin = 17,
    CompositeTint = 18,
    CompositeScreenTint = 19,
    CompositeBlendingMode = 20,
    CompositeOpacity = 21,
    CompositeThreshold = 22,
    PartTint = 23,
    PartScreenTint = 24,
    PartEmissionStrength = 25,
    PartBlendingMode = 26,
    PartOpacity = 27,
    PartMaskAlphaThreshold = 28,
    PartAutoResizedMesh = 29,
    TogglePartAutoResizedMesh = 30,
    MeshGroupDynamic = 31,
    ToggleMeshGroupDynamic = 32,
    MeshGroupTranslateChildren = 33,
    ToggleMeshGroupTranslateChildren = 34,
    GridDeformDynamic = 35,
    ToggleGridDeformDynamic = 36,
    PathDeformDynamic = 37,
    TogglePathDeformDynamic = 38,
    PathDeformPhysicsEnabled = 39,
    TogglePathDeformPhysicsEnabled = 40,
    PathDeformCurveType = 41,
    PathDeformGravity = 42,
    PathDeformRestoreConstant = 43,
    PathDeformDamping = 44,
    PathDeformInputScale = 45,
    PathDeformPropagateScale = 46,
    SimplePhysicsModelType = 47,
    SimplePhysicsMapMode = 48,
    SimplePhysicsLocalOnly = 49,
    ToggleSimplePhysicsLocalOnly = 50,
    SimplePhysicsGravity = 51,
    SimplePhysicsLength = 52,
    SimplePhysicsFrequency = 53,
    SimplePhysicsAngleDamping = 54,
    SimplePhysicsLengthDamping = 55,
    SimplePhysicsOutputScaleX = 56,
    SimplePhysicsOutputScaleY = 57
};
void ngInitInspectorNodeApplyCommands();
template<class T> requires std::same_as<T,InspectorNodeApplyCommand>
void ngInitCommands() {ngInitInspectorNodeApplyCommands();}

}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::commands::inspector::apply_node::InspectorNodeApplyCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.inspector.apply_node.InspectorNodeApplyCommand)";
    inline static constexpr char unqualifiedArgument[] = "(InspectorNodeApplyCommand)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.inspector.apply_node.InspectorNodeApplyCommand";
    inline static constexpr char unqualifiedElement[] = "InspectorNodeApplyCommand";
};
} // namespace nicxlive::core::d_runtime
