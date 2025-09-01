/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.widgets.lock;
import nijigenerate.widgets;
import nijigenerate.core;
import std.string;

/**
    A lock button
*/
bool incLockButton(bool* val, string origin) {
    bool clicked = false;

    igSameLine(0, 0);
    igPushID(origin.ptr);
        igPushItemWidth(16);
            incText(((*val ? "\uE897" : "\uE898")));
            
            if ((clicked = igIsItemClicked(ImGuiMouseButton.Left)) == true) {
                *val = !*val;
            }
            
        igPopItemWidth();
    igPopID();
    return clicked;
}
