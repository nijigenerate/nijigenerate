/*
    Copyright © 2022, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.panels.toolsettings;
import nijigenerate.viewport;
import nijigenerate.panels;
import nijigenerate.windows;
import nijigenerate : incActivePuppet;
import bindbc.imgui;
import nijilive;
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
        incViewport.toolSettings();
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


