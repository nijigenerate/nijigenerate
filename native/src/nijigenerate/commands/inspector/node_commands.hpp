#pragma once
#include "apply_node_commands.hpp"
#include "apply_node_table.hpp"
#include <nijigenerate/panels/inspector/node.hpp>
namespace nijigenerate::commands::inspector::apply_node {
using NodeApplyInspector=panels::inspector::node::NodeInspector<panels::inspector::ModelEditSubMode::Layout>;
using TranslationXAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::translationX>;
using TranslationXCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,TranslationXAccess,"Node","float">;
using TranslationXCommand = DefApply<"TranslationX",
    NodeApplyInspector, TranslationXAccess, "Node", "float">::Apply;
using TranslationYAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::translationY>;
using TranslationYCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,TranslationYAccess,"Node","float">;
using TranslationYCommand = DefApply<"TranslationY",
    NodeApplyInspector, TranslationYAccess, "Node", "float">::Apply;
using TranslationZAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::translationZ>;
using TranslationZCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,TranslationZAccess,"Node","float">;
using TranslationZCommand = DefApply<"TranslationZ",
    NodeApplyInspector, TranslationZAccess, "Node", "float">::Apply;
using RotationXAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::rotationX>;
using RotationXCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,RotationXAccess,"Node","float">;
using RotationXCommand = DefApply<"RotationX",
    NodeApplyInspector, RotationXAccess, "Node", "float">::Apply;
using RotationYAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::rotationY>;
using RotationYCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,RotationYAccess,"Node","float">;
using RotationYCommand = DefApply<"RotationY",
    NodeApplyInspector, RotationYAccess, "Node", "float">::Apply;
using RotationZAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::rotationZ>;
using RotationZCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,RotationZAccess,"Node","float">;
using RotationZCommand = DefApply<"RotationZ",
    NodeApplyInspector, RotationZAccess, "Node", "float">::Apply;
using ScaleXAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::scaleX>;
using ScaleXCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,ScaleXAccess,"Node","float">;
using ScaleXCommand = DefApply<"ScaleX",
    NodeApplyInspector, ScaleXAccess, "Node", "float">::Apply;
using ScaleYAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::scaleY>;
using ScaleYCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,ScaleYAccess,"Node","float">;
using ScaleYCommand = DefApply<"ScaleY",
    NodeApplyInspector, ScaleYAccess, "Node", "float">::Apply;
using PixelSnapAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::pixelSnap>;
using PixelSnapCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,PixelSnapAccess,"Node","bool">;
using PixelSnapCommand = DefApply<"PixelSnap",
    NodeApplyInspector, PixelSnapAccess, "Node", "bool">::Apply;
using TogglePixelSnapCommandBase=ToggleInspectorPropCommand<NodeApplyInspector,PixelSnapAccess,"Node">;
using TogglePixelSnapCommand = DefApply<"PixelSnap",
    NodeApplyInspector, PixelSnapAccess, "Node", "bool">::Toggle;
using ZSortAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::zSort>;
using ZSortCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,ZSortAccess,"Node","float">;
using ZSortCommand = DefApply<"ZSort",
    NodeApplyInspector, ZSortAccess, "Node", "float">::Apply;
using LockToRootAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::lockToRoot>;
using LockToRootCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,LockToRootAccess,"Node","bool">;
using LockToRootCommand = DefApply<"LockToRoot",
    NodeApplyInspector, LockToRootAccess, "Node", "bool">::Apply;
using ToggleLockToRootCommandBase=ToggleInspectorPropCommand<NodeApplyInspector,LockToRootAccess,"Node">;
using ToggleLockToRootCommand = DefApply<"LockToRoot",
    NodeApplyInspector, LockToRootAccess, "Node", "bool">::Toggle;
using PinToMeshAccess=InspectorAttributeAccess<NodeApplyInspector,&NodeApplyInspector::pinToMesh>;
using PinToMeshCommandBase=ApplyInspectorPropCommand<NodeApplyInspector,PinToMeshAccess,"Node","bool">;
using PinToMeshCommand = DefApply<"PinToMesh",
    NodeApplyInspector, PinToMeshAccess, "Node", "bool">::Apply;
using TogglePinToMeshCommandBase=ToggleInspectorPropCommand<NodeApplyInspector,PinToMeshAccess,"Node">;
using TogglePinToMeshCommand = DefApply<"PinToMesh",
    NodeApplyInspector, PinToMeshAccess, "Node", "bool">::Toggle;
void ngInitNodeInspectorCommands();
}
namespace nijigenerate::commands {
template<> struct CommandTraits<inspector::apply_node::TranslationXCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::TranslationXCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::TranslationYCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::TranslationYCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::TranslationZCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::TranslationZCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::RotationXCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::RotationXCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::RotationYCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::RotationYCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::RotationZCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::RotationZCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::ScaleXCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::ScaleXCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::ScaleYCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::ScaleYCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::PixelSnapCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::PixelSnapCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::TogglePixelSnapCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::TogglePixelSnapCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::ZSortCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::ZSortCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::LockToRootCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::LockToRootCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::ToggleLockToRootCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::ToggleLockToRootCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::PinToMeshCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::PinToMeshCommandBase>;
};
template<> struct CommandTraits<inspector::apply_node::TogglePinToMeshCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<inspector::apply_node::TogglePinToMeshCommandBase>;
};
}
