/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.windows.texviewer;
import nijigenerate.windows.base;
import nijigenerate.core;
import std.string;
import nijigenerate.utils.link;
import nijilive;
import i18n;

class TextureViewerWindow : Window {
private:
    Texture texture;
    float zoom = 1;

protected:

    override
    void onBeginUpdate() {
        igSetNextWindowSize(ImVec2(512, 512), ImGuiCond.FirstUseEver);
        super.onBeginUpdate();
    }

    override
    void onUpdate() {
        igSliderFloat("Zoom", &zoom, 0.1, 10, "%.2f");
        if (igBeginChild("TextureViewerArea", ImVec2(0, 0), false, ImGuiWindowFlags.HorizontalScrollbar)) {
            igImage(
                cast(void*)texture.getTextureId(), 
                ImVec2(texture.width*zoom, texture.height*zoom), 
                ImVec2(0, 0), 
                ImVec2(1, 1), 
                ImVec4(1, 1, 1, 1), 
                ImVec4(0, 0, 0, 0)
            );
        }
        igEndChild();
    }

public:
    this(Texture texture) {
        this.texture = texture;
        super(_("Texture Viewer"));
    }
}
