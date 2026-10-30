#include "base.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <animation/animation.hpp>
#include <utility>

namespace nijigenerate::viewport::base {

// Keep the actual receiver alive if its callback replaces the parent's child.
void DelegationViewport::draw(std::shared_ptr<nicxlive::core::math::Camera> camera) {
    if (auto child = _subView) child->draw(camera);
}
void DelegationViewport::drawTools() { if (auto child = _subView) child->drawTools(); }
void DelegationViewport::drawOptions() { if (auto child = _subView) child->drawOptions(); }
void DelegationViewport::drawConfirmBar() { if (auto child = _subView) child->drawConfirmBar(); }
std::uint32_t DelegationViewport::asyncDerivedUpdateViewportChannel() {
    if (auto child = _subView) return child->asyncDerivedUpdateViewportChannel();
    return 0;
}
void DelegationViewport::drawOverlay(ImDrawList* drawList, ImVec2 origin, ImRect rect) {
    if (auto child = _subView) child->drawOverlay(drawList, origin, rect);
}
void DelegationViewport::update(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera) {
    if (auto child = _subView) child->update(io, camera);
}
void DelegationViewport::updatePresentation(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera) {
    if (auto child = _subView) child->updatePresentation(io, camera);
}
void DelegationViewport::withdraw() { if (auto child = _subView) child->withdraw(); }
void DelegationViewport::present() { if (auto child = _subView) child->present(); }
void DelegationViewport::menu() { if (auto child = _subView) child->menu(); }
void DelegationViewport::menuOpening() { if (auto child = _subView) child->menuOpening(); }
void DelegationViewport::toolSettings() { if (auto child = _subView) child->toolSettings(); }
void DelegationViewport::toolbar() { if (auto child = _subView) child->toolbar(); }
bool DelegationViewport::hasMenu() {
    if (auto child = _subView) return child->hasMenu();
    return false;
}
void DelegationViewport::selectionChanged(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> nodes) {
    if (auto child = _subView) child->selectionChanged(nodes);
}
void DelegationViewport::animationChanged(nicxlive::core::Animation animation) {
    if (auto child = _subView) child->animationChanged(animation);
}
void DelegationViewport::armedParameterChanged(std::shared_ptr<nicxlive::core::param::Parameter> parameter) {
    if (auto child = _subView) child->armedParameterChanged(parameter);
}
std::shared_ptr<Viewport> DelegationViewport::subView() { return _subView; }
void DelegationViewport::subView(std::shared_ptr<Viewport> value) { _subView = std::move(value); }

} // namespace nijigenerate::viewport::base
