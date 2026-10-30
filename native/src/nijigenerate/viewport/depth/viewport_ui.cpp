#include "viewport.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/widgets/button.hpp"
#include "nijigenerate/widgets/tooltip.hpp"

namespace nijigenerate::viewport::depth::viewport {

void DepthEditViewport::drawDepthOptions() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    if (widgets::ngButtonColored(core::translation::ngTranslateCString("\xEF\x80\x95 Snap").data(), ImVec2(0.0f, 0.0f),
        brush.snapToGrid ? widgets::colorUndefined : ImVec4(0.6f, 0.6f, 0.6f, 1.0f))) {
        brush.snapToGrid = !brush.snapToGrid;
    }
    const auto tooltip = brush.snapToGrid
        ? compat::RetainedString::fromStaticLiteral("Snap tool points to grid vertices")
        : compat::RetainedString::fromStaticLiteral("Use free tool points");
    widgets::ngTooltip(compat::RetainedStringSlice(core::translation::ngTranslateRetained(tooltip)));
    ImGui::PopStyleVar();
}

} // namespace nijigenerate::viewport::depth::viewport
