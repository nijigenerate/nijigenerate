#pragma once
#include "apply_node_commands.hpp"
#include "apply_node_table.hpp"
#include <nijigenerate/panels/inspector/part.hpp>
namespace nijigenerate::commands::inspector::apply_node {
using PartInspector=panels::inspector::part::NodeInspector<panels::inspector::ModelEditSubMode::Layout,
    nicxlive::core::nodes::Part>;
using PartTintAccess=InspectorAttributeAccess<PartInspector,&PartInspector::tint>;
using PartTintCommandBase = ApplyInspectorPropCommand<PartInspector, PartTintAccess,
    "Part", "Vector!(float, 3)">;
using PartTintCommand = DefApply<"PartTint",
    PartInspector, PartTintAccess, "Part", "Vector!(float, 3)">::Apply;
using PartScreenTintAccess=InspectorAttributeAccess<PartInspector,&PartInspector::screenTint>;
using PartScreenTintCommandBase = ApplyInspectorPropCommand<PartInspector, PartScreenTintAccess,
    "Part", "Vector!(float, 3)">;
using PartScreenTintCommand = DefApply<"PartScreenTint",
    PartInspector, PartScreenTintAccess, "Part", "Vector!(float, 3)">::Apply;
using PartEmissionStrengthAccess=InspectorAttributeAccess<PartInspector,&PartInspector::emissionStrength>;
using PartEmissionStrengthCommandBase=ApplyInspectorPropCommand<PartInspector,PartEmissionStrengthAccess,"Part","float">;
using PartEmissionStrengthCommand = DefApply<"PartEmissionStrength",
    PartInspector, PartEmissionStrengthAccess, "Part", "float">::Apply;
using PartBlendingModeAccess=InspectorAttributeAccess<PartInspector,&PartInspector::blendingMode>;
using PartBlendingModeCommandBase=ApplyInspectorPropCommand<PartInspector,PartBlendingModeAccess,"Part","BlendMode">;
using PartBlendingModeCommand = DefApply<"PartBlendingMode",
    PartInspector, PartBlendingModeAccess, "Part", "BlendMode">::Apply;
using PartOpacityAccess=InspectorAttributeAccess<PartInspector,&PartInspector::opacity>;
using PartOpacityCommandBase=ApplyInspectorPropCommand<PartInspector,PartOpacityAccess,"Part","float">;
using PartOpacityCommand = DefApply<"PartOpacity",
    PartInspector, PartOpacityAccess, "Part", "float">::Apply;
using PartMaskAlphaThresholdAccess=InspectorAttributeAccess<PartInspector,&PartInspector::maskAlphaThreshold>;
using PartMaskAlphaThresholdCommandBase=ApplyInspectorPropCommand<PartInspector,PartMaskAlphaThresholdAccess,"Part","float">;
using PartMaskAlphaThresholdCommand = DefApply<"PartMaskAlphaThreshold",
    PartInspector, PartMaskAlphaThresholdAccess, "Part", "float">::Apply;
using PartAutoResizedMeshAccess=InspectorAttributeAccess<PartInspector,&PartInspector::autoResizedMesh>;
using PartAutoResizedMeshCommandBase=ApplyInspectorPropCommand<PartInspector,PartAutoResizedMeshAccess,"Part","bool">;
using PartAutoResizedMeshCommand = DefApply<"PartAutoResizedMesh",
    PartInspector, PartAutoResizedMeshAccess, "Part", "bool">::Apply;
using TogglePartAutoResizedMeshCommandBase=ToggleInspectorPropCommand<PartInspector,PartAutoResizedMeshAccess,"Part">;
using TogglePartAutoResizedMeshCommand = DefApply<"PartAutoResizedMesh",
    PartInspector, PartAutoResizedMeshAccess, "Part", "bool">::Toggle;
void ngInitPartInspectorCommands();
}
namespace nijigenerate::commands {
template<> struct CommandTraits<inspector::apply_node::PartTintCommand> { static constexpr auto attributes=std::tuple{}; using BaseClasses=CommandTypeList<inspector::apply_node::PartTintCommandBase>; };
template<> struct CommandTraits<inspector::apply_node::PartScreenTintCommand> { static constexpr auto attributes=std::tuple{}; using BaseClasses=CommandTypeList<inspector::apply_node::PartScreenTintCommandBase>; };
template<> struct CommandTraits<inspector::apply_node::PartEmissionStrengthCommand> { static constexpr auto attributes=std::tuple{}; using BaseClasses=CommandTypeList<inspector::apply_node::PartEmissionStrengthCommandBase>; };
template<> struct CommandTraits<inspector::apply_node::PartBlendingModeCommand> { static constexpr auto attributes=std::tuple{}; using BaseClasses=CommandTypeList<inspector::apply_node::PartBlendingModeCommandBase>; };
template<> struct CommandTraits<inspector::apply_node::PartOpacityCommand> { static constexpr auto attributes=std::tuple{}; using BaseClasses=CommandTypeList<inspector::apply_node::PartOpacityCommandBase>; };
template<> struct CommandTraits<inspector::apply_node::PartMaskAlphaThresholdCommand> { static constexpr auto attributes=std::tuple{}; using BaseClasses=CommandTypeList<inspector::apply_node::PartMaskAlphaThresholdCommandBase>; };
template<> struct CommandTraits<inspector::apply_node::PartAutoResizedMeshCommand> { static constexpr auto attributes=std::tuple{}; using BaseClasses=CommandTypeList<inspector::apply_node::PartAutoResizedMeshCommandBase>; };
template<> struct CommandTraits<inspector::apply_node::TogglePartAutoResizedMeshCommand> { static constexpr auto attributes=std::tuple{}; using BaseClasses=CommandTypeList<inspector::apply_node::TogglePartAutoResizedMeshCommandBase>; };
}
