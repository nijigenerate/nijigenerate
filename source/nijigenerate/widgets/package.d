/*
    Copyright © 2020-2023, nijilive Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.widgets;

public import bindbc.imgui;
public import nijigenerate.widgets.inputtext;
public import nijigenerate.widgets.progress;
public import nijigenerate.widgets.controller;
public import nijigenerate.widgets.toolbar;
public import nijigenerate.widgets.mainmenu;
public import nijigenerate.widgets.tooltip;
public import nijigenerate.widgets.statusbar;
public import nijigenerate.widgets.secrets;
public import nijigenerate.widgets.dummy;
public import nijigenerate.widgets.drag;
public import nijigenerate.widgets.lock;
public import nijigenerate.widgets.button;
public import nijigenerate.widgets.dialog;
public import nijigenerate.widgets.label;
public import nijigenerate.widgets.texture;
public import nijigenerate.widgets.category;
public import nijigenerate.widgets.dragdrop;
public import nijigenerate.widgets.timeline;
public import nijigenerate.widgets.modal;

bool incBegin(const(char)* name, bool* pOpen, ImGuiWindowFlags flags) {
    version (NoUIScaling) {
        return igBegin(
            name, 
            pOpen, 
            incIsWayland() ? flags : flags | ImGuiWindowFlags.NoDecoration
        );
    } else version (UseUIScaling) {
        return igBegin(
            name, 
            pOpen, 
            flags
        );
    }
}

void incEnd() {
    igEnd();
}