#include "imgexport.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/io/imageexport.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/category.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <runtime_state.hpp>
#include <cmath>
#include <limits>
#include <cstdint>
#include <utility>
#include <new>
#include <stdexcept>
#include <source_location>

namespace nijigenerate::windows::imgexport {
namespace {
std::int32_t dimension(float value) {
    if (!std::isfinite(value) || value < -2147483648.0f || value >= 2147483648.0f)
        return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value);
}
}
void ImageExportWindow::initialize(compat::RetainedStringSlice file) {
    const auto receiver = weak_from_this().lock();
    base::Window::initialize(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Export Image...")));
    outFile = file;
    // Preserve the source's separate receiver and root-argument evaluations.
    const auto queryPuppet = project::incActivePuppet();
    const auto traversalPuppet = compat::ngRequireObject(project::incActivePuppet());
    const auto traversalRoot = traversalPuppet->root;
    cameras = compat::ngRequireObject(queryPuppet)->findNodesType<ext::nodes::ExCamera>(traversalRoot);
    if (cameras.empty()) {
        using namespace widgets::dialog;
        const auto title = core::translation::ngTranslateCString("Error");
        const auto titleTag = title.owner.use_count() ? DialogTag::retained(title.data(), title.owner)
            : DialogTag::borrowed(title.data());
        const auto message = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "No cameras to export from in Scene, please add a Camera."));
        const auto messageOwner = message.retentionOwner();
        const auto body = messageOwner.use_count() ? DialogText::retained(message.view(), messageOwner)
            : DialogText::borrowed(message.view());
        ngDialog(DialogTag::borrowed("NO_CAMERA"), titleTag, body);
        close();
        return;
    }
    cameraPuppetOwner_ = traversalPuppet;
    selectedCamera = cameras[0];
}
void ImageExportWindow::export_() {
    const auto receiver = weak_from_this().lock();
    using namespace nicxlive::core;
    const auto cameraNode = compat::ngRequireObject(selectedCamera);
    const auto camera = cameraNode->getCamera();
    // getCamera may reenter and change the selected field. Evaluate it again
    // for the source's next call and copy the returned viewport value.
    const auto viewportNode = compat::ngRequireObject(selectedCamera);
    const auto viewport = viewportNode->getViewport();
    int oldWidth = 0, oldHeight = 0;
    inGetViewport(oldWidth, oldHeight);
    const auto oldCamera = inGetCameraHandle();
    const float initial = std::numeric_limits<float>::quiet_NaN();
    float oldRed = initial, oldGreen = initial, oldBlue = initial, oldAlpha = initial;
    inSetCamera(camera);
    inSetViewport(dimension(viewport.x), dimension(viewport.y));
    if (transparency) {
        inGetClearColor(oldRed, oldGreen, oldBlue, oldAlpha);
        inSetClearColor(0, 0, 0, 0);
    }
    inBeginScene();
    const auto drawingPuppet = compat::ngRequireObject(project::incActivePuppet());
    drawingPuppet->draw();
    inEndScene();
    if (postprocessing) inPostProcessScene();
    const auto dataLength = inViewportDataLength();
    common::SharedArray<std::uint8_t> data;
    try { data = common::SharedArray<std::uint8_t>(dataLength, 0); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    inDumpViewport(data);
    const auto exportFile = outFile;
    const auto exportData = compat::Slice<std::uint8_t>::retainedView(
        data.retentionOwner(), data.data(), data.size());
    const auto exportWidth = dimension(viewport.x);
    const auto exportHeight = dimension(viewport.y);
    io::imageexport::incExportImage(exportFile, exportData, exportWidth, exportHeight);
    // The authored implementation restores state only after a successful export.
    if (transparency) inSetClearColor(oldRed, oldGreen, oldBlue, oldAlpha);
    inSetViewport(oldWidth, oldHeight);
    inSetCamera(oldCamera);
}
void ImageExportWindow::onBeginUpdate() {
    const auto receiver = weak_from_this().lock();
    flags |= ImGuiWindowFlags_NoSavedSettings;
    const float positionX = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const float halfWidth = compat::ngRequireObject(ImGui::GetMainViewport())->Size.x / 2;
    const float x = positionX + halfWidth;
    const float positionY = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const float halfHeight = compat::ngRequireObject(ImGui::GetMainViewport())->Size.y / 2;
    const ImVec2 position(x, positionY + halfHeight);
    const ImVec2 size(512, 256);
    ImGui::SetNextWindowPos(position, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(size,
        ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
    base::Window::onBeginUpdate();
}
void ImageExportWindow::onUpdate() {
    const auto receiver = weak_from_this().lock();
    using core::translation::ngTranslateRetained;
    using core::translation::ngTranslateCString;
    using namespace widgets;
    if (ImGui::BeginChild("ExportContent", ImVec2(0, -28), ImGuiChildFlags_Borders)) {
        ngText(compat::RetainedStringSlice{ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Export Settings"))});
        ImGui::Spacing();
        if (ngBeginCategory(ngTranslateCString("Camera").data())) {
            const auto previewCamera = compat::ngRequireObject(selectedCamera);
            const auto previewName = previewCamera->getName();
            const compat::SourceCString previewLabel{
                compat::RetainedStringSlice{compat::RetainedString(previewName)}};
            compat::detail::ngSourceCStringPostconditions(previewName.view(), previewLabel.data());
            if (ImGui::BeginCombo("###CAMERA", previewLabel.data())) {
                auto currentCameras = cameras;
                for (auto& camera : currentCameras) {
                    const auto itemCamera = compat::ngRequireObject(camera);
                    const auto itemLabel = itemCamera->cName();
                    if (ImGui::MenuItem(itemLabel)) selectedCamera = camera;
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();
            ngCheckbox(ngTranslateCString("Allow Transparency").data(), &transparency);
            ngCheckbox(ngTranslateCString("Use Post Processing").data(), &postprocessing);
        }
        ngEndCategory();
    }
    ImGui::EndChild();
    if (ImGui::BeginChild("ExportButtons", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
        ngDummy(ImVec2(-64, 0));
        ImGui::SameLine(0, 0);
        if (ngButtonColored(ngTranslateCString("Save").data(), ImVec2(64, 24))) {
            export_();
            close();
        }
    }
    ImGui::EndChild();
}
void ImageExportWindow::onClose() {
    const auto receiver = weak_from_this().lock();
    core::ngResetClearColor();
}
}
