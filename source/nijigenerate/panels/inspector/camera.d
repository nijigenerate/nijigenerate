module nijigenerate.panels.inspector.camera;

import nijigenerate.panels.inspector.common;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijilive;
import i18n;

void incInspectorModelCamera(ExCamera node) {
    if (incBeginCategory(__("Camera"))) {
        
        incText(_("Viewport"));
        igIndent();
            igSetNextItemWidth(incAvailableSpace().x);
            igDragFloat2("###VIEWPORT", &node.getViewport().vector);
        igUnindent();

        // Padding
        igSpacing();
        igSpacing();
    }
    incEndCategory();
}
