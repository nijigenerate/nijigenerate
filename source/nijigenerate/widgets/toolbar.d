/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.widgets.toolbar;
import nijigenerate.viewport;
import nijigenerate.widgets;
import nijigenerate.core;
import nijigenerate.windows;
import nijigenerate;
import nijigenerate.viewport.model.onionslice;
import i18n;

bool incBeginInnerToolbar(float height, bool matchTitlebar=false, bool offset=true) {

    auto style = igGetStyle();
    auto window = igGetCurrentWindow();

    auto barColor = matchTitlebar ? (
        igIsWindowFocused(ImGuiFocusedFlags.RootAndChildWindows) ? 
            style.Colors[ImGuiCol.TitleBgActive] : 
            style.Colors[ImGuiCol.TitleBg]
    ) : style.Colors[ImGuiCol.MenuBarBg];

    igPushStyleVar(ImGuiStyleVar.FrameRounding, 0);
    igPushStyleVar(ImGuiStyleVar.ChildBorderSize, 0);
    igPushStyleVar(ImGuiStyleVar.WindowPadding, ImVec2(0, 0));
    igPushStyleVar(ImGuiStyleVar.FrameBorderSize, 0);
    igPushStyleColor(ImGuiCol.ChildBg, barColor);
    igPushStyleColor(ImGuiCol.Button, barColor);

    if (!window.IsExplicitChild) {
        igPushClipRect(
            ImVec2(
                window.OuterRectClipped.Max.x, 
                offset ? window.OuterRectClipped.Max.y-1 : window.OuterRectClipped.Max.y
            ), 
            ImVec2(
                window.OuterRectClipped.Min.x, 
                window.OuterRectClipped.Min.y
            ), 
            false
        );
        igSetCursorPosY(offset ? igGetCursorPosY()-1 : igGetCursorPosY());
    }
    
    bool visible = igBeginChild("###Toolbar", ImVec2(0, height), false, ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse);
    if (visible) igSetCursorPosX(igGetCursorPosX()+style.FramePadding.x);
    return visible;
}

void incEndInnerToolbar() {
    auto window = igGetCurrentWindow();
    if (!window.IsExplicitChild) igPopClipRect();

    igEndChild();
    igPopStyleColor(2);
    igPopStyleVar(4);

    // Move cursor up
    igSetCursorPosY(igGetCursorPosY()-igGetStyle().ItemSpacing.y);
}

/**
    A toolbar button
*/
bool incToolbarButton(const(char)* text, float width = 0) {
    bool clicked = igButton(text, ImVec2(width, incAvailableSpace().y));
    igSameLine(0, 0);
    return clicked;
}

/**
    A toolbar button
*/
void incToolbarSpacer(float space) {
    incDummy(ImVec2(space, 0));
    igSameLine(0, 0);
}

/**
    Vertical separator for toolbar
*/
void incToolbarSeparator() {
    igPushStyleColor(ImGuiCol.Separator, ImVec4(0.5, 0.5, 0.5, 1));
        igSeparatorEx(ImGuiSeparatorFlags.Vertical);
        igSameLine(0, 6);
    igPopStyleColor();
}

void incToolbarText(string text) {
    igSetCursorPosY(6);
    incText(text);
    igSameLine(0, 4);
}
