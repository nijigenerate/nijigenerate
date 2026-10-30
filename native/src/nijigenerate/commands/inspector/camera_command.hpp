#pragma once
#include "apply_node_commands.hpp"
#include "apply_node_table.hpp"
#include <nijigenerate/panels/inspector/camera.hpp>

namespace nijigenerate::commands::inspector::apply_node {
using CameraInspector = panels::inspector::camera::NodeInspector<
    panels::inspector::ModelEditSubMode::Layout, ext::nodes::ExCamera>;
using CameraViewportAccess = InspectorAttributeAccess<CameraInspector, &CameraInspector::viewportOrigin>;
using CameraViewportCommandBase = ApplyInspectorPropCommand<CameraInspector, CameraViewportAccess,
    "ExCamera", "Vector!(float, 2)">;
using ViewportOriginCommand = DefApply<"ViewportOrigin",
    CameraInspector, CameraViewportAccess, "ExCamera", "Vector!(float, 2)">::Apply;
void ngInitCameraViewportCommand();
std::shared_ptr<CommandResult> ngRunCameraViewportCommand(std::shared_ptr<Context> context,
    nicxlive::core::math::Vec2 value);
}
namespace nijigenerate::commands {
template<> struct CommandTraits<inspector::apply_node::ViewportOriginCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::CameraViewportCommandBase>;
};
}
