#include <nijigenerate/widgets/toggle.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <imgui_internal.h>
#include <algorithm>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::widgets {
namespace {
template<class Text> bool toggleSwitch(Text id, bool& value, ImVec2 size) {
    auto* window = ImGui::GetCurrentWindow();
    if (compat::ngRequireObject(window)->SkipItems) return false;
    auto* context = ImGui::GetCurrentContext();
    const ImGuiStyle style = compat::ngRequireObject(context)->Style;
    const float height = size.y > 0 ? size.y : ImGui::GetFrameHeight();
    const float width = size.x > 0 ? size.x : height * 1.8f;
    const ImVec2 finalSize(width, height);
    const auto retainedLabel = [&] {
        try {
            std::string label = "##";
            if (!id.empty()) label.append(id.data(), id.size());
            return compat::RetainedString(std::move(label));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    const compat::SourceCString labelCString{compat::RetainedStringSlice(retainedLabel)};
    compat::detail::ngSourceCStringPostconditions(retainedLabel.view(), labelCString.data());
    const bool toggled = ImGui::InvisibleButton(labelCString.data(), finalSize);
    if (toggled) value = !value;
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    const ImRect bounds(min, max);
    const float radius = (bounds.Max.y - bounds.Min.y) * .5f;
    const auto off = style.Colors[ImGuiCol_FrameBg];
    const auto on = style.Colors[ImGuiCol_Button];
    const auto hoverColor = style.Colors[ImGuiCol_ButtonHovered];
    const auto activeColor = style.Colors[ImGuiCol_ButtonActive];
    auto background = value ? on : off;
    if (hovered || active) background = value ? activeColor : hoverColor;
    const auto bg = ImGui::GetColorU32(background);
    const auto knob = ImGui::GetColorU32(style.Colors[ImGuiCol_WindowBg]);
    const auto border = ImGui::GetColorU32(style.Colors[ImGuiCol_Border]);
    auto* draw = ImGui::GetWindowDrawList();
    compat::ngRequireObject(draw)->AddRectFilled(bounds.Min, bounds.Max, bg, radius);
    draw->AddRect(bounds.Min, bounds.Max, border, radius, ImDrawFlags_RoundCornersAll, 1.0f);
    const float x = value ? bounds.Max.x - radius : bounds.Min.x + radius;
    const ImVec2 center(x, bounds.Min.y + radius);
    const float knobRadius = std::max(2.0f, radius - 2.0f);
    draw->AddCircleFilled(center, knobRadius, knob);
    draw->AddCircle(center, knobRadius, border, 0, 1.0f);
    return toggled;
}
} // namespace
bool ngToggleSwitch(compat::RetainedStringSlice id, bool& value, ImVec2 size) {
    return toggleSwitch(id, value, size);
}
bool ngToggleSwitch(std::string_view id, bool& value, ImVec2 size) { return toggleSwitch(id, value, size); }
}
