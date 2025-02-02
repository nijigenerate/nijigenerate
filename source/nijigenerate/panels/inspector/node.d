module nijigenerate.panels.inspector.node;

import nijigenerate.viewport.vertex;
import nijigenerate.viewport.model.deform;
import nijigenerate.core;
import nijigenerate.panels;
import nijigenerate.panels.inspector.common;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijigenerate.windows;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate;
import nijilive;
import nijilive.core.nodes.common;
import std.string;
import std.algorithm.searching;
import std.algorithm.mutation;
import std.typecons: tuple;
import std.conv;
import std.utf;
import i18n;
import std.range: enumerate;

/// Model View

void incInspectorModelTRS(Node node) {
    if (incBeginCategory(__("Transform"))) {
        float adjustSpeed = 1;
        // if (igIsKeyDown(igGetKeyIndex(ImGuiKeyModFlags_Shift))) {
        //     adjustSpeed = 0.1;
        // }

        ImVec2 avail;
        igGetContentRegionAvail(&avail);

        float fontSize = 16;

        //
        // Translation
        //

        // Translation portion of the transformation matrix.
        igTextColored(CategoryTextColor, __("Translation"));
        igPushItemWidth((avail.x-4f)/3f);

            // Translation X
            igPushID(0);
            if (incDragFloat("translation_x", &node.localTransform.translation.vector[0], adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat)) {
                incActionPush(
                    new NodeValueChangeAction!(Node, float)(
                        "X",
                        node, 
                        incGetDragFloatInitialValue("translation_x"),
                        node.localTransform.translation.vector[0],
                        &node.localTransform.translation.vector[0]
                    )
                );
            }
            igPopID();

            igSameLine(0, 4);

            // Translation Y
            igPushID(1);
                if (incDragFloat("translation_y", &node.localTransform.translation.vector[1], adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat)) {
                    incActionPush(
                        new NodeValueChangeAction!(Node, float)(
                            "Y",
                            node, 
                            incGetDragFloatInitialValue("translation_y"),
                            node.localTransform.translation.vector[1],
                            &node.localTransform.translation.vector[1]
                        )
                    );
                }
            igPopID();

            igSameLine(0, 4);

            // Translation Z
            igPushID(2);
                if (incDragFloat("translation_z", &node.localTransform.translation.vector[2], adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat)) {
                    incActionPush(
                        new NodeValueChangeAction!(Node, float)(
                            "Z",
                            node, 
                            incGetDragFloatInitialValue("translation_z"),
                            node.localTransform.translation.vector[2],
                            &node.localTransform.translation.vector[2]
                        )
                    );
                }
            igPopID();


        
            // Padding
            igSpacing();
            igSpacing();
            
            igBeginGroup();
                // Button which locks all transformation to be based off the root node
                // of the puppet, this more or less makes the item stay in place
                // even if the parent moves.
                ImVec2 textLength = incMeasureString(_("Lock to Root Node"));
                igTextColored(CategoryTextColor, __("Lock to Root Node"));

                incSpacer(ImVec2(-12, 1));
                bool lockToRoot = node.lockToRoot;
                if (incLockButton(&lockToRoot, "root_lk")) {
                    incLockToRootNode(node);
                }

                bool pinToMesh = node.pinToMesh;
                if (igCheckbox(__("Pin origin to parent mesh."), &pinToMesh)) {
                    node.pinToMesh = pinToMesh;
                }
            igEndGroup();

            // Button which locks all transformation to be based off the root node
            // of the puppet, this more or less makes the item stay in place
            // even if the parent moves.
            incTooltip(_("Makes so that the translation of this node is based off the root node, making it stay in place even if its parent moves."));
        
            // Padding
            igSpacing();
            igSpacing();

        igPopItemWidth();


        //
        // Rotation
        //
        igSpacing();
        
        // Rotation portion of the transformation matrix.
        igTextColored(CategoryTextColor, __("Rotation"));
        igPushItemWidth((avail.x-4f)/3f);
            float rotationDegrees;

            // Rotation X
            igPushID(3);
                rotationDegrees = degrees(node.localTransform.rotation.vector[0]);
                if (incDragFloat("rotation_x", &rotationDegrees, adjustSpeed/100, -float.max, float.max, "%.2f°", ImGuiSliderFlags.NoRoundToFormat)) {       
                    node.localTransform.rotation.vector[0] = radians(rotationDegrees);         
                    
                    incActionPush(
                        new NodeValueChangeAction!(Node, float)(
                            _("Rotation X"),
                            node, 
                            incGetDragFloatInitialValue("rotation_x"),
                            node.localTransform.rotation.vector[0],
                            &node.localTransform.rotation.vector[0]
                        )
                    );
                }
            igPopID();
            
            igSameLine(0, 4);

            // Rotation Y
            igPushID(4);
                rotationDegrees = degrees(node.localTransform.rotation.vector[1]);
                if (incDragFloat("rotation_y", &rotationDegrees, adjustSpeed/100, -float.max, float.max, "%.2f°", ImGuiSliderFlags.NoRoundToFormat)) {
                    node.localTransform.rotation.vector[1] = radians(rotationDegrees);

                    incActionPush(
                        new NodeValueChangeAction!(Node, float)(
                            _("Rotation Y"),
                            node, 
                            incGetDragFloatInitialValue("rotation_y"),
                            node.localTransform.rotation.vector[1],
                            &node.localTransform.rotation.vector[1]
                        )
                    );
                }
            igPopID();

            igSameLine(0, 4);

            // Rotation Z
            igPushID(5);
                rotationDegrees = degrees(node.localTransform.rotation.vector[2]);
                if (incDragFloat("rotation_z", &rotationDegrees, adjustSpeed/100, -float.max, float.max, "%.2f°", ImGuiSliderFlags.NoRoundToFormat)) {
                    node.localTransform.rotation.vector[2] = radians(rotationDegrees);

                    incActionPush(
                        new NodeValueChangeAction!(Node, float)(
                            _("Rotation Z"),
                            node, 
                            incGetDragFloatInitialValue("rotation_z"),
                            node.localTransform.rotation.vector[2],
                            &node.localTransform.rotation.vector[2]
                        )
                    );
                }
            igPopID();

        igPopItemWidth();

        avail.x += igGetFontSize();

        //
        // Scaling
        //
        igSpacing();
        
        // Scaling portion of the transformation matrix.
        igTextColored(CategoryTextColor, __("Scale"));
        igPushItemWidth((avail.x-14f)/2f);
            
            // Scale X
            igPushID(6);
                if (incDragFloat("scale_x", &node.localTransform.scale.vector[0], adjustSpeed/100, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat)) {
                    incActionPush(
                        new NodeValueChangeAction!(Node, float)(
                            _("Scale X"),
                            node, 
                            incGetDragFloatInitialValue("scale_x"),
                            node.localTransform.scale.vector[0],
                            &node.localTransform.scale.vector[0]
                        )
                    );
                }
            igPopID();

            igSameLine(0, 4);

            // Scale Y
            igPushID(7);
                if (incDragFloat("scale_y", &node.localTransform.scale.vector[1], adjustSpeed/100, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat)) {
                    incActionPush(
                        new NodeValueChangeAction!(Node, float)(
                            _("Scale Y"),
                            node, 
                            incGetDragFloatInitialValue("scale_y"),
                            node.localTransform.scale.vector[1],
                            &node.localTransform.scale.vector[1]
                        )
                    );
                }
            igPopID();

        igPopItemWidth();

        igSpacing();
        igSpacing();

        // An option in which positions will be snapped to whole integer values.
        // In other words texture will always be on a pixel.
        textLength = incMeasureString(_("Snap to Pixel"));
        igTextColored(CategoryTextColor, __("Snap to Pixel"));
        incSpacer(ImVec2(-12, 1));
        if (incLockButton(&node.localTransform.pixelSnap, "pix_lk")) {
            incActionPush(
                new NodeValueChangeAction!(Node, bool)(
                    _("Snap to Pixel"),
                    node, 
                    !node.localTransform.pixelSnap,
                    node.localTransform.pixelSnap,
                    &node.localTransform.pixelSnap
                )
            );
        }
        
        // Padding
        igSpacing();
        igSpacing();

        // The sorting order ID, which nijilive uses to sort
        // Parts to draw in the user specified order.
        // negative values = closer to camera
        // positive values = further away from camera
        igTextColored(CategoryTextColor, __("Sorting"));
        float zsortV = node.relZSort;
        float zsortB = zsortV;
        if (igInputFloat("###ZSort", &zsortV, 0.01, 0.05, "%0.2f")) {
            node.zSort = zsortV;
            node.notifyChange(node, NotifyReason.AttributeChanged);
            incActionPush(
                new NodeValueChangeAction!(Node, float)(
                    _("Sorting"),
                    node, 
                    zsortB,
                    zsortV,
                    &node.relZSort()
                )
            );
        }
    }
    incEndCategory();
}


/// Armed parameter view


void incInspectorDeformTRS(Node node, Parameter param, vec2u cursor) {
    if (incBeginCategory(__("Transform"))) {   
        float adjustSpeed = 1;

        ImVec2 avail;
        igGetContentRegionAvail(&avail);

        float fontSize = 16;

        //
        // Translation
        //



        // Translation portion of the transformation matrix.
        igTextColored(CategoryTextColor, __("Translation"));
        igPushItemWidth((avail.x-4f)/3f);

            // Translation X
            igPushID(0);
                incInspectorDeformFloatDragVal("translation_x", "transform.t.x", 1f, node, param, cursor);
            igPopID();

            igSameLine(0, 4);

            // Translation Y
            igPushID(1);
                incInspectorDeformFloatDragVal("translation_y", "transform.t.y", 1f, node, param, cursor);
            igPopID();

            igSameLine(0, 4);

            // Translation Z
            igPushID(2);
                incInspectorDeformFloatDragVal("translation_z", "transform.t.z", 1f, node, param, cursor);
            igPopID();


        
            // Padding
            igSpacing();
            igSpacing();

        igPopItemWidth();


        //
        // Rotation
        //
        igSpacing();
        
        // Rotation portion of the transformation matrix.
        igTextColored(CategoryTextColor, __("Rotation"));
        igPushItemWidth((avail.x-4f)/3f);

            // Rotation X
            igPushID(3);
                incInspectorDeformFloatDragVal("rotation.x", "transform.r.x", 0.05f, node, param, cursor, true);
            igPopID();

            igSameLine(0, 4);

            // Rotation Y
            igPushID(4);
                incInspectorDeformFloatDragVal("rotation.y", "transform.r.y", 0.05f, node, param, cursor, true);
            igPopID();

            igSameLine(0, 4);

            // Rotation Z
            igPushID(5);
                incInspectorDeformFloatDragVal("rotation.z", "transform.r.z", 0.05f, node, param, cursor, true);
            igPopID();

        igPopItemWidth();

        avail.x += igGetFontSize();

        //
        // Scaling
        //
        igSpacing();
        
        // Scaling portion of the transformation matrix.
        igTextColored(CategoryTextColor, __("Scale"));
        igPushItemWidth((avail.x-14f)/2f);
            
            // Scale X
            igPushID(6);
                incInspectorDeformFloatDragVal("scale.x", "transform.s.x", 0.1f, node, param, cursor);
            igPopID();

            igSameLine(0, 4);

            // Scale Y
            igPushID(7);
                incInspectorDeformFloatDragVal("scale.y", "transform.s.y", 0.1f, node, param, cursor);
            igPopID();

        igPopItemWidth();

        igSpacing();
        igSpacing();

        igTextColored(CategoryTextColor, __("Sorting"));
        incInspectorDeformInputFloat("zSort", "zSort", 0.01, 0.05, node, param, cursor);
    }
    incEndCategory();
}
