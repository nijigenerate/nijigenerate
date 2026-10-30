#pragma once
#include "apply_node_commands.hpp"
#include "apply_node_table.hpp"
#include <nijigenerate/panels/inspector/simplephysics.hpp>
namespace nijigenerate::commands::inspector::apply_node {
using SimplePhysicsInspector=panels::inspector::simplephysics::NodeInspector<
    panels::inspector::ModelEditSubMode::Layout,panels::inspector::simplephysics::SimplePhysics>;
using SimplePhysicsModelTypeAccess=InspectorAttributeAccess<SimplePhysicsInspector,&SimplePhysicsInspector::modelType>;
using SimplePhysicsModelTypeCommandBase=ApplyInspectorPropCommand<SimplePhysicsInspector,SimplePhysicsModelTypeAccess,"SimplePhysics","PhysicsModel">;
using SimplePhysicsModelTypeCommand = DefApply<"SimplePhysicsModelType",
    SimplePhysicsInspector, SimplePhysicsModelTypeAccess, "SimplePhysics", "PhysicsModel">::Apply;
using SimplePhysicsMapModeAccess=InspectorAttributeAccess<SimplePhysicsInspector,&SimplePhysicsInspector::mapMode>;
using SimplePhysicsMapModeCommandBase=ApplyInspectorPropCommand<SimplePhysicsInspector,SimplePhysicsMapModeAccess,"SimplePhysics","ParamMapMode">;
using SimplePhysicsMapModeCommand = DefApply<"SimplePhysicsMapMode",
    SimplePhysicsInspector, SimplePhysicsMapModeAccess, "SimplePhysics", "ParamMapMode">::Apply;
using SimplePhysicsLocalOnlyAccess=InspectorAttributeAccess<SimplePhysicsInspector,&SimplePhysicsInspector::localOnly>;
using SimplePhysicsLocalOnlyCommandBase=ApplyInspectorPropCommand<SimplePhysicsInspector,SimplePhysicsLocalOnlyAccess,"SimplePhysics","bool">;
using SimplePhysicsLocalOnlyCommand = DefApply<"SimplePhysicsLocalOnly",
    SimplePhysicsInspector, SimplePhysicsLocalOnlyAccess, "SimplePhysics", "bool">::Apply;
using ToggleSimplePhysicsLocalOnlyCommandBase=ToggleInspectorPropCommand<SimplePhysicsInspector,SimplePhysicsLocalOnlyAccess,"SimplePhysics">;
using ToggleSimplePhysicsLocalOnlyCommand = DefApply<"SimplePhysicsLocalOnly",
    SimplePhysicsInspector, SimplePhysicsLocalOnlyAccess, "SimplePhysics", "bool">::Toggle;
using SimplePhysicsGravityAccess=InspectorAttributeAccess<SimplePhysicsInspector,&SimplePhysicsInspector::gravity>;
using SimplePhysicsGravityCommandBase=ApplyInspectorPropCommand<SimplePhysicsInspector,SimplePhysicsGravityAccess,"SimplePhysics","float">;
using SimplePhysicsGravityCommand = DefApply<"SimplePhysicsGravity",
    SimplePhysicsInspector, SimplePhysicsGravityAccess, "SimplePhysics", "float">::Apply;
using SimplePhysicsLengthAccess=InspectorAttributeAccess<SimplePhysicsInspector,&SimplePhysicsInspector::length>;
using SimplePhysicsLengthCommandBase=ApplyInspectorPropCommand<SimplePhysicsInspector,SimplePhysicsLengthAccess,"SimplePhysics","float">;
using SimplePhysicsLengthCommand = DefApply<"SimplePhysicsLength",
    SimplePhysicsInspector, SimplePhysicsLengthAccess, "SimplePhysics", "float">::Apply;
using SimplePhysicsFrequencyAccess=InspectorAttributeAccess<SimplePhysicsInspector,&SimplePhysicsInspector::frequency>;
using SimplePhysicsFrequencyCommandBase=ApplyInspectorPropCommand<SimplePhysicsInspector,SimplePhysicsFrequencyAccess,"SimplePhysics","float">;
using SimplePhysicsFrequencyCommand = DefApply<"SimplePhysicsFrequency",
    SimplePhysicsInspector, SimplePhysicsFrequencyAccess, "SimplePhysics", "float">::Apply;
using SimplePhysicsAngleDampingAccess=InspectorAttributeAccess<SimplePhysicsInspector,&SimplePhysicsInspector::angleDamping>;
using SimplePhysicsAngleDampingCommandBase=ApplyInspectorPropCommand<SimplePhysicsInspector,SimplePhysicsAngleDampingAccess,"SimplePhysics","float">;
using SimplePhysicsAngleDampingCommand = DefApply<"SimplePhysicsAngleDamping",
    SimplePhysicsInspector, SimplePhysicsAngleDampingAccess, "SimplePhysics", "float">::Apply;
using SimplePhysicsLengthDampingAccess=InspectorAttributeAccess<SimplePhysicsInspector,&SimplePhysicsInspector::lengthDamping>;
using SimplePhysicsLengthDampingCommandBase=ApplyInspectorPropCommand<SimplePhysicsInspector,SimplePhysicsLengthDampingAccess,"SimplePhysics","float">;
using SimplePhysicsLengthDampingCommand = DefApply<"SimplePhysicsLengthDamping",
    SimplePhysicsInspector, SimplePhysicsLengthDampingAccess, "SimplePhysics", "float">::Apply;
using SimplePhysicsOutputScaleXAccess=InspectorAttributeAccess<SimplePhysicsInspector,&SimplePhysicsInspector::outputScaleX>;
using SimplePhysicsOutputScaleXCommandBase=ApplyInspectorPropCommand<SimplePhysicsInspector,SimplePhysicsOutputScaleXAccess,"SimplePhysics","float">;
using SimplePhysicsOutputScaleXCommand = DefApply<"SimplePhysicsOutputScaleX",
    SimplePhysicsInspector, SimplePhysicsOutputScaleXAccess, "SimplePhysics", "float">::Apply;
using SimplePhysicsOutputScaleYAccess=InspectorAttributeAccess<SimplePhysicsInspector,&SimplePhysicsInspector::outputScaleY>;
using SimplePhysicsOutputScaleYCommandBase=ApplyInspectorPropCommand<SimplePhysicsInspector,SimplePhysicsOutputScaleYAccess,"SimplePhysics","float">;
using SimplePhysicsOutputScaleYCommand = DefApply<"SimplePhysicsOutputScaleY",
    SimplePhysicsInspector, SimplePhysicsOutputScaleYAccess, "SimplePhysics", "float">::Apply;
void ngInitSimplePhysicsInspectorCommands();
}
namespace nijigenerate::commands {
template<> struct CommandTraits<inspector::apply_node::SimplePhysicsModelTypeCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::SimplePhysicsModelTypeCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::SimplePhysicsMapModeCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::SimplePhysicsMapModeCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::SimplePhysicsLocalOnlyCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::SimplePhysicsLocalOnlyCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::ToggleSimplePhysicsLocalOnlyCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::ToggleSimplePhysicsLocalOnlyCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::SimplePhysicsGravityCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::SimplePhysicsGravityCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::SimplePhysicsLengthCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::SimplePhysicsLengthCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::SimplePhysicsFrequencyCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::SimplePhysicsFrequencyCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::SimplePhysicsAngleDampingCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::SimplePhysicsAngleDampingCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::SimplePhysicsLengthDampingCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::SimplePhysicsLengthDampingCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::SimplePhysicsOutputScaleXCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::SimplePhysicsOutputScaleXCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::SimplePhysicsOutputScaleYCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::SimplePhysicsOutputScaleYCommandBase>;
};
}
