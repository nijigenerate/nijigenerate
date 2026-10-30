#include "nijigenerate/widgets/dialog.hpp"
#include "nijigenerate/widgets/detail/dialog_state.hpp"
#include "nijigenerate/widgets/button.hpp"
#include "nijigenerate/widgets/dummy.hpp"
#include "nijigenerate/widgets/label.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"

#include <texture.hpp>

namespace nijigenerate::widgets::dialog {
void ngRenderDialogs() {
    if (detail::entries.empty()) return;
    auto entryBacking = detail::entries;
    auto* entry = &entryBacking[0];
    bool popupOpen;
    {
        const auto title = entry->title;
        popupOpen = ImGui::IsPopupOpen(title.get());
    }
    if (!popupOpen) {
        const auto title = entry->title;
        ImGui::OpenPopup(title.get());
    }
    const auto flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_AlwaysAutoResize;
    const float positionX = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.x;
    const float halfWidth = compat::ngRequireObject(ImGui::GetMainViewport())->Size.x / 2;
    const float x = positionX + halfWidth;
    const float positionY = compat::ngRequireObject(ImGui::GetMainViewport())->Pos.y;
    const float halfHeight = compat::ngRequireObject(ImGui::GetMainViewport())->Size.y / 2;
    const ImVec2 position(x, positionY + halfHeight);
    ImGui::SetNextWindowPos(position, ImGuiCond_Appearing, ImVec2(.5f, .5f));
    bool modalVisible;
    {
        const auto title = entry->title;
        modalVisible = ImGui::BeginPopupModal(title.get(), nullptr, flags);
    }
    if (!modalVisible) return;

    const float imageScale = 112;
    const float endPadding = 4;
    ImGui::BeginGroup();
    if (ImGui::BeginChild("ErrorMainBoxLogo", ImVec2(imageScale, imageScale))) {
        const auto index = static_cast<std::size_t>(entry->level);
        if (index >= detail::adaTextures.size())
            throw compat::ngStaticError<compat::ArrayIndexError>(index, detail::adaTextures.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/dialog.d"), 79);
        const auto icon = detail::adaTextures.data()[index];
        ImGui::Image(static_cast<ImTextureID>(compat::ngRequireObject(icon)->getTextureId()), ImVec2(imageScale, imageScale),
                     ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0, 0, 0, 0));
    }
    ImGui::EndChild();
    ImGui::SameLine(0, 0);
    ImGui::PushTextWrapPos(512);
    ngText(compat::RetainedStringSlice(entry->text.retained()));
    ImGui::PopTextWrapPos();
    ImGui::SameLine(0, 0);
    ngDummy(ImVec2(endPadding, 1));
    ImGui::EndGroup();

    const auto available = ngAvailableSpace();
    const float height = 24;
    const float width = 80;
    const float totalWidth = width * entry->btncount;
    const float messageWidth = imageScale + ngMeasureString(
        compat::RetainedStringSlice(entry->text.retained())).x + endPadding;
    const float minimum = 256;
    if (messageWidth < minimum && totalWidth < minimum) {
        ImGui::Dummy(ImVec2(minimum - (totalWidth + 1), height));
        ImGui::SameLine(0, 0);
    } else if (available.x > totalWidth) {
        ImGui::Dummy(ImVec2(available.x - (totalWidth + 1), height));
        ImGui::SameLine(0, 0);
    }

    if ((entry->btns & DialogButtons::Cancel) == DialogButtons::Cancel) {
        const auto label = core::translation::ngTranslateCString("Cancel");
        if (ngButtonColored(label.data(), ImVec2(width, height))) {
            entry->selected = DialogButtons::Cancel;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 0);
    }
    if ((entry->btns & DialogButtons::OK) == DialogButtons::OK) {
        const auto label = core::translation::ngTranslateCString("OK");
        if (ngButtonColored(label.data(), ImVec2(width, height))) {
            entry->selected = DialogButtons::OK;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 0);
    }
    if ((entry->btns & DialogButtons::Yes) == DialogButtons::Yes) {
        const auto label = core::translation::ngTranslateCString("Yes");
        if (ngButtonColored(label.data(), ImVec2(width, height))) {
            entry->selected = DialogButtons::Yes;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 0);
    }
    if ((entry->btns & DialogButtons::No) == DialogButtons::No) {
        const auto label = core::translation::ngTranslateCString("No");
        if (ngButtonColored(label.data(), ImVec2(width, height))) {
            entry->selected = DialogButtons::No;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndPopup();
}
} // namespace nijigenerate::widgets::dialog
