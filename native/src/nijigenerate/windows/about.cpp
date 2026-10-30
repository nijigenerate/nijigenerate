/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    Authors: Luna Nielsen
*/
#include "nijigenerate/windows/about.hpp"
#include "nijigenerate/windows/about_resources.hpp"
#include "nijigenerate/core/logo.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/widgets/dummy.hpp"
#include "nijigenerate/widgets/label.hpp"
#include "nijigenerate/widgets/button.hpp"
#include "nijigenerate/utils/link.hpp"
#include "nijigenerate/ver.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include <texture.hpp>
#include <timing.hpp>
#include <imgui_internal.h>
#include <cmath>
#include <limits>
#include <string_view>

namespace nijigenerate::windows::about {
void AboutWindow::initialize() {
    const auto receiver = weak_from_this().lock();
    Window::initialize(core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("About")));
    onlyOne = true;
    cfg.headingFormats[0] = {2, true};
    cfg.headingFormats[1] = {1.5f, false};
    cfg.headingFormats[2] = {1.2f, false};
    cfg.linkCallback = [](widgets::markdown::MarkdownLinkCallbackData data) {
        utils::link::ngOpenLink(data.link.view());
    };
    ada_float = {0, 0};
}
void AboutWindow::onBeginUpdate() {
    const auto receiver = weak_from_this().lock();
    ImGui::SetNextWindowSize({640, 512}, ImGuiCond_Appearing);
    const auto maximum = std::numeric_limits<float>::max();
    ImGui::SetNextWindowSizeConstraints({640, 512}, {maximum, maximum});
    Window::onBeginUpdate();
}
void AboutWindow::onUpdate() {
    const auto receiver = weak_from_this().lock();
    const auto position = ImGui::GetCursorScreenPos();
    const auto available = widgets::ngAvailableSpace();
    ImGui::SetCursorScreenPos({position.x + (available.x - (ADA_SIZE - ADA_SIZE_PARTIAL)),
        static_cast<float>(position.y + (available.y - (ADA_SIZE + 28)) +
                           std::sin(nicxlive::core::currentTime()) * 4)});
    ImGui::Image(static_cast<ImTextureID>(compat::ngRequireObject(core::ngGetAda())->getTextureId()), {ADA_SIZE, ADA_SIZE},
                 {0, 0}, {1, 1}, {1, 1, 1, .4f}, {0, 0, 0, 0});
    ImGui::SetCursorScreenPos(position);
    if (ImGui::BeginChild("##LogoArea", {0, 102})) {
        ImGui::Image(static_cast<ImTextureID>(compat::ngRequireObject(core::ngGetLogo())->getTextureId()), {64, 64},
                     {0, 0}, {1, 1}, {1, 1, 1, 1}, {0, 0, 0, 0});
        ImGui::SameLine(0, 8);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0, 8);
        if (ImGui::BeginChild("##LogoTextArea", {0, -28})) {
            widgets::ngText("nijigenerate");
            widgets::ngText(ver::INC_VERSION);
            ImGui::Separator();
            ImGui::TextColored({.5f, .5f, .5f, 1}, "nijilive v. %s", detail::runtimeVersion);
            ImGui::TextColored({.5f, .5f, .5f, 1}, "imgui v. %s", ImGui::GetVersion());
        }
        ImGui::EndChild();
        ImGui::Spacing();
        widgets::ngText(compat::RetainedStringSlice{core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Credits"))});
        ImGui::Separator();
    }
    ImGui::EndChild();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.176f, .447f, .698f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.313f, .521f, .737f, 1));
    if (ImGui::BeginChild("##CreditsArea", {0, -36})) {
        // The generated immutable bytes are the complete D string-import input.
        const compat::RetainedStringSlice contributors{compat::RetainedString::fromStaticView(std::string_view(
            reinterpret_cast<const char*>(detail::contributors), detail::contributorsSize))};
        widgets::markdown::ngMarkdown(contributors, cfg);
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::EndChild();
    if (ImGui::BeginChild("##ButtonArea", {0, 0})) {
        auto space = widgets::ngAvailableSpace();
        widgets::ngDummy({space.x * 3 / 4, space.y});
        ImGui::SameLine(0, 0);
        space = widgets::ngAvailableSpace();
        const float spacing = space.x / 1 - 8;
        if (widgets::ngButtonColored("GitHub", {8 + spacing, 0}))
            utils::link::ngOpenLink("https://github.com/nijigenerate/nijigenerate");
        ImGui::SameLine(0, 8);
    }
    ImGui::EndChild();
}
} // namespace nijigenerate::windows::about
