#include "nijigenerate/widgets/category_internal.hpp"
namespace nijigenerate::widgets::category::detail {
void getCategoryColors(ImVec4 color, ImVec4& hoverColor, ImVec4& activeColor, ImVec4& bgColor,
                       ImVec4& shadowColor, ImVec4& textColor, bool& isDark) {
    float h = std::numeric_limits<float>::quiet_NaN();
    float s = h;
    float v = h;
    hoverColor = color;
    activeColor = color;
    shadowColor = color;
    bgColor = color;
    ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, h, s, v);
    isDark = v <= 0.5;
    if (!isDark) textColor = ImVec4(0, 0, 0, 1);
    else textColor = ImVec4(1, 1, 1, 1);
    s -= 0.075 * s;
    if (v > 0.5) v -= 0.05;
    else v += 0.15;
    ImGui::ColorConvertHSVtoRGB(h, s, v, activeColor.x, activeColor.y, activeColor.z);
    s -= 0.075 * s;
    if (v > 0.5) v -= 0.05;
    else v += 0.15;
    ImGui::ColorConvertHSVtoRGB(h, s, v, hoverColor.x, hoverColor.y, hoverColor.z);
    ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, h, s, v);
    s *= 0.90;
    if (v <= 0.15) v = 0.15;
    else v *= 0.80;
    ImGui::ColorConvertHSVtoRGB(h, s, v, bgColor.x, bgColor.y, bgColor.z);
    ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, h, s, v);
    s *= 0.85;
    v *= 0.75;
    ImGui::ColorConvertHSVtoRGB(h, s, v, shadowColor.x, shadowColor.y, shadowColor.z);
    shadowColor.w *= 2;
}
}
