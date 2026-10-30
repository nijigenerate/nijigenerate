#include "nijigenerate/widgets/statusbar_internal.hpp"
#include "nijigenerate/widgets/dummy.hpp"
#include "nijigenerate/widgets/label.hpp"
#include "nijigenerate/widgets/window.hpp"
#include "nijigenerate/core/tasks.hpp"
#include "nijigenerate/core/window_theme.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace nijigenerate::widgets {

void ngStatusbar() {
    constexpr auto flags = ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar;
    if (core::ngGetDarkMode()) {
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(.1f, .1f, .1f, 1.f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(.9f, .9f, .9f, 1.f));
    }
    if (ImGui::BeginViewportSideBar("##Statusbar", ImGui::GetMainViewport(),
            ImGuiDir_Down, 22.f, flags)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.f, 1.f, 1.f, .098f));
            const auto drawingItems = statusbar::detail::items();
            for (std::size_t i = 0; i < drawingItems.size(); ++i) {
                const auto item = drawingItems[i];
                if (i == statusbar::detail::itemIndex()) break;
                const float startX = ImGui::GetCursorPosX();
                const ImVec2 size = ngMeasureString(compat::RetainedStringSlice(item.action));
                ngText(compat::RetainedStringSlice(item.action));
                ImGui::SameLine(startX + (size.x - 8.f), 0.f);
                ngText(":");
                ImGui::SameLine(0.f, 4.f);
                ngText(compat::RetainedStringSlice(item.key));
                ImGui::Separator();
            }
            ImGui::PopStyleColor();

            if (core::tasks::ngTaskLength() > 0) {
                if (core::tasks::ngTaskGetProgress() >= 0.f) {
                    ImGui::ProgressBar(core::tasks::ngTaskGetProgress(), ImVec2(128.f, 0.f));
                }
                const auto taskStatus = core::tasks::ngTaskGetStatusSource();
                ngText(compat::RetainedStringSlice(taskStatus));
                if (!core::tasks::ngGetStatus().empty()) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }
            }
            ngText(core::tasks::ngGetStatus());
            ImGui::EndMenuBar();
        }
        ngEnd();
    }
    ImGui::PopStyleColor();
    statusbar::detail::itemIndex() = 0;
}

} // namespace nijigenerate::widgets
