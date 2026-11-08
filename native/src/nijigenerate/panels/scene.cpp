#include "scene.hpp"
#include <nijigenerate/backend/vulkan/backend.hpp>
#include <nijigenerate/core/translation.hpp>
namespace nijigenerate::panels::scene {
ScenePanel::ScenePanel()
    : Panel(compat::RetainedString::fromStaticLiteral("Scene"),
        core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Scene")), false) {}
void ScenePanel::onUpdate() {
    auto& light = backend::vulkan::ngSceneAmbientLight;
    const auto label = core::translation::ngTranslateCString("Ambient Light");
    ImGui::ColorEdit3(label.data(), &light.x,
        ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
}
void ngInitializeScenePanelModule() { ngInitializePanelModule<ScenePanel>(); }
}
