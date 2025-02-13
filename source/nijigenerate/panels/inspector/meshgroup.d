module nijigenerate.panels.inspector.meshgroup;

import nijigenerate.panels.inspector.common;
import nijigenerate;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijilive;
import i18n;

/// Model View

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: MeshGroup) : BaseInspector!(mode, T) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }

    override
    void run() {
        if (targets.length == 0) return;
        auto node = targets[0];
        if (incBeginCategory(__("MeshGroup"))) {
            

            igSpacing();

            bool dynamic = node.dynamic;
            if (ngCheckbox(__("Dynamic Deformation (slower)"), &dynamic)) {
                node.switchMode(dynamic);
            }
            incTooltip(_("Whether the MeshGroup should dynamically deform children,\nthis is an expensive operation and should not be overused."));

            bool translateChildren = node.getTranslateChildren();
            if (ngCheckbox(__("Translate origins"), &translateChildren)) {
                node.setTranslateChildren(translateChildren);
            }
            incTooltip(_("Translate origin of child nodes for non-Drawable object."));

            // Padding
            igSpacing();
        }
        incEndCategory();
    }
}