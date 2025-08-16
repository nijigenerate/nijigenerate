module nijigenerate.panels.inspector.meshgroup;

import nijigenerate.panels.inspector.common;
import nijigenerate;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate.commands; // cmd!, Context
import nijigenerate.commands.inspector.apply_node : InspectorNodeApplyCommand;
import nijilive;
import i18n;
import std.format;
import std.algorithm;
import std.algorithm.searching;
import std.string;


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

            if (_shared!dynamic(()=>ngCheckbox(__("Dynamic Deformation (slower)"), &dynamic.value))) {
                auto ctx = new Context(); ctx.inspector = this; ctx.nodes(cast(Node[])targets);
                cmd!(InspectorNodeApplyCommand.MeshGroupDynamic)(ctx);
            }
            incTooltip(_("Whether the MeshGroup should dynamically deform children,\nthis is an expensive operation and should not be overused."));

            if (_shared!translateChildren(()=>ngCheckbox(__("Translate origins"), &translateChildren.value))) {
                auto ctx = new Context(); ctx.inspector = this; ctx.nodes(cast(Node[])targets);
                cmd!(InspectorNodeApplyCommand.MeshGroupTranslateChildren)(ctx);
            }
            incTooltip(_("Translate origin of child nodes for non-Drawable object."));

            // Padding
            igSpacing();
        }
        incEndCategory();
    }

    mixin MultiEdit;
    mixin(attribute!(bool, "dynamic", null, (x, v)=>x~".switchMode("~v~")"));
    mixin(attribute!(bool, "translateChildren", (x)=>x~".getTranslateChildren()", (x, v)=>x~".setTranslateChildren("~v~")"));

    override
    void capture(Node[] nodes) {
        super.capture(nodes);
        dynamic.capture();
        translateChildren.capture();
    }
}
