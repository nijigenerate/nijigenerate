#include "base.hpp"
#include "nijigenerate/widgets/button.hpp"
#include "nijigenerate/widgets/drag.hpp"
#include "nijigenerate/widgets/tooltip.hpp"

namespace nijigenerate::viewport::depth::tools::base {

bool DepthEditTool::drawOptionDrag(compat::RetainedStringSlice id, float* value, float adjustSpeed,
    float minValue, float maxValue, compat::RetainedStringSlice fmt, bool sameLineAfter) {
    const auto receiverLifetime = sourceOwner_.lock();
    const auto begin = id.data();
    const auto end = begin ? begin + id.size() : begin;
    ImGui::PushID(begin, end);
    ImGui::PushItemWidth(72.0f);
    const auto changed = widgets::ngDragFloat(id, value, adjustSpeed, minValue, maxValue,
        fmt, ImGuiSliderFlags_NoRoundToFormat);
    ImGui::PopItemWidth();
    ImGui::PopID();
    widgets::ngTooltip(id);
    if (sameLineAfter) ImGui::SameLine(0.0f, 4.0f);
    return changed;
}

bool DepthEditTool::drawToolButton(DepthToolMode activeMode) {
    const auto receiverLifetime = sourceOwner_.lock();
    const auto text = icon();
    const auto selected = activeMode == mode();
    const auto color = selected ? widgets::colorUndefined : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    if (widgets::ngButtonColored(text, ImVec2(0.0f, 0.0f), color)) return true;
    widgets::ngTooltip(compat::RetainedStringSlice(tooltip()));
    return false;
}

} // namespace nijigenerate::viewport::depth::tools::base
