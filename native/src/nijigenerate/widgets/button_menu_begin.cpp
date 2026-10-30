#include "button.hpp"
#include "button_menu_state.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <imgui_internal.h>
#include <algorithm>
#include <new>

namespace nijigenerate::widgets {

bool ngBeginDropdownMenuRetained(compat::RetainedString id, compat::RetainedString icon,
    ImVec2 minimum, ImVec2 maximum) {
    return ngBeginDropdownMenu(id.view(), icon.view(), minimum, maximum);
}

bool ngBeginDropdownMenu(std::string_view id, std::string_view icon, ImVec2 minimum, ImVec2 maximum) {
    auto* storage = ImGui::GetStateStorage();
    auto* window = ImGui::GetCurrentWindow();
    const auto* begin = id.data();
    const auto* end = begin ? begin + id.size() : begin;
    const auto itemId = ImGui::GetID(begin, end);

    ImGui::PushID(static_cast<int>(itemId));
    const auto stateId = ImGui::GetID("WAS_OPEN");
    auto* state = static_cast<detail::DropDownMenuData*>(
        compat::ngRequireObject(storage)->GetVoidPtr(stateId));
    if (!state) {
        // The original raw allocation reads an indeterminate wasOpen on first
        // use. The documented native extension initializes that state only.
        if (void* memory = ImGui::MemAlloc(sizeof(detail::DropDownMenuData)))
            state = new (memory) detail::DropDownMenuData;
        storage->SetVoidPtr(ImGui::GetID("WAS_OPEN"), state);
    }

    const bool pressed = ngDropdownButtonIcon("DROPDOWN_BTN", icon, {-1, -1},
        compat::ngRequireObject(state)->wasOpen);
    if (ImGui::IsPopupOpen("DROPDOWN_CONTENT") && pressed)
        ImGui::ClosePopupsOverWindow(window, true);
    else if (pressed)
        ImGui::OpenPopup("DROPDOWN_CONTENT", ImGuiPopupFlags_MouseButtonLeft | ImGuiPopupFlags_NoOpenOverItems);

    auto position = ImGui::GetCursorScreenPos();
    if (window) {
        // Preserve inmath's min(max(x, min), max), including authored endpoints.
        position.x = std::min(std::max(position.x, window->OuterRectClipped.Max.x),
            window->OuterRectClipped.Min.x - 192.0f);
    }
    ImGui::SetNextWindowSizeConstraints(minimum, maximum);
    ImGui::SetNextWindowPos({position.x, position.y + 4}, ImGuiCond_Always, {0, 0});
    state->wasOpen = ImGui::BeginPopup("DROPDOWN_CONTENT");
    if (!state->wasOpen) ImGui::PopID();
    else state->winSize = compat::ngRequireObject(ImGui::GetCurrentWindow())->Size;
    return state->wasOpen;
}

} // namespace nijigenerate::widgets
