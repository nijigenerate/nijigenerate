/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.viewport;

import nijilive;
import nijigenerate.project;
public import nijigenerate.viewport.base;

private {

    class ModelView {
        void onEditModeChanging(EditMode mode) {
            incViewportWithdrawMode(mode);
        }
        void onEditModeChanged(EditMode mode) {
            incViewportPresentMode(mode);
        }

        void onCameraFocused(float focus, vec2 pos) {
            incViewportTargetZoom = focus;
            incViewportTargetPosition = pos;
        }
    }
    ModelView view;
    static this() {
        view = new ModelView;
        ngRegisterProjectCallback((Project project) { 
            incViewportPresentMode(editMode_);
            project.EditModeChanging.connect(&view.onEditModeChanging);
            project.EditModeChanged.connect(&view.onEditModeChanged);
            project.CameraFocused.connect(&view.onCameraFocused);
            incViewportReset();
        });
    }
}