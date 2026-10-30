#pragma once
#include <imgui.h>
#include <imgui_internal.h>

// Adapted from Dear ImGui 1.91.9b DragScalarN/DragFloat2 and ColorPicker3.
// The MIT License (MIT)
// Copyright (c) 2014-2025 Omar Cornut
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

namespace nijigenerate::widgets {
// ColorEdit3's complete pinned edit/picker/options protocol, on the same live
// RGB fields throughout nested calls. No input snapshot or final copy-back.
bool ngColorEdit3Fields(const char* label, float& red, float& green, float& blue,
    ImGuiColorEditFlags flags = 0);

// Each component is an actual live float object. Separate struct members are
// never presented as a float array, and edits are never copied back afterward.
inline bool ngDragFloat2Fields(const char* label, float& x, float& y,
    float speed = 1.0f, float minimum = 0.0f, float maximum = 0.0f,
    const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& context = *GImGui;
    float* const components[2]{&x, &y};
    bool changed = false;
    ImGui::BeginGroup();
    ImGui::PushID(label);
    ImGui::PushMultiItemsWidths(2, ImGui::CalcItemWidth());
    for (int i = 0; i < 2; ++i) {
        ImGui::PushID(i);
        if (i > 0) ImGui::SameLine(0, context.Style.ItemInnerSpacing.x);
        changed |= ImGui::DragScalar("", ImGuiDataType_Float, components[i], speed,
            &minimum, &maximum, format, flags);
        ImGui::PopID();
        ImGui::PopItemWidth();
    }
    ImGui::PopID();

    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    if (label != labelEnd) {
        ImGui::SameLine(0, context.Style.ItemInnerSpacing.x);
        ImGui::TextEx(label, labelEnd);
    }
    ImGui::EndGroup();
    return changed;
}

// Preserve ColorPicker3's own authored temporary and conditional write order.
// This is not an additional temporary around ColorPicker3: call ColorPicker4
// directly, just as the pinned three-component primitive does.
inline bool ngColorPicker3Fields(const char* label, float& red, float& green, float& blue,
    ImGuiColorEditFlags flags = 0) {
    float rgba[4]{red, green, blue, 1.0f};
    if (!ImGui::ColorPicker4(label, rgba, flags | ImGuiColorEditFlags_NoAlpha)) return false;
    red = rgba[0];
    green = rgba[1];
    blue = rgba[2];
    return true;
}
}
