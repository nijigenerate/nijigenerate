/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.viewport.anim;
import nijigenerate.core.input;
import nijigenerate;
import nijigenerate.viewport.base;

import nijilive;
import bindbc.imgui;

// No overlay in deform mode
void incViewportAnimOverlay() { }


class AnimationViewport : Viewport {
protected:
    bool alwaysUpdateMode = false;
public:
    override
    void draw(Camera camera) { 
        incActivePuppet.update();
        incActivePuppet.draw();

    };
}