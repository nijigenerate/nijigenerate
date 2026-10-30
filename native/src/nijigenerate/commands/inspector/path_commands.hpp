#pragma once
// Concrete commands from the independent commands/inspector/apply_node.d.
#include "apply_node_commands.hpp"
#include "apply_node_table.hpp"
#include <nijigenerate/panels/inspector/pathdeform.hpp>
namespace nijigenerate::commands::inspector::apply_node {
using PathApplyInspector = panels::inspector::pathdeform::NodeInspector<panels::inspector::ModelEditSubMode::Layout>;
using PathDeformDynamicAccess = InspectorAttributeAccess<PathApplyInspector, &PathApplyInspector::dynamic>;
using PathDeformDynamicCommandBase = ApplyInspectorPropCommand<PathApplyInspector, PathDeformDynamicAccess, "PathDeformer", "bool">;
using PathDeformDynamicCommand = DefApply<"PathDeformDynamic",
    PathApplyInspector, PathDeformDynamicAccess, "PathDeformer", "bool">::Apply;
using TogglePathDeformDynamicCommandBase = ToggleInspectorPropCommand<PathApplyInspector, PathDeformDynamicAccess, "PathDeformer">;
using TogglePathDeformDynamicCommand = DefApply<"PathDeformDynamic",
    PathApplyInspector, PathDeformDynamicAccess, "PathDeformer", "bool">::Toggle;
using PathDeformPhysicsEnabledAccess = InspectorAttributeAccess<PathApplyInspector, &PathApplyInspector::physicsEnabled>;
using PathDeformPhysicsEnabledCommandBase = ApplyInspectorPropCommand<PathApplyInspector, PathDeformPhysicsEnabledAccess, "PathDeformer", "bool">;
using PathDeformPhysicsEnabledCommand = DefApply<"PathDeformPhysicsEnabled",
    PathApplyInspector, PathDeformPhysicsEnabledAccess, "PathDeformer", "bool">::Apply;
using TogglePathDeformPhysicsEnabledCommandBase = ToggleInspectorPropCommand<PathApplyInspector, PathDeformPhysicsEnabledAccess, "PathDeformer">;
using TogglePathDeformPhysicsEnabledCommand = DefApply<"PathDeformPhysicsEnabled",
    PathApplyInspector, PathDeformPhysicsEnabledAccess, "PathDeformer", "bool">::Toggle;
using PathDeformCurveTypeAccess = InspectorAttributeAccess<PathApplyInspector, &PathApplyInspector::curveType>;
using PathDeformCurveTypeCommandBase = ApplyInspectorPropCommand<PathApplyInspector, PathDeformCurveTypeAccess, "PathDeformer", "CurveType">;
using PathDeformCurveTypeCommand = DefApply<"PathDeformCurveType",
    PathApplyInspector, PathDeformCurveTypeAccess, "PathDeformer", "CurveType">::Apply;
using PathDeformGravityAccess = InspectorAttributeAccess<PathApplyInspector, &PathApplyInspector::gravity>;
using PathDeformGravityCommandBase = ApplyInspectorPropCommand<PathApplyInspector, PathDeformGravityAccess, "PathDeformer", "float">;
using PathDeformGravityCommand = DefApply<"PathDeformGravity",
    PathApplyInspector, PathDeformGravityAccess, "PathDeformer", "float">::Apply;
using PathDeformRestoreConstantAccess = InspectorAttributeAccess<PathApplyInspector, &PathApplyInspector::restoreConstant>;
using PathDeformRestoreConstantCommandBase = ApplyInspectorPropCommand<PathApplyInspector, PathDeformRestoreConstantAccess, "PathDeformer", "float">;
using PathDeformRestoreConstantCommand = DefApply<"PathDeformRestoreConstant",
    PathApplyInspector, PathDeformRestoreConstantAccess, "PathDeformer", "float">::Apply;
using PathDeformDampingAccess = InspectorAttributeAccess<PathApplyInspector, &PathApplyInspector::damping>;
using PathDeformDampingCommandBase = ApplyInspectorPropCommand<PathApplyInspector, PathDeformDampingAccess, "PathDeformer", "float">;
using PathDeformDampingCommand = DefApply<"PathDeformDamping",
    PathApplyInspector, PathDeformDampingAccess, "PathDeformer", "float">::Apply;
using PathDeformInputScaleAccess = InspectorAttributeAccess<PathApplyInspector, &PathApplyInspector::inputScale>;
using PathDeformInputScaleCommandBase = ApplyInspectorPropCommand<PathApplyInspector, PathDeformInputScaleAccess, "PathDeformer", "float">;
using PathDeformInputScaleCommand = DefApply<"PathDeformInputScale",
    PathApplyInspector, PathDeformInputScaleAccess, "PathDeformer", "float">::Apply;
using PathDeformPropagateScaleAccess = InspectorAttributeAccess<PathApplyInspector, &PathApplyInspector::propagateScale>;
using PathDeformPropagateScaleCommandBase = ApplyInspectorPropCommand<PathApplyInspector, PathDeformPropagateScaleAccess, "PathDeformer", "float">;
using PathDeformPropagateScaleCommand = DefApply<"PathDeformPropagateScale",
    PathApplyInspector, PathDeformPropagateScaleAccess, "PathDeformer", "float">::Apply;
void ngInitPathInspectorCommands();
}
namespace nijigenerate::commands {
template<> struct CommandTraits<inspector::apply_node::PathDeformDynamicCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::PathDeformDynamicCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::TogglePathDeformDynamicCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::TogglePathDeformDynamicCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::PathDeformPhysicsEnabledCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::PathDeformPhysicsEnabledCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::TogglePathDeformPhysicsEnabledCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::TogglePathDeformPhysicsEnabledCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::PathDeformCurveTypeCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::PathDeformCurveTypeCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::PathDeformGravityCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::PathDeformGravityCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::PathDeformRestoreConstantCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::PathDeformRestoreConstantCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::PathDeformDampingCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::PathDeformDampingCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::PathDeformInputScaleCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::PathDeformInputScaleCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::PathDeformPropagateScaleCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::PathDeformPropagateScaleCommandBase>;
};
}
