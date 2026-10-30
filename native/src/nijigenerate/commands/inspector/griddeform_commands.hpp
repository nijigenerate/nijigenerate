#pragma once
#include "apply_node_commands.hpp"
#include "apply_node_table.hpp"
#include <nijigenerate/panels/inspector/griddeform.hpp>
namespace nijigenerate::commands::inspector::apply_node {
using GridInspector=panels::inspector::griddeform::NodeInspector<
    panels::inspector::ModelEditSubMode::Layout, nicxlive::core::nodes::GridDeformer>;
using GridDeformDynamicAccess=InspectorAttributeAccess<GridInspector,&GridInspector::dynamic>;
using GridDeformDynamicCommandBase=ApplyInspectorPropCommand<GridInspector,GridDeformDynamicAccess,"GridDeformer","bool">;
using ToggleGridDeformDynamicCommandBase=ToggleInspectorPropCommand<GridInspector,GridDeformDynamicAccess,"GridDeformer">;
using GridDeformDynamicCommand = DefApply<"GridDeformDynamic",
    GridInspector, GridDeformDynamicAccess, "GridDeformer", "bool">::Apply;
using ToggleGridDeformDynamicCommand = DefApply<"GridDeformDynamic",
    GridInspector, GridDeformDynamicAccess, "GridDeformer", "bool">::Toggle;
void ngInitGridInspectorCommands();
}
namespace nijigenerate::commands {
template<> struct CommandTraits<inspector::apply_node::GridDeformDynamicCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::GridDeformDynamicCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::ToggleGridDeformDynamicCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::ToggleGridDeformDynamicCommandBase>;
};
}
