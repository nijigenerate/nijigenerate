/*
    Copyright © 2022, nijilife Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.panels.toolsettings;
import nijigenerate.viewport;
import nijigenerate.panels;
import nijigenerate.windows;
import nijigenerate : incActivePuppet;
import bindbc.imgui;
import nijilife;
import std.conv;
import i18n;

/**
    A list of tool settings
*/
class ToolSettingsPanel : Panel {
private:

protected:
    override
    void onUpdate() {
        incViewportToolSettings();
    }

public:
    this() {
        super("Tool Settings", _("Tool Settings"), false);
    }
}

/**
    Generate logger frame
*/
mixin incPanel!ToolSettingsPanel;


