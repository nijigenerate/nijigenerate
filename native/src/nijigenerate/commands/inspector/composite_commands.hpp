#pragma once
#include "apply_node_commands.hpp"
#include "apply_node_table.hpp"
#include <nijigenerate/panels/inspector/composite.hpp>
namespace nijigenerate::commands::inspector::apply_node {
using CompositeInspector=panels::inspector::composite::NodeInspector<
    panels::inspector::ModelEditSubMode::Layout,nicxlive::core::nodes::Composite>;
using CompositeTintAccess=InspectorAttributeAccess<CompositeInspector,&CompositeInspector::tint>;
using CompositeTintCommandBase = ApplyInspectorPropCommand<CompositeInspector, CompositeTintAccess,
    "Composite", "Vector!(float, 3)">;
using CompositeTintCommand = DefApply<"CompositeTint",
    CompositeInspector, CompositeTintAccess, "Composite", "Vector!(float, 3)">::Apply;
using CompositeScreenTintAccess=InspectorAttributeAccess<CompositeInspector,&CompositeInspector::screenTint>;
using CompositeScreenTintCommandBase = ApplyInspectorPropCommand<CompositeInspector, CompositeScreenTintAccess,
    "Composite", "Vector!(float, 3)">;
using CompositeScreenTintCommand = DefApply<"CompositeScreenTint",
    CompositeInspector, CompositeScreenTintAccess, "Composite", "Vector!(float, 3)">::Apply;
using CompositeBlendingModeAccess=InspectorAttributeAccess<CompositeInspector,&CompositeInspector::blendingMode>;
using CompositeBlendingModeCommandBase=ApplyInspectorPropCommand<CompositeInspector,CompositeBlendingModeAccess,"Composite","BlendMode">;
using CompositeBlendingModeCommand = DefApply<"CompositeBlendingMode",
    CompositeInspector, CompositeBlendingModeAccess, "Composite", "BlendMode">::Apply;
using CompositeOpacityAccess=InspectorAttributeAccess<CompositeInspector,&CompositeInspector::opacity>;
using CompositeOpacityCommandBase=ApplyInspectorPropCommand<CompositeInspector,CompositeOpacityAccess,"Composite","float">;
using CompositeOpacityCommand = DefApply<"CompositeOpacity",
    CompositeInspector, CompositeOpacityAccess, "Composite", "float">::Apply;
using CompositeThresholdAccess=InspectorAttributeAccess<CompositeInspector,&CompositeInspector::threshold>;
using CompositeThresholdCommandBase=ApplyInspectorPropCommand<CompositeInspector,CompositeThresholdAccess,"Composite","float">;
using CompositeThresholdCommand = DefApply<"CompositeThreshold",
    CompositeInspector, CompositeThresholdAccess, "Composite", "float">::Apply;
void ngInitCompositeInspectorCommands();
}
namespace nijigenerate::commands {
template<> struct CommandTraits<inspector::apply_node::CompositeTintCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::CompositeTintCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::CompositeScreenTintCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::CompositeScreenTintCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::CompositeBlendingModeCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::CompositeBlendingModeCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::CompositeOpacityCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::CompositeOpacityCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::CompositeThresholdCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::CompositeThresholdCommandBase>;
};
}
