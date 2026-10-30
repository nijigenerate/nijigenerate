#include "nijigenerate/core/window.hpp"
#include "nijigenerate/core/window_state_internal.hpp"

namespace nijigenerate::core {

void ngSetDefaultLayout() {
    auto& state = detail::windowState();
    ImGui::DockBuilderRemoveNodeChildNodes(state.viewportDock);
    ImGuiID main = state.viewportDock;
    ImGuiID nodes = 0, inspector = 0, history = 0, parameters = 0;
    ImGuiID toolSettings = 0, loggerAndTextureSlots = 0, timeline = 0, animationList = 0;
    ImGuiID armedParameters = 0, resources = 0;
    animationList = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.10f, nullptr, &main);
    nodes = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.10f, nullptr, &main);
    inspector = ImGui::DockBuilderSplitNode(nodes, ImGuiDir_Down, 0.60f, nullptr, &nodes);
    toolSettings = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.10f, nullptr, &main);
    history = ImGui::DockBuilderSplitNode(toolSettings, ImGuiDir_Down, 0.50f, nullptr, &toolSettings);
    timeline = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.15f, nullptr, &main);
    parameters = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.15f, nullptr, &main);
    armedParameters = ImGui::DockBuilderSplitNode(parameters, ImGuiDir_Up, 0.3f, nullptr, &parameters);
    resources = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.20f, nullptr, &main);
    ImGui::DockBuilderDockWindow("###Nodes", nodes);
    ImGui::DockBuilderDockWindow("###Inspector", inspector);
    ImGui::DockBuilderDockWindow("###Tool Settings", toolSettings);
    ImGui::DockBuilderDockWindow("###AutoRig", toolSettings);
    ImGui::DockBuilderDockWindow("###History", history);
    ImGui::DockBuilderDockWindow("###Scene", history);
    ImGui::DockBuilderDockWindow("###Timeline", timeline);
    ImGui::DockBuilderDockWindow("###Animation List", animationList);
    ImGui::DockBuilderDockWindow("###Logger", timeline);
    ImGui::DockBuilderDockWindow("###Armed Parameters", armedParameters);
    ImGui::DockBuilderDockWindow("###Parameters", parameters);
    // The primary source leaves this local ID at its unsigned default zero.
    ImGui::DockBuilderDockWindow("###Texture Slots", loggerAndTextureSlots);
    ImGui::DockBuilderDockWindow("###Resources", resources);
    ImGui::DockBuilderFinish(state.viewportDock);
}

} // namespace nijigenerate::core
