#include "autosave.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/io/autosave.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <limits>

namespace nijigenerate::windows::autosave {
using core::translation::ngTranslateRetained;
using core::translation::ngTranslateCString;
void RestoreSaveWindow::initialize(compat::RetainedStringSlice path) {
    const auto receiver = weak_from_this().lock();
    Window::initialize(ngTranslateRetained(compat::RetainedString::fromStaticLiteral("Restore Autosave")));
    projectPath = path;
}
void RestoreSaveWindow::onBeginUpdate() {
    const auto receiver = weak_from_this().lock();
    // Retain the source's four getter evaluations and their left-to-right order.
    const float positionX = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const float halfWidth = compat::ngRequireObject(ImGui::GetMainViewport())->Size.x / 2;
    const float x = positionX + halfWidth;
    const float positionY = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const float halfHeight = compat::ngRequireObject(ImGui::GetMainViewport())->Size.y / 2;
    const float y = positionY + halfHeight;
    ImGui::SetNextWindowPos({x, y},
        ImGuiCond_Appearing, {.5f, .5f});
    ImGui::SetNextWindowSize({400, 128}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({400, 128},
        {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()});
    Window::onBeginUpdate();
}
void RestoreSaveWindow::onUpdate() {
    const auto receiver = weak_from_this().lock();
    const float available = widgets::ngAvailableSpace().x;
    if (ImGui::BeginChild("RestoreSaveMessage", {0, -28}, true)) {
        ImGui::PushTextWrapPos(available);
        widgets::ngText(compat::RetainedStringSlice{ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "nijigenerate closed unexpectedly while editing this file."))});
        widgets::ngText(compat::RetainedStringSlice{ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Restore data from a backup?"))});
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
    if (ImGui::BeginChild("RestoreSaveButtons", {0, 0}, false, ImGuiWindowFlags_NoScrollbar)) {
        widgets::ngDummy({-128, 0}); ImGui::SameLine(0, 0);
        if (widgets::ngButtonColored(ngTranslateCString("Discard").data(), {64, 24})) {
            project::incOpenProject(projectPath, compat::RetainedString::emptyLiteral());
            close();
        }
        ImGui::SameLine(0, 0);
        if (widgets::ngButtonColored(ngTranslateCString("Restore").data(), {64, 24})) {
            auto directory = io::autosave::getAutosaveDir(projectPath.slice(0,
                platform::ngAutosaveStripExtension(projectPath.view()).size()));
            auto entries = io::autosave::currentBackups(directory);
            const auto last = entries.size() - 1;
            if (last >= entries.size())
                throw compat::ngStaticError<compat::ArrayIndexError>(last, entries.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/autosave.d"), 65);
            project::incOpenProject(projectPath, entries[last].name());
            close();
        }
    }
    ImGui::EndChild();
}
}
