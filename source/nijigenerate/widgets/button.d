/*
    Copyright © 2022, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.widgets.button;
import nijigenerate.widgets;
//import nijigenerate.core;
import nijilive;
import std.math : isFinite;
import std.string;

const ImVec4 colorUndefined = ImVec4.init;

private {
    ImVec4 buttonTextColor = colorUndefined;
}

ImVec4 ngButtonTextColor() {
    return buttonTextColor;
}

void ngButtonTextColor(ImVec4 color) {
    buttonTextColor = color;
}

bool incButtonColored(const(char)* text, const ImVec2 size = ImVec2(0, 0), const ImVec4 textColor = colorUndefined) {

    auto style = igGetStyle();
    bool pressed = false;
    ImVec2 originalFramePadding = style.FramePadding;

    if (size.x != 0) {
        style.FramePadding.x = 0;
    }
    if (size.y != 0) {
        style.FramePadding.y = 0;
    }

    if (!isFinite(textColor.x) || !isFinite(textColor.y) || !isFinite(textColor.z) || !isFinite(textColor.w)) {
        if (isFinite(buttonTextColor.x)&&isFinite(buttonTextColor.y)&&isFinite(buttonTextColor.z)&&isFinite(buttonTextColor.w)) {
            igPushStyleColor(ImGuiCol.Text, buttonTextColor);
            pressed = igButton(text, size);
            igPopStyleColor();
        } else {
            pressed = igButton(text, size);
        }
    } else {
        igPushStyleColor(ImGuiCol.Text, textColor);
        pressed = igButton(text, size);
        igPopStyleColor();
    }
    style.FramePadding = originalFramePadding;
    return pressed;
}

bool incDropdownButtonIcon(string idStr, string icon, ImVec2 size = ImVec2(-1, -1), bool open=false) {
    
    // Early escape to incDropdownButton if no icon is set
    if (icon.length == 0) return incDropdownButton(idStr, size, open);

    // Otherwise we begin our special code path
    auto ctx = igGetCurrentContext();
    auto window = igGetCurrentWindow();
    auto style = igGetStyle();

    if (window.SkipItems)
        return false;
    
    // Should always appear on same line
    igSameLine(0, 0);
    ImVec2 isize = incMeasureString(icon);

    const(float) default_size = igGetFrameHeight();
    if (size.x <= 0) size.x = isize.x+28;
    if (size.y <= 0) size.y = default_size;

    auto id = igGetID(idStr.ptr, idStr.ptr+idStr.length);
    const(ImRect) bb = {
        window.DC.CursorPos,
        ImVec2(window.DC.CursorPos.x + size.x, window.DC.CursorPos.y + size.y)
    };

    igItemSize(size, (size.y >= default_size) ? ctx.Style.FramePadding.y : -1.0f);
    if (!igItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = igButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags.None);

    // Render
    const ImU32 bgCol = igGetColorU32(
        ((held && hovered) || open) ? style.Colors[ImGuiCol.ButtonActive] : hovered ? 
            style.Colors[ImGuiCol.ButtonHovered] : 
            style.Colors[ImGuiCol.Button]);
    igRenderNavHighlight(bb, id);
    igRenderFrame(bb.Min, bb.Max, bgCol, true, ctx.Style.FrameRounding);
    string s = "";
    ImVec2 ssize = incMeasureString(s);


    if (isFinite(buttonTextColor.x)&&isFinite(buttonTextColor.y)&&isFinite(buttonTextColor.z)&&isFinite(buttonTextColor.w)) {
        igPushStyleColor(ImGuiCol.Text, buttonTextColor);
    }

    igRenderText(ImVec2(
        bb.Min.x + max(0.0f, 4), 
        bb.Min.y + max(0.0f, (size.y - isize.y) * 0.5f)
    ), icon.ptr, icon.ptr+icon.length, true);
    
    igRenderText(ImVec2(
        bb.Min.x + max(0.0f, (size.x - (ssize.x+2))), 
        bb.Min.y + max(0.0f, (size.y - ssize.y) * 0.5f)
    ), s.ptr, s.ptr+s.length, true);

    if (isFinite(buttonTextColor.x)&&isFinite(buttonTextColor.y)&&isFinite(buttonTextColor.z)&&isFinite(buttonTextColor.w)) {
        igPopStyleColor();
    }

    return pressed;
}

bool incDropdownButton(string idStr, ImVec2 size = ImVec2(-1, -1), bool open=false) {
    auto ctx = igGetCurrentContext();
    auto window = igGetCurrentWindow();
    auto style = igGetStyle();

    if (window.SkipItems)
        return false;
    
    // Should always appear on same line
    igSameLine(0, 0);

    const(float) default_size = igGetFrameHeight();
    if (size.x <= 0) size.x = 16;
    if (size.y <= 0) size.y = default_size;

    auto id = igGetID(idStr.ptr, idStr.ptr+idStr.length);
    const(ImRect) bb = {
        window.DC.CursorPos,
        ImVec2(window.DC.CursorPos.x + size.x, window.DC.CursorPos.y + size.y)
    };

    igItemSize(size, (size.y >= default_size) ? ctx.Style.FramePadding.y : -1.0f);
    if (!igItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = igButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags.None);

    // Render
    const ImU32 bgCol = igGetColorU32(
        ((held && hovered) || open) ? style.Colors[ImGuiCol.ButtonActive] : hovered ? 
            style.Colors[ImGuiCol.ButtonHovered] : 
            style.Colors[ImGuiCol.Button]);
    igRenderNavHighlight(bb, id);
    igRenderFrame(bb.Min, bb.Max, bgCol, true, ctx.Style.FrameRounding);
    const(string) s = "";
    ImVec2 ssize = incMeasureString(s);

    if (isFinite(buttonTextColor.x)&&isFinite(buttonTextColor.y)&&isFinite(buttonTextColor.z)&&isFinite(buttonTextColor.w)) {
        igPushStyleColor(ImGuiCol.Text, buttonTextColor);
    }

    igRenderText(ImVec2(
        bb.Min.x + max(0.0f, (size.x - ssize.x) * 0.5f), 
        bb.Min.y + max(0.0f, (size.y - ssize.y) * 0.5f)
    ), s.ptr, s.ptr+s.length, true);

    if (isFinite(buttonTextColor.x)&&isFinite(buttonTextColor.y)&&isFinite(buttonTextColor.z)&&isFinite(buttonTextColor.w)) {
        igPopStyleColor();
    }

    return pressed;
}

private {
    struct DropDownMenuData {
        bool wasOpen;
        ImVec2 winSize;
    }
}

bool incBeginDropdownMenu(string idStr, string icon="", ImVec2 cMin=ImVec2(192, 0), ImVec2 cMax=ImVec2(192, float.max)) {
    auto storage = igGetStateStorage();
    auto window = igGetCurrentWindow();
    auto id = igGetID(idStr.ptr, idStr.ptr+idStr.length);

    igPushID(id);
    DropDownMenuData* menuData = cast(DropDownMenuData*)ImGuiStorage_GetVoidPtr(storage, igGetID("WAS_OPEN"));
    if (!menuData) {
        menuData = cast(DropDownMenuData*)igMemAlloc(DropDownMenuData.sizeof);
        ImGuiStorage_SetVoidPtr(storage, igGetID("WAS_OPEN"), menuData);
    }

    // Dropdown button itself
    auto pressed = incDropdownButtonIcon("DROPDOWN_BTN", icon, ImVec2(-1, -1), menuData.wasOpen);
    if (igIsPopupOpen("DROPDOWN_CONTENT") && pressed) igClosePopupsOverWindow(window, true);
    else if (pressed) igOpenPopup("DROPDOWN_CONTENT", ImGuiPopupFlags.MouseButtonLeft | ImGuiPopupFlags.NoOpenOverItems);

    ImVec2 pos;
    igGetCursorScreenPos(&pos);

    // Clamp to outer window
    if (window) pos.x = clamp(pos.x, window.OuterRectClipped.Max.x, window.OuterRectClipped.Min.x-192);

    // Dropdown menu
    igSetNextWindowSizeConstraints(cMin, cMax);
    igSetNextWindowPos(ImVec2(pos.x, pos.y+4), ImGuiCond.Always, ImVec2(0, 0));
    menuData.wasOpen = igBeginPopup("DROPDOWN_CONTENT");
    if (!menuData.wasOpen) igPopID();
    else {
        menuData.winSize = igGetCurrentWindow().Size;
    }
    return menuData.wasOpen;
}

void incEndDropdownMenu() {
    igEndPopup();
    igPopID();
}

bool ngBeginTabItem(const(char)* text, bool* open = null, ImGuiTabItemFlags flags = ImGuiTabItemFlags.None) {
    bool pressed = false;
    if (isFinite(buttonTextColor.x)&&isFinite(buttonTextColor.y)&&isFinite(buttonTextColor.z)&&isFinite(buttonTextColor.w)) {
        igPushStyleColor(ImGuiCol.Text, buttonTextColor);
        pressed = igBeginTabItem(text, open, flags);
        igPopStyleColor();
    } else {
        pressed = igBeginTabItem(text, open, flags);
    }
    return pressed;
}

void ngEndTabItem() {
    igEndTabItem();
}

bool ngCheckbox(const char* text, bool* value) {
    bool result = false;
    igPushStyleVar(ImGuiStyleVar.FramePadding, ImVec2(2, 2));
    igPushStyleVar(ImGuiStyleVar.ItemInnerSpacing, ImVec2(2, 2));
    igPushStyleVar(ImGuiStyleVar.FrameRounding, 2);
    result = igCheckbox(text, value);
    igPopStyleVar(3);
    return result;
}