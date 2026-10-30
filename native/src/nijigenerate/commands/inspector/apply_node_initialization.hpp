#pragma once

#include "node_commands.hpp"
#include "drawable_commands.hpp"
#include "camera_command.hpp"
#include "composite_commands.hpp"
#include "part_commands.hpp"
#include "meshgroup_commands.hpp"
#include "griddeform_commands.hpp"
#include "path_commands.hpp"
#include "simplephysics_commands.hpp"
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>

namespace nijigenerate::commands::inspector::apply_node::initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
template<auto Id, class C>
using Optional = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
// The actual source EnumMembers pass, using the real declared command aliases.
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Optional<InspectorNodeApplyCommand::TranslationX, TranslationXCommand>,
    Optional<InspectorNodeApplyCommand::TranslationY, TranslationYCommand>,
    Optional<InspectorNodeApplyCommand::TranslationZ, TranslationZCommand>,
    Optional<InspectorNodeApplyCommand::RotationX, RotationXCommand>,
    Optional<InspectorNodeApplyCommand::RotationY, RotationYCommand>,
    Optional<InspectorNodeApplyCommand::RotationZ, RotationZCommand>,
    Optional<InspectorNodeApplyCommand::ScaleX, ScaleXCommand>,
    Optional<InspectorNodeApplyCommand::ScaleY, ScaleYCommand>,
    Optional<InspectorNodeApplyCommand::PixelSnap, PixelSnapCommand>,
    Optional<InspectorNodeApplyCommand::TogglePixelSnap, TogglePixelSnapCommand>,
    Optional<InspectorNodeApplyCommand::ZSort, ZSortCommand>,
    Optional<InspectorNodeApplyCommand::LockToRoot, LockToRootCommand>,
    Optional<InspectorNodeApplyCommand::ToggleLockToRoot, ToggleLockToRootCommand>,
    Optional<InspectorNodeApplyCommand::PinToMesh, PinToMeshCommand>,
    Optional<InspectorNodeApplyCommand::TogglePinToMesh, TogglePinToMeshCommand>,
    Optional<InspectorNodeApplyCommand::OffsetX, OffsetXCommand>,
    Optional<InspectorNodeApplyCommand::OffsetY, OffsetYCommand>,
    Optional<InspectorNodeApplyCommand::ViewportOrigin, ViewportOriginCommand>,
    Optional<InspectorNodeApplyCommand::CompositeTint, CompositeTintCommand>,
    Optional<InspectorNodeApplyCommand::CompositeScreenTint, CompositeScreenTintCommand>,
    Optional<InspectorNodeApplyCommand::CompositeBlendingMode, CompositeBlendingModeCommand>,
    Optional<InspectorNodeApplyCommand::CompositeOpacity, CompositeOpacityCommand>,
    Optional<InspectorNodeApplyCommand::CompositeThreshold, CompositeThresholdCommand>,
    Optional<InspectorNodeApplyCommand::PartTint, PartTintCommand>,
    Optional<InspectorNodeApplyCommand::PartScreenTint, PartScreenTintCommand>,
    Optional<InspectorNodeApplyCommand::PartEmissionStrength, PartEmissionStrengthCommand>,
    Optional<InspectorNodeApplyCommand::PartBlendingMode, PartBlendingModeCommand>,
    Optional<InspectorNodeApplyCommand::PartOpacity, PartOpacityCommand>,
    Optional<InspectorNodeApplyCommand::PartMaskAlphaThreshold, PartMaskAlphaThresholdCommand>,
    Optional<InspectorNodeApplyCommand::PartAutoResizedMesh, PartAutoResizedMeshCommand>,
    Optional<InspectorNodeApplyCommand::TogglePartAutoResizedMesh, TogglePartAutoResizedMeshCommand>,
    Optional<InspectorNodeApplyCommand::MeshGroupDynamic, MeshGroupDynamicCommand>,
    Optional<InspectorNodeApplyCommand::ToggleMeshGroupDynamic, ToggleMeshGroupDynamicCommand>,
    Optional<InspectorNodeApplyCommand::MeshGroupTranslateChildren, MeshGroupTranslateChildrenCommand>,
    Optional<InspectorNodeApplyCommand::ToggleMeshGroupTranslateChildren, ToggleMeshGroupTranslateChildrenCommand>,
    Optional<InspectorNodeApplyCommand::GridDeformDynamic, GridDeformDynamicCommand>,
    Optional<InspectorNodeApplyCommand::ToggleGridDeformDynamic, ToggleGridDeformDynamicCommand>,
    Optional<InspectorNodeApplyCommand::PathDeformDynamic, PathDeformDynamicCommand>,
    Optional<InspectorNodeApplyCommand::TogglePathDeformDynamic, TogglePathDeformDynamicCommand>,
    Optional<InspectorNodeApplyCommand::PathDeformPhysicsEnabled, PathDeformPhysicsEnabledCommand>,
    Optional<InspectorNodeApplyCommand::TogglePathDeformPhysicsEnabled, TogglePathDeformPhysicsEnabledCommand>,
    Optional<InspectorNodeApplyCommand::PathDeformCurveType, PathDeformCurveTypeCommand>,
    Optional<InspectorNodeApplyCommand::PathDeformGravity, PathDeformGravityCommand>,
    Optional<InspectorNodeApplyCommand::PathDeformRestoreConstant, PathDeformRestoreConstantCommand>,
    Optional<InspectorNodeApplyCommand::PathDeformDamping, PathDeformDampingCommand>,
    Optional<InspectorNodeApplyCommand::PathDeformInputScale, PathDeformInputScaleCommand>,
    Optional<InspectorNodeApplyCommand::PathDeformPropagateScale, PathDeformPropagateScaleCommand>,
    Optional<InspectorNodeApplyCommand::SimplePhysicsModelType, SimplePhysicsModelTypeCommand>,
    Optional<InspectorNodeApplyCommand::SimplePhysicsMapMode, SimplePhysicsMapModeCommand>,
    Optional<InspectorNodeApplyCommand::SimplePhysicsLocalOnly, SimplePhysicsLocalOnlyCommand>,
    Optional<InspectorNodeApplyCommand::ToggleSimplePhysicsLocalOnly, ToggleSimplePhysicsLocalOnlyCommand>,
    Optional<InspectorNodeApplyCommand::SimplePhysicsGravity, SimplePhysicsGravityCommand>,
    Optional<InspectorNodeApplyCommand::SimplePhysicsLength, SimplePhysicsLengthCommand>,
    Optional<InspectorNodeApplyCommand::SimplePhysicsFrequency, SimplePhysicsFrequencyCommand>,
    Optional<InspectorNodeApplyCommand::SimplePhysicsAngleDamping, SimplePhysicsAngleDampingCommand>,
    Optional<InspectorNodeApplyCommand::SimplePhysicsLengthDamping, SimplePhysicsLengthDampingCommand>,
    Optional<InspectorNodeApplyCommand::SimplePhysicsOutputScaleX, SimplePhysicsOutputScaleXCommand>,
    Optional<InspectorNodeApplyCommand::SimplePhysicsOutputScaleY, SimplePhysicsOutputScaleYCommand>>;
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<inspector::apply_node::InspectorNodeApplyCommand> {
    static consteval bool supported() {
        return inspector::apply_node::initialization::Program::supported<inspector::apply_node::initialization::Table>();
    }
    template<class ActualKey = inspector::apply_node::InspectorNodeApplyCommand>
        requires (std::same_as<ActualKey, inspector::apply_node::InspectorNodeApplyCommand> && supported())
    static void invoke() {
        inspector::apply_node::ngInitCommands<ActualKey>();
    }
};
}

