module nijigenerate.panels.inspector.drawable;

import nijigenerate.panels.inspector.common;
import nijigenerate;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijilive;
import std.format;
import std.utf;
import std.string;
import i18n;

void incInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: Drawable)(T node) if (!is(T: MeshGroup) && !is(T: Part)) {
    // The main type of anything that can be drawn to the screen
    // in nijilive.
    if (incBeginCategory(__("Drawable"))) {
        float adjustSpeed = 1;
        ImVec2 avail = incAvailableSpace();

        igBeginGroup();
            igTextColored(CategoryTextColor, __("Texture Offset"));
            igPushItemWidth((avail.x-4f)/2f);

                // Translation X
                igPushID(42);
                if (incDragFloat("offset_x", &node.getMesh().origin.vector[0], adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat)) {
                    incActionPush(
                        new NodeValueChangeAction!(Node, float)(
                            "X",
                            node, 
                            incGetDragFloatInitialValue("offset_x"),
                            node.getMesh().origin.vector[0],
                            &node.getMesh().origin.vector[0]
                        )
                    );
                }
                igPopID();

                igSameLine(0, 4);

                // Translation Y
                igPushID(43);
                    if (incDragFloat("offset_y", &node.getMesh().origin.vector[1], adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat)) {
                        incActionPush(
                            new NodeValueChangeAction!(Node, float)(
                                "Y",
                                node, 
                                incGetDragFloatInitialValue("offset_y"),
                                node.getMesh().origin.vector[1],
                                &node.getMesh().origin.vector[1]
                            )
                        );
                    }
                igPopID();
            igPopItemWidth();
        igEndGroup();
    }
    incEndCategory();
}