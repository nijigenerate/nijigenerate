module nijigenerate.panels.inspector.camera;

import nijigenerate.panels.inspector.common;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate;
import nijigenerate.commands; // cmd!, Context
import nijigenerate.commands.inspector.apply_node : InspectorNodeApplyCommand;
import nijilive;
import i18n;
import std.array;
import std.algorithm;
import std.format;
import std.string;

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: ExCamera) : BaseInspector!(mode, T) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }
    override
    void run() {
        if (targets.length == 0) return;
        auto node = targets[0];
        if (incBeginCategory(__("Camera"))) {
            
            incText(_("Viewport"));
            igIndent();
                igSetNextItemWidth(incAvailableSpace().x);
                if (_shared!viewportOrigin(()=>igDragFloat2("###VIEWPORT", cast(float[2]*)(viewportOrigin.value.ptr)))) {
                    auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                    cmd!(InspectorNodeApplyCommand.ViewportOrigin)(ctx);
                }
            igUnindent();

            // Padding
            igSpacing();
            igSpacing();
        }
        incEndCategory();
    }

    mixin MultiEdit;

    mixin(attribute!(vec2, "viewportOrigin", (x)=>x~".getViewport()", (x, v)=>x~".setViewport("~v~")"));

    override
    void capture(Node[] targets) {
        super.capture(targets);
        viewportOrigin.capture();
    }
}
