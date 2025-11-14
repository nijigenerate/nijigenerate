module nijigenerate.panels.inspector.griddeform;

import nijigenerate.panels.inspector.common;
import nijigenerate;
import nijigenerate.widgets;
import nijigenerate.commands; // cmd!, Context
import nijigenerate.commands.inspector.apply_node : InspectorNodeApplyCommand;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import i18n;
import std.algorithm : sort, map;
import std.algorithm.iteration : uniq;
import std.array : array;
import std.string;

/// Model View

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: GridDeformer) : BaseInspector!(mode, T) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }

    override
    void run() {
        if (targets.length == 0) return;
        auto node = targets[0];
        if (incBeginCategory(__("GridDeformer"))) {

            igSpacing();

            if (_shared!dynamic(()=>ngCheckbox(__("Dynamic Deformation (slower)"), &dynamic.value))) {
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(InspectorNodeApplyCommand.GridDeformDynamic)(ctx, dynamic.value);
            }
            incTooltip(_("Whether the GridDeformer should dynamically deform its targets. Enabling this has a performance cost."));

            igSpacing();

            auto baseVerts = node.vertices;
            igTextColored(CategoryTextColor, __("Grid Information"));
            igText(__("Vertices: %d"), cast(int)baseVerts.length);

            size_t cols = 0;
            size_t rows = 0;
            bool validGrid = false;
            if (baseVerts.length >= 4) {
                auto baseArray = baseVerts.toArray();
                auto xs = baseArray.map!(v => v.x).array;
                auto ys = baseArray.map!(v => v.y).array;
                xs.sort();
                ys.sort();
                xs = xs.uniq.array;
                ys = ys.uniq.array;
                cols = xs.length;
                rows = ys.length;
                validGrid = cols >= 2 && rows >= 2 && cols * rows == baseVerts.length;
            }

            if (validGrid) {
                igText(__("Columns: %d"), cast(int)cols);
                igText(__("Rows: %d"), cast(int)rows);
            } else {
                igText(__("Grid axes are not initialized."));
            }
        }
        incEndCategory();
    }

    mixin MultiEdit;
    mixin(attribute!(bool, "dynamic", (x)=>x~".dynamic", (x, v)=>x~".switchDynamic("~v~")"));

    override
    void capture(Node[] nodes) {
        super.capture(nodes);
        dynamic.capture();
    }
}
