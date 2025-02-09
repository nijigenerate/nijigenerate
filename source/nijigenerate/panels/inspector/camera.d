module nijigenerate.panels.inspector.camera;

import nijigenerate.panels.inspector.common;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate;
import nijilive;
import i18n;

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: ExCamera) : BaseInspector!(mode, T) {
    override
    void run(T node) {
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
}
