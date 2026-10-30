#pragma once
#include "apply_node_commands.hpp"
#include "apply_node_table.hpp"
#include <nijigenerate/panels/inspector/meshgroup.hpp>
namespace nijigenerate::commands::inspector::apply_node {
using MeshGroupInspector = panels::inspector::meshgroup::NodeInspector<
    panels::inspector::ModelEditSubMode::Layout, nicxlive::core::nodes::MeshGroup>;
using MeshGroupDynamicAccess = InspectorAttributeAccess<MeshGroupInspector,&MeshGroupInspector::dynamic>;
using MeshGroupTranslateChildrenAccess = InspectorAttributeAccess<MeshGroupInspector,&MeshGroupInspector::translateChildren>;
using MeshGroupDynamicCommandBase = ApplyInspectorPropCommand<MeshGroupInspector,MeshGroupDynamicAccess,"MeshGroup","bool">;
using MeshGroupTranslateChildrenCommandBase = ApplyInspectorPropCommand<MeshGroupInspector,MeshGroupTranslateChildrenAccess,"MeshGroup","bool">;
using MeshGroupDynamicCommand = DefApply<"MeshGroupDynamic",
    MeshGroupInspector, MeshGroupDynamicAccess, "MeshGroup", "bool">::Apply;
using MeshGroupTranslateChildrenCommand = DefApply<"MeshGroupTranslateChildren",
    MeshGroupInspector, MeshGroupTranslateChildrenAccess, "MeshGroup", "bool">::Apply;
void ngInitMeshGroupPropertyCommands();
using ToggleMeshGroupDynamicCommandBase = ToggleInspectorPropCommand<MeshGroupInspector,MeshGroupDynamicAccess,"MeshGroup">;
using ToggleMeshGroupTranslateChildrenCommandBase = ToggleInspectorPropCommand<MeshGroupInspector,MeshGroupTranslateChildrenAccess,"MeshGroup">;
using ToggleMeshGroupDynamicCommand = DefApply<"MeshGroupDynamic",
    MeshGroupInspector, MeshGroupDynamicAccess, "MeshGroup", "bool">::Toggle;
using ToggleMeshGroupTranslateChildrenCommand = DefApply<"MeshGroupTranslateChildren",
    MeshGroupInspector, MeshGroupTranslateChildrenAccess, "MeshGroup", "bool">::Toggle;
}
namespace nijigenerate::commands {
template<> struct CommandTraits<inspector::apply_node::ToggleMeshGroupDynamicCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::ToggleMeshGroupDynamicCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::ToggleMeshGroupTranslateChildrenCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::ToggleMeshGroupTranslateChildrenCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::MeshGroupDynamicCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::MeshGroupDynamicCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::MeshGroupTranslateChildrenCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::MeshGroupTranslateChildrenCommandBase>;
};
}

