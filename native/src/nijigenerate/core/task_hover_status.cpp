#include "tasks.hpp"
#include <imgui.h>
#include <utility>

namespace nijigenerate::core::tasks {

void ngHoverStatus(TaskString status) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_None)) ngSetStatus(std::move(status));
}

void ngHoverStatus(compat::RetainedStringSlice status) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_None)) ngSetStatus(status);
}

} // namespace nijigenerate::core::tasks
