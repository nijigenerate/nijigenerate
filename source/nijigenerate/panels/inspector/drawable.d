module nijigenerate.panels.inspector.drawable;

import nijigenerate.panels.inspector.common;
import nijigenerate;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.commands; // cmd!, Context
import nijigenerate.commands.inspector.apply_node : InspectorNodeApplyCommand; // enum ids
import nijilive;
import std.format;
import std.utf;
import std.string;
import i18n;
import std.algorithm;
import std.array;

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: Drawable) : BaseInspector!(mode, T) if (!is(T: MeshGroup) && !is(T: Part)) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }

    // The main type of anything that can be drawn to the screen
    // in nijilive.
    override
    void run() {
        if (targets.length == 0) return;
        auto node = targets[0];
        if (incBeginCategory(__("Drawable"))) {
            float adjustSpeed = 1;
            ImVec2 avail = incAvailableSpace();

            igBeginGroup();
                igTextColored(CategoryTextColor, __("Texture Offset"));
                igPushItemWidth((avail.x-4f)/2f);

                    // Translation X
                    igPushID(42);
                    if (_shared!(offsetX)(
                        ()=>incDragFloat("offset_x", &offsetX.value, adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                        auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                        cmd!(InspectorNodeApplyCommand.OffsetX)(ctx);
                    }
                    igPopID();

                    igSameLine(0, 4);

                    // Translation Y
                    igPushID(43);
                        if (_shared!(offsetY)(
                            ()=>incDragFloat("offset_y", &offsetY.value, adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                            auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                            cmd!(InspectorNodeApplyCommand.OffsetY)(ctx);
                        }
                    igPopID();
                igPopItemWidth();
            igEndGroup();
        }
        incEndCategory();
    }

    mixin MultiEdit;
    mixin(attribute!(float, "offsetX", (x)=>x~".getMesh().origin.vector[0]", (x, v)=>x~".getMesh().origin.vector[0]="~v));
    mixin(attribute!(float, "offsetY", (x)=>x~".getMesh().origin.vector[1]", (x, v)=>x~".getMesh().origin.vector[1]="~v));

    override
    void capture(Node[] nodes) {
        super.capture(nodes);
        offsetX.capture();
        offsetY.capture();
    }
}
