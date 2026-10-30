#include "texviewer.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <utility>

namespace nijigenerate::windows::texviewer {

void TextureViewerWindow::initialize(std::shared_ptr<nicxlive::core::Texture> value) {
    const auto receiver = weak_from_this().lock();
    texture = value;
    base::Window::initialize(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Texture Viewer")));
}

void TextureViewerWindow::onBeginUpdate() {
    const auto receiver = weak_from_this().lock();
    ImGui::SetNextWindowSize(ImVec2(512, 512), ImGuiCond_FirstUseEver);
    base::Window::onBeginUpdate();
}

void TextureViewerWindow::onUpdate() {
    const auto receiver = weak_from_this().lock();
    ImGui::SliderFloat("Zoom", &zoom, 0.1f, 10.0f, "%.2f");
    if (ImGui::BeginChild("TextureViewerArea", ImVec2(0, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        // D evaluates arguments from left to right. The virtual ID lookup may
        // reenter user code, so it must finish before either dimension is read.
        const auto imageTexture = compat::ngRequireObject(texture);
        const auto imageId = static_cast<ImTextureID>(imageTexture->getTextureId());
        const auto width = compat::ngRequireObject(texture)->width();
        const auto imageWidth = width * zoom;
        const auto height = compat::ngRequireObject(texture)->height();
        const auto imageHeight = height * zoom;
        ImGui::Image(imageId, ImVec2(imageWidth, imageHeight),
            ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0, 0, 0, 0));
    }
    ImGui::EndChild();
}

} // namespace nijigenerate::windows::texviewer
