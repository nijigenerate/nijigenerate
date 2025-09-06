/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.viewport;

import nijilive;
import nijilive.core.animation.player;
import nijigenerate.project;
public import nijigenerate.viewport.base;
import nijigenerate.viewport.vertex.automesh.alpha_provider : alphaPreviewDisposeTexture;

private {

    class ModelView {
        void onEditModeChanging(EditMode mode) {
            incViewport.withdraw();
        }
        void onEditModeChanged(EditMode mode) {
            incViewport.onEditModeChanged(mode);
            incViewport.present();
            // Dispose alpha preview texture when exiting VertexEdit
            if (mode != EditMode.VertexEdit) {
                alphaPreviewDisposeTexture();
            }
        }

        void onCameraFocused(float focus, vec2 pos) {
            incViewportTargetZoom = focus;
            incViewportTargetPosition = pos;
        }

        void onAnimationChanged(AnimationPlayback* anim) {
            incViewport.animationChanged(*anim.animation);
        }
    }
    ModelView view;
    static this() {
        view = new ModelView;
        ngRegisterProjectCallback((Project project) { 
            incViewport.present();
            project.EditModeChanging.connect(&view.onEditModeChanging);
            project.EditModeChanged.connect(&view.onEditModeChanged);
            project.CameraFocused.connect(&view.onCameraFocused);
            project.SelectionChanged.connect(&incViewport.selectionChanged);
            project.ArmedParameterChanged.connect(&incViewport.armedParameterChanged);
            project.AnimationChanged.connect(&view.onAnimationChanged);
            incViewportReset();
        });
    }
}
