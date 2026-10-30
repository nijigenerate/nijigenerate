#include "grid.hpp"
#include "remove_float.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/widgets/button.hpp"
#include "nijigenerate/widgets/category.hpp"
#include "nijigenerate/widgets/drag.hpp"
#include "nijigenerate/widgets/dummy.hpp"
#include "nijigenerate/widgets/label.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include <array>
#include <cstdint>

namespace nijigenerate::viewport::vertex::automesh::grid {
namespace {
template<std::size_t N> compat::RetainedString literal(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
}

void GridAutoMeshProcessor::configureLegacyUI() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    const auto editScale = [](compat::Slice<float>& scales) {
        std::int32_t deleteIndex = -1;
        if (ImGui::BeginChild("###AXIS_ADJ", ImVec2(0, 240))) {
            if (!scales.empty()) {
                const auto elements = scales;
                for (std::size_t i = 0; i < elements.size(); ++i) {
                    ImGui::SetNextItemWidth(80);
                    ImGui::PushID(static_cast<int>(i));
                    if (i >= scales.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                        i, scales.size(), literal("source/nijigenerate/viewport/vertex/automesh/grid.d"), 127);
                    widgets::ngDragFloat(compat::RetainedStringSlice(literal("adj_offset")), &scales[i],
                        0.01f, -1, 2, compat::RetainedStringSlice(literal("%.2f")), ImGuiSliderFlags_NoRoundToFormat);
                    ImGui::SameLine(0, 0);
                    if (i == scales.size() - 1) {
                        widgets::ngDummy(ImVec2(-52, 32));
                        ImGui::SameLine(0, 0);
                        if (widgets::ngButtonColored("\xee\xa4\xae", ImVec2(24, 24)))
                            deleteIndex = static_cast<std::int32_t>(i);
                        ImGui::SameLine(0, 0);
                        if (widgets::ngButtonColored("\xee\x85\x85", ImVec2(24, 24))) scales.append(1);
                    } else {
                        widgets::ngDummy(ImVec2(-28, 32));
                        ImGui::SameLine(0, 0);
                        if (widgets::ngButtonColored("\xee\xa4\xae", ImVec2(24, 24)))
                            deleteIndex = static_cast<std::int32_t>(i);
                    }
                    ImGui::PopID();
                }
            } else {
                widgets::ngDummy(ImVec2(-28, 24));
                ImGui::SameLine(0, 0);
                if (widgets::ngButtonColored("\xee\x85\x85", ImVec2(24, 24))) scales.append(1);
            }
        }
        ImGui::EndChild();
        if (deleteIndex != -1) {
            const auto index = static_cast<std::uint32_t>(deleteIndex);
            scales = ::nijigenerate::viewport::vertex::automesh::detail::removeFloat(scales, index);
        }
    };
    // Both source helpers use the same nonvirtual rebuild algorithm. Do not
    // dispatch through the public overridable post-write hook from Legacy UI.
    const auto divideAxes = [this] { rebuildScalesFromSegments(); };
    ImGui::PushID("CONFIGURE_OPTIONS");
    if (widgets::ngBeginCategory(core::translation::ngTranslateCString("Auto Segmentation").data())) {
        widgets::ngText(compat::RetainedStringSlice(core::translation::ngTranslateRetained(literal("Mask threshold"))));
        ImGui::Indent();
        ImGui::PushID("MASK_THRESHOLD");
        ImGui::SetNextItemWidth(64);
        if (widgets::ngDragFloat(compat::RetainedStringSlice(literal("mask_threshold")), &maskThreshold,
                1, 1, 200, compat::RetainedStringSlice(literal("%.2f")), ImGuiSliderFlags_NoRoundToFormat)) {
            maskThreshold = maskThreshold;
        }
        ImGui::PopID();
        ImGui::Unindent();
        struct Field { compat::RetainedString label; const char* pushed; compat::RetainedString id; float* value;
            float step, min, max; compat::RetainedString format; };
        const std::array<Field, 3> fields{{
            {literal("X Segments"), "XSEGMENTS", literal("x_segments"), &xSegments, 1, 2, 20, literal("%.0f")},
            {literal("Y Segments"), "YSEGMENTS", literal("y_segments"), &ySegments, 1, 2, 20, literal("%.0f")},
            {literal("Margin"), "MARGIN", literal("margin"), &margin, 0.1f, 0, 1, literal("%.2f")}}};
        for (const auto field : fields) {
            widgets::ngText(compat::RetainedStringSlice(core::translation::ngTranslateRetained(field.label)));
            ImGui::Indent();
            ImGui::PushID(field.pushed);
            ImGui::SetNextItemWidth(64);
            if (widgets::ngDragFloat(compat::RetainedStringSlice(field.id), field.value, field.step, field.min, field.max,
                    compat::RetainedStringSlice(field.format), ImGuiSliderFlags_NoRoundToFormat)) divideAxes();
            ImGui::PopID();
            ImGui::Unindent();
        }
    }
    widgets::ngEndCategory();
    if (widgets::ngBeginCategory(core::translation::ngTranslateCString("X Scale").data())) editScale(scaleX);
    widgets::ngEndCategory();
    if (widgets::ngBeginCategory(core::translation::ngTranslateCString("Y Scale").data())) editScale(scaleY);
    widgets::ngEndCategory();
    ImGui::PopID();
    ImGui::Separator();
    widgets::ngText(compat::RetainedStringSlice(core::translation::ngTranslateRetained(literal("Alpha Preview"))));
    ImGui::Indent();
    alphaPreviewWidget(_alphaPreview, ImVec2(192, 192));
    ImGui::Unindent();
}

} // namespace nijigenerate::viewport::vertex::automesh::grid
