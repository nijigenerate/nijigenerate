/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.panels.animlist;
import nijigenerate.panels;
import nijigenerate : EditMode;
import i18n;
import bindbc.imgui;
import nijigenerate.project;
import std.string;
import nijilive;
import nijigenerate.widgets;
import nijigenerate.windows;

/**
    The logger frame
*/
class AnimListPanel : Panel {
private:

protected:
    override
    void onUpdate() {
        auto canim = incAnimationGet();
        string currAnimName = canim ? canim.name : "";

        if (igBeginChild("ANIM_LIST", ImVec2(0, -32), true)) {
            foreach(name, ref anim; incActivePuppet().getAnimations()) {
                igPushID(name.ptr, name.ptr+name.length);
                    if (igBeginPopup("###OPTIONS")) {
                        if (igMenuItem(__("Duplicate"))) {
                            import nijilive.fmt.serialize : inToJson, inLoadJsonDataFromMemory;

                            // maybe we can just serialize and deserialize, without deep copy implementation
                            auto newAnim = inLoadJsonDataFromMemory!Animation(inToJson(anim));
                            auto newName = name ~ "_copy";
                            while (newName in incActivePuppet().getAnimations()) {
                                newName ~= "_copy";
                            }

                            incActivePuppet().getAnimations()[newName] = newAnim;
                            newAnim.finalize(incActivePuppet());
                            incAnimationChange(newName);
                        }

                        if (igMenuItem(__("Edit..."))) {
                            incPushWindow(new EditAnimationWindow(anim, name));
                        }
                        if (igMenuItem(__("Delete"))) {
                            incActivePuppet().getAnimations().remove(name);
                            igEndPopup();
                            igPopID();
                            igEndChild();
                            return;
                        }
                        igEndPopup();
                    }

                    if (igSelectable(name.toStringz, name == currAnimName)) {
                        incAnimationChange(name);
                    }

                    igOpenPopupOnItemClick("###OPTIONS", ImGuiPopupFlags.MouseButtonRight);
                igPopID();
            }
        }
        igEndChild();

        incDummy(ImVec2(0, 2));

        if (igBeginChild("ANIM_BTNS", ImVec2(0, 0), false, ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse)) {

            incDummy(ImVec2(-26, 0));
            igSameLine(0, 0);

            if (incButtonColored("", ImVec2(24, 24))) {
                incPushWindow(new EditAnimationWindow());
            }
        }
        igEndChild();
    }

public:
    this() {
        super("Animation List", _("Animation List"), false);
        activeModes = EditMode.AnimEdit;
    }
}

/**
    Generate logger frame
*/
mixin incPanel!AnimListPanel;


