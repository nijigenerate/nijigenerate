/*
    Copyright © 2020-2023, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.panels.scene;
import nijigenerate.core;
import nijigenerate.panels;
import nijigenerate.windows;
import nijigenerate.widgets;
import nijigenerate;
import bindbc.imgui;
import nijilive;
import std.conv;
import i18n;
import std.string;


/**
    The textures frame
*/
class ScenePanel : Panel {
protected:
    override
    void onUpdate() {
        igColorEdit3(
            __("Ambient Light"), 
            &inSceneAmbientLight.vector, 
            ImGuiColorEditFlags.PickerHueWheel |
                ImGuiColorEditFlags.NoInputs
        );
    }

public:
    this() {
        super("Scene", _("Scene"), false);
    }
}

/**
    Generate scene panel frame
*/
mixin incPanel!ScenePanel;



