/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
#include "project.hpp"
#include <imgui.h>
#include <imgui_internal.h>

namespace nijigenerate::project {

void incClearImguiData() {
    auto* context = ImGui::GetCurrentContext();
    if (context) {
        const auto count = context->Windows.Size;
        auto* const windows = context->Windows.Data;
        for (int index = 0; index < count; ++index) {
            auto* window = windows[index];
            if (window) window->StateStorage.Clear();
        }
    }
}

} // namespace nijigenerate::project
