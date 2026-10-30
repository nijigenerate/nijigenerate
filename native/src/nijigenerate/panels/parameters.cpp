#include "parameters.hpp"

#include <imgui.h>
#include <algorithm>
#include <utility>

namespace nijigenerate::panels::parameters::detail {
namespace {
double styleClamp(double value, double lower, double upper) {
    // inmath.math.clamp is min(max(value, lower), upper), with stable operands.
    const double raised = value < lower ? lower : value;
    return upper < raised ? upper : raised;
}
}

thread_local ParameterUiCommand pendingParameterUiCommand;
thread_local ParamDragDropData* dragDropData = nullptr;

bool queueParameterUiCommand(ParameterUiCommand action) {
    pendingParameterUiCommand = std::move(action);
    return true;
}

void pushColorScheme(::nicxlive::core::math::Vec3 color) {
    float hue, saturation, value;
    ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, hue, saturation, value);

    // inmath lerp uses a*(1-t)+b*t, with 1-t evaluated as float even for double T.
    float maxSaturation = static_cast<float>(1.0 * (1.0f - value) + 0.60 * value);
    float red, green, blue;
    ImGui::ColorConvertHSVtoRGB(hue,
        static_cast<float>(styleClamp(static_cast<double>(saturation) * (1.0f - value) +
            (saturation - 0.20) * value, 0.0, static_cast<double>(maxSaturation))),
        static_cast<float>(styleClamp(value - 0.15, 0.15, 0.90)), red, green, blue);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(red, green, blue, 1));

    maxSaturation = static_cast<float>(1.0 * (1.0f - value) + 0.60 * value);
    const double lower = styleClamp(saturation - 0.25, 0.0, static_cast<double>(maxSaturation));
    const double upper = styleClamp(saturation + 0.25, 0.0, static_cast<double>(maxSaturation));
    ImGui::ColorConvertHSVtoRGB(hue,
        static_cast<float>(lower * (1.0f - saturation) + upper * saturation),
        static_cast<float>(value <= 0.55 ? styleClamp(value + 0.25, 0.45, 0.95) :
            styleClamp(value - (0.25 * (1.0f + value)), 0.30, 1.0)), red, green, blue);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(red, green, blue, 1));
}

void popColorScheme() {
    ImGui::PopStyleColor(2);
}

void setTransparency(float alpha, float text) {
    constexpr ImGuiCol colors[] = {ImGuiCol_WindowBg, ImGuiCol_Text, ImGuiCol_FrameBg,
        ImGuiCol_Button, ImGuiCol_Border, ImGuiCol_PopupBg};
    for (const auto id : colors) {
        auto style = ImGui::GetStyleColorVec4(id);
        style.w = id == ImGuiCol_Text ? text : alpha;
        ImGui::PushStyleColor(id, style);
    }
}

void resetTransparency() {
    ImGui::PopStyleColor(6);
}

} // namespace nijigenerate::panels::parameters::detail

namespace nijigenerate::panels::parameters {

void incRunPendingParameterUiCommand() {
    if (!detail::pendingParameterUiCommand) return;
    auto action = detail::pendingParameterUiCommand;
    detail::pendingParameterUiCommand = nullptr;
    action();
}

} // namespace nijigenerate::panels::parameters
