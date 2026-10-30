#pragma once
#include <nodes/part.hpp>
#include <imgui.h>

namespace nijigenerate::widgets {
void ngSetMaskDragDropPayload(const nicxlive::core::nodes::MaskBinding& binding,
    ImGuiCond condition=ImGuiCond_Always);
const nicxlive::core::nodes::MaskBinding* ngMaskFromDragDropPayload(const ImGuiPayload* payload);
}
