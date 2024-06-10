/*
    Copyright © 2020-2023, nijilive Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.widgets.tooltip;
import nijigenerate.widgets;
import std.string;
import nijigenerate.core;

/**
    Creates a new tooltip
*/
void incTooltip(string tip) {
    igPushStyleVar(ImGuiStyleVar.WindowPadding, ImVec2(4, 4));
    if (igIsItemHovered()) {
        igBeginTooltip();

            igPushFont(incMainFont());
                igPushTextWrapPos(igGetFontSize() * 35);
                igTextUnformatted(tip.ptr, tip.ptr+tip.length);
                igPopTextWrapPos();
            igPopFont();

        igEndTooltip();
    }
    igPopStyleVar();
}