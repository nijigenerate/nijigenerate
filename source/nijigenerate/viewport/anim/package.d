/*
    Copyright © 2020-2023, nijilife Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.viewport.anim;
import nijigenerate.core.input;
import nijigenerate;
import nijilife;
import bindbc.imgui;

// No overlay in deform mode
void incViewportAnimOverlay() { }

void incViewportAnimUpdate(ImGuiIO* io, Camera camera) {
    
}

void incViewportAnimDraw(Camera camera) {
    incActivePuppet.update();
    incActivePuppet.draw();
}

void incViewportAnimToolbar() {

}

void incViewportAnimPresent() {

}

void incViewportAnimWithdraw() {

}