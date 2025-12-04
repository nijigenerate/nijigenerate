module nijigenerate.widgets.toggle;

import nijigenerate.widgets;
import bindbc.imgui;
import std.string : toStringz;
import std.algorithm : max;

/// Simple on/off switch (pill style). Returns true if toggled this frame.
bool ngToggleSwitch(string id, ref bool value, ImVec2 size = ImVec2(36, 20)) {
    ImGuiWindow* window = igGetCurrentWindow();
    if (window.SkipItems) return false;

    ImGuiContext* ctx = igGetCurrentContext();
    ImGuiStyle style = ctx.Style;

    float height = size.y > 0 ? size.y : igGetFrameHeight();
    float width  = size.x > 0 ? size.x : height * 1.8f;
    ImVec2 finalSize = ImVec2(width, height);

    string label = "##" ~ id;
    bool toggled = igInvisibleButton(label.toStringz, finalSize);
    if (toggled) value = !value;

    bool hovered = igIsItemHovered();
    bool active  = igIsItemActive();

    ImVec2 minRect; igGetItemRectMin(&minRect);
    ImVec2 maxRect; igGetItemRectMax(&maxRect);
    ImRect bb = ImRect(minRect, maxRect);
    float radius = (bb.Max.y - bb.Min.y) * 0.5f;

    // Colors: off = default frame bg, on = button palette; knob uses window bg with border
    ImVec4 bgOff     = style.Colors[ImGuiCol.FrameBg];
    ImVec4 bgOn      = style.Colors[ImGuiCol.Button]; // on = normal button color
    ImVec4 bgHover   = style.Colors[ImGuiCol.ButtonHovered];
    ImVec4 bgActive  = style.Colors[ImGuiCol.ButtonActive];
    ImVec4 bgColor   = value ? bgOn : bgOff;
    if (hovered || active) bgColor = value ? bgActive : bgHover;

    ImU32 bgColU32   = igGetColorU32(bgColor);
    ImU32 knobCol    = igGetColorU32(style.Colors[ImGuiCol.WindowBg]);
    ImU32 borderCol  = igGetColorU32(style.Colors[ImGuiCol.Border]);

    ImDrawList* draw = igGetWindowDrawList();
    ImDrawList_AddRectFilled(draw, bb.Min, bb.Max, bgColU32, radius);
    ImDrawList_AddRect(draw, bb.Min, bb.Max, borderCol, radius, ImDrawFlags.RoundCornersAll, 1.0f);

    float t = value ? (bb.Max.x - radius) : (bb.Min.x + radius);
    ImVec2 center = ImVec2(t, bb.Min.y + radius);
    float knobR = max(2.0f, radius - 2.0f);
    ImDrawList_AddCircleFilled(draw, center, knobR, knobCol);
    ImDrawList_AddCircle(draw, center, knobR, borderCol, 0, 1.0f);

    return toggled;
}
