#include "psddepthmap.hpp"
#include "detail/psddepthmap_state.hpp"
#include "detail/psddepthmap_helpers.hpp"
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/commands/depth/psd_dialog_commands.hpp>
#include <nijigenerate/commands/puppet/edit_commands.hpp>
#include <algorithm>
#include <limits>
namespace nijigenerate::windows::psddepthmap {
using namespace commands::depth::map;
namespace {
void showError(core::translation::TranslatedCString title, compat::RetainedString message) {
    using namespace widgets::dialog;
    ngDialog(DialogTag::retained(title.pointer, title.owner), DialogText(std::move(message)));
}
}
bool PSDDepthMapWindow::apply() {
    const auto keepAlive = weak_from_this().lock();
    if (previewDirty) rebuildPreview();
    if (previewCompositionPending()) {
        lastApplyErrorMessage = detail::helpers::tr("GPU preview composition is still running.");
        return false;
    }
    // Apply consumes preview.grids directly. Recompose only the grids named by
    // edited layers, using the same target composer as the original path.
    if (!compose3DAdjustChangesForApply()) {
        const auto title = core::translation::ngTranslateCString("Error");
        showError(title, errorMessage);
        return false;
    }
    if (errorMessage.size()) {
        const auto title = core::translation::ngTranslateCString("Error");
        showError(title, errorMessage);
        return false;
    }
    compat::RetainedString validationError;
    if (!ngCanApplyPsdDepthImportResult(composedPreview, validationError)) {
        lastApplyErrorMessage = validationError;
        const auto title = core::translation::ngTranslateCString("Error");
        showError(title, validationError);
        return false;
    }
    closeDialogActionScope();
    auto result = ngApplyPsdDepthImportResult(composedPreview);
    if (!compat::ngRequireObject(result)->succeeded) {
        if (dialogDisplayed) ensureDialogActionScope();
        lastApplyErrorMessage = result->message;
        const auto title = core::translation::ngTranslateCString("Error");
        showError(title, result->message);
        return false;
    }
    lastApplyErrorMessage = nullptr;
    deactivateDialogCommandContext();
    close();
    return true;
}

void PSDDepthMapWindow::onBeginUpdate() {
    const auto keepAlive = weak_from_this().lock();
    dialogDisplayed = true;
    activateDialogCommandContext();
    ensureDialogActionScope();
    flags |= ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    const auto viewportX = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const auto viewportWidth = compat::ngRequireObject(ImGui::GetMainViewport())->Size.x;
    const auto viewportY = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const auto viewportHeight = compat::ngRequireObject(ImGui::GetMainViewport())->Size.y;
    ImVec2 wpos(viewportX + viewportWidth / 2, viewportY + viewportHeight / 2);
    ImVec2 uiSize = ImVec2(980, 760);
    ImGui::SetNextWindowPos(wpos, ImGuiCond_Appearing, ImVec2(0.5, 0.5));
    ImGui::SetNextWindowSize(uiSize, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(760, 520), ImVec2((std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)()));
    base::Window::onBeginUpdate();
}

void PSDDepthMapWindow::onUpdate() {
    const auto keepAlive = weak_from_this().lock();
    if (dialogDisplayed && detail::activePsdDepthMapWindow.get() == this) ensureDialogActionScope();
    if (previewDirty) rebuildPreview();
    else pollPendingPreviewGpuComposition();

    auto space = widgets::ngAvailableSpace();
    auto settingsWidth = std::min(340.0f, std::max(280.0f, space.x * 0.28f));
    if (ImGui::BeginTable("###PsdDepthDialogLayout", 2,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp, ImVec2(0, space.y))) {
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Settings").data(),
            ImGuiTableColumnFlags_WidthFixed, settingsWidth);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Preview").data(),
            ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        auto actionsHeight = 94.0f;
        auto settingsHeight = std::max(120.0f, space.y - actionsHeight);
        if (ImGui::BeginChild("###PsdDepthSettingsPane", ImVec2(0, settingsHeight), true)) {
            drawOptions();
        }
        ImGui::EndChild();
        auto actionWidth = widgets::ngAvailableSpace().x;
        auto historyButtonWidth = std::max(1.0f, (actionWidth - 4.0f) * 0.5f);
        ImGui::BeginDisabled(!core::actionstack::incActionCanUndo());
        if (widgets::ngButtonColored(core::translation::ngTranslateCString("Undo").data(),
            ImVec2(historyButtonWidth, 26))) {
            auto ctx = dialogCommandContext();
            commands::puppet::edit::ngRunEditCommand<commands::puppet::edit::EditCommand::Undo>(ctx);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!core::actionstack::incActionCanRedo());
        if (widgets::ngButtonColored(core::translation::ngTranslateCString("Redo").data(),
            ImVec2(historyButtonWidth, 26))) {
            auto ctx = dialogCommandContext();
            commands::puppet::edit::ngRunEditCommand<commands::puppet::edit::EditCommand::Redo>(ctx);
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(previewCompositionPending());
        if (widgets::ngButtonColored(core::translation::ngTranslateCString("Apply").data(), ImVec2(actionWidth, 26))) {
            auto ctx = dialogCommandContext();
            commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::ApplyPsdDepthDialog>(ctx);
        }
        ImGui::EndDisabled();
        if (widgets::ngButtonColored(core::translation::ngTranslateCString("Cancel").data(), ImVec2(actionWidth, 26))) {
            auto ctx = dialogCommandContext();
            commands::depth::psd_dialog::ngRunPsdDepthDialogCommand<commands::depth::psd_dialog::PsdDepthDialogCommand::CancelPsdDepthDialog>(ctx);
        }

        ImGui::TableNextColumn();
        if (ImGui::BeginChild("###PsdDepthWorkspace", ImVec2(0, space.y), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            if (ImGui::BeginTabBar("###PsdDepthMapImportTabs")) {
                auto threeDTabFlags = initial3DAdjustTabSelected ?
                    ImGuiTabItemFlags_None : ImGuiTabItemFlags_SetSelected;
                if (ImGui::BeginTabItem(core::translation::ngTranslateCString("3D Adjust").data(),
                    nullptr, threeDTabFlags)) {
                    initial3DAdjustTabSelected = true;
                    draw3DAdjustTab(std::max(120.0f, widgets::ngAvailableSpace().y));
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(core::translation::ngTranslateCString("Source / Mapping").data())) {
                    drawSourceMappingTab(std::max(120.0f, widgets::ngAvailableSpace().y));
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }
}

void PSDDepthMapWindow::onClose() {
    const auto keepAlive = weak_from_this().lock();
    dialogDisplayed = false;
    deactivateDialogCommandContext();
    closeDialogActionScope();
    cancelPendingPreviewGpuComposition();
    disposePreviewTextures();
}
void PSDDepthMapWindow::initialize(compat::RetainedString value) {
    path = std::move(value);
    base::Window::initialize(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("PSD Depth Map Import")));
}
}
