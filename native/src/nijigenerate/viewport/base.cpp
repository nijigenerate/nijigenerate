#include "base.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <animation/animation.hpp>

namespace nijigenerate::viewport::base {

void Viewport::draw(std::shared_ptr<nicxlive::core::math::Camera>) {}
void Viewport::drawTools() {}
void Viewport::drawOptions() {}
void Viewport::drawConfirmBar() {}
void Viewport::drawOverlay(ImDrawList*, ImVec2, ImRect) {}
std::uint32_t Viewport::asyncDerivedUpdateViewportChannel() { return 0; }
void Viewport::update(ImGuiIO*, std::shared_ptr<nicxlive::core::math::Camera>) {}
void Viewport::updatePresentation(ImGuiIO*, std::shared_ptr<nicxlive::core::math::Camera>) {}
void Viewport::withdraw() {}
void Viewport::present() {}
void Viewport::menu() {}
void Viewport::menuOpening() {}
void Viewport::toolSettings() {}
bool Viewport::hasMenu() { return false; }
bool Viewport::alwaysUpdate() { return alwaysUpdateMode; }
void Viewport::alwaysUpdate(bool value) { alwaysUpdateMode = value; }
void Viewport::toolbar() {}
void Viewport::selectionChanged(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>) {}
void Viewport::animationChanged(nicxlive::core::Animation) {}
void Viewport::armedParameterChanged(std::shared_ptr<nicxlive::core::param::Parameter>) {}

} // namespace nijigenerate::viewport::base
