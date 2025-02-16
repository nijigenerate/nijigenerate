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
import std.algorithm;
import std.algorithm.searching;
import std.algorithm.mutation;
import std.typecons: tuple;
import std.conv;
import std.utf;
import std.array;
import i18n;
import std.range: enumerate;

/// Model View

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: Node) : BaseInspector!(mode, T) 
    if (!is(T: Composite) && !is(T: MeshGroup) && !is(T: Drawable) && !is(T: SimplePhysics) && !is(T: ExCamera) && !is(T: PathDeformer))
{
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }
    override
    void run() {
        if (targets.length == 0) return;
        auto node = targets[0];
        if (node == incActivePuppet().root) return;
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
                if (_shared!(translationX)(
                        ()=>incDragFloat("translation_x", &translationX.value, adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                    translationX.apply();
                    incActionPush(
                        new NodeValueChangeAction!(Node[], float)(
                            "X",
                            targets, 
                            targets.map!((n)=>incGetDragFloatInitialValue("translation_x")).array,
                            targets.map!((n)=>translationX.value).array,
                            targets.map!((n)=>&n.localTransform.translation.vector[0]).array
                        )
                    );
                }
                igPopID();

                igSameLine(0, 4);

                // Translation Y
                igPushID(1);
                    if (_shared!(translationY)(
                            ()=>incDragFloat("translation_y", &translationY.value, adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                        translationY.apply();
                        incActionPush(
                            new NodeValueChangeAction!(Node[], float)(
                                "Y",
                                targets, 
                                targets.map!((n)=>incGetDragFloatInitialValue("translation_y")).array,
                                targets.map!((n)=>translationY.value).array,
                                targets.map!((n)=>&n.localTransform.translation.vector[1]).array
                            )
                        );
                    }
                igPopID();

                igSameLine(0, 4);

                // Translation Z
                igPushID(2);
                    if (_shared!(translationZ)(
                            ()=>incDragFloat("translation_z", &translationZ.value, adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                        translationZ.apply();
                        incActionPush(
                            new NodeValueChangeAction!(Node[], float)(
                                "Z",
                                targets, 
                                targets.map!((n)=>incGetDragFloatInitialValue("translation_z")).array,
                                targets.map!((n)=>translationZ.value).array,
                                targets.map!((n)=>&n.localTransform.translation.vector[2]).array
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
                    if (_shared!lockToRoot(()=>incLockButton(&lockToRoot.value, "root_lk"))) {
                        foreach (t; targets) {
                            t.lockToRoot = !lockToRoot.value;
                            incLockToRootNode(t);
                        }
                    }

                    if (_shared!pinToMesh(()=>ngCheckbox(__("Pin origin to parent mesh."), &pinToMesh.value))) {
                        pinToMesh.apply();
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
                    rotationDegrees = degrees(rotationX.value);
                    if (_shared!(rotationX)(
                        ()=>incDragFloat("rotation_x", &rotationDegrees, adjustSpeed/100, -float.max, float.max, "%.2f°", ImGuiSliderFlags.NoRoundToFormat))) {
                        rotationX.value = radians(rotationDegrees);
                        rotationX.apply();
                        incActionPush(
                            new NodeValueChangeAction!(Node[], float)(
                                _("Rotation X"),
                                targets, 
                                targets.map!((n)=>incGetDragFloatInitialValue("rotation_x")).array,
                                targets.map!((n)=>rotationX.value).array,
                                targets.map!((n)=>&n.localTransform.rotation.vector[0]).array
                            )
                        );
                    }
                igPopID();
                
                igSameLine(0, 4);

                // Rotation Y
                igPushID(4);
                    rotationDegrees = degrees(rotationY.value);
                    if (_shared!(rotationY)(
                        ()=>incDragFloat("rotation_y", &rotationDegrees, adjustSpeed/100, -float.max, float.max, "%.2f°", ImGuiSliderFlags.NoRoundToFormat))) {
                        rotationY.value = radians(rotationDegrees);
                        rotationY.apply();
                        incActionPush(
                            new NodeValueChangeAction!(Node[], float)(
                                _("Rotation Y"),
                                targets, 
                                targets.map!((n)=>incGetDragFloatInitialValue("rotation_y")).array,
                                targets.map!((n)=>rotationY.value).array,
                                targets.map!((n)=>&n.localTransform.rotation.vector[1]).array
                            )
                        );
                    }
                igPopID();

                igSameLine(0, 4);

                // Rotation Z
                igPushID(5);
                    rotationDegrees = degrees(rotationZ.value);
                    if (_shared!(rotationZ)(
                        ()=>incDragFloat("rotation_z", &rotationDegrees, adjustSpeed/100, -float.max, float.max, "%.2f°", ImGuiSliderFlags.NoRoundToFormat))) {
                        rotationZ.value = radians(rotationDegrees);
                        rotationZ.apply();
                        incActionPush(
                            new NodeValueChangeAction!(Node[], float)(
                                _("Rotation Z"),
                                targets, 
                                targets.map!((n)=>incGetDragFloatInitialValue("rotation_z")).array,
                                targets.map!((n)=>rotationZ.value).array,
                                targets.map!((n)=>&n.localTransform.rotation.vector[2]).array
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
                    if (_shared!(scaleX)(
                        ()=>incDragFloat("scale_x", &scaleX.value, adjustSpeed/100, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                        scaleX.apply();
                        incActionPush(
                            new NodeValueChangeAction!(Node[], float)(
                                _("Scale X"),
                                targets, 
                                targets.map!((n)=>incGetDragFloatInitialValue("scale_x")).array,
                                targets.map!((n)=>scaleX.value).array,
                                targets.map!((n)=>&n.localTransform.scale.vector[0]).array
                            )
                        );
                    }
                igPopID();

                igSameLine(0, 4);

                // Scale Y
                igPushID(7);
                    if (_shared!(scaleY)(
                        ()=>incDragFloat("scale_y", &scaleY.value, adjustSpeed/100, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                        scaleY.apply();
                        incActionPush(
                            new NodeValueChangeAction!(Node[], float)(
                                _("Scale Y"),
                                targets, 
                                targets.map!((n)=>incGetDragFloatInitialValue("scale_y")).array,
                                targets.map!((n)=>scaleY.value).array,
                                targets.map!((n)=>&n.localTransform.scale.vector[1]).array
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
            if (_shared!(pixelSnap)(()=>incLockButton(&pixelSnap.value, "pix_lk"))) {
                pixelSnap.apply();
                incActionPush(
                    new NodeValueChangeAction!(Node[], bool)(
                        _("Snap to Pixel"),
                        targets,
                        targets.map!((n)=>!pixelSnap.value).array,
                        targets.map!((n)=>pixelSnap.value).array,
                        targets.map!((n)=>&n.localTransform.pixelSnap).array
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
            auto zSortB = targets.map!((n)=>n.relZSort).array;
            if (_shared!zSort(()=>igInputFloat("###ZSort", &zSort.value, 0.01, 0.05, "%0.2f"))) {
                zSort.apply();
                incActionPush(
                    new NodeValueChangeAction!(Node[], float)(
                        _("Sorting"),
                        targets,
                        zSortB,
                        targets.map!((n)=>zSort.value).array,
                        targets.map!((n)=>&n.relZSort()).array
                    )
                );
                node.notifyChange(node, NotifyReason.AttributeChanged);
            }
        }
        incEndCategory();
    }

    mixin MultiEdit;
    mixin(attribute!(float, "translationX", (x)=>x~".localTransform.translation.vector[0]", (x, v)=>x~".localTransform.translation.vector[0]="~v));
    mixin(attribute!(float, "translationY", (x)=>x~".localTransform.translation.vector[1]", (x, v)=>x~".localTransform.translation.vector[1]="~v));
    mixin(attribute!(float, "translationZ", (x)=>x~".localTransform.translation.vector[2]", (x, v)=>x~".localTransform.translation.vector[2]="~v));
    mixin(attribute!(float, "rotationX", (x)=>x~".localTransform.rotation.vector[0]", (x, v)=>x~".localTransform.rotation.vector[0]="~v));
    mixin(attribute!(float, "rotationY", (x)=>x~".localTransform.rotation.vector[1]", (x, v)=>x~".localTransform.rotation.vector[1]="~v));
    mixin(attribute!(float, "rotationZ", (x)=>x~".localTransform.rotation.vector[2]", (x, v)=>x~".localTransform.rotation.vector[2]="~v));
    mixin(attribute!(float, "scaleX", (x)=>x~".localTransform.scale.vector[0]", (x, v)=>x~".localTransform.scale.vector[0]="~v));
    mixin(attribute!(float, "scaleY", (x)=>x~".localTransform.scale.vector[1]", (x, v)=>x~".localTransform.scale.vector[1]="~v));
    mixin(attribute!(bool, "lockToRoot"));
    mixin(attribute!(bool, "pinToMesh"));
    mixin(attribute!(bool, "pixelSnap", (x)=>x~".localTransform.pixelSnap", (x, v)=>x~".localTransform.pixelSnap="~v));
    mixin(attribute!(float, "zSort", (x)=>x~".relZSort", (x, v)=>x~".zSort="~v));

    override
    void capture(Node[] nodes) {
        super.capture(nodes);
        translationX.capture();
        translationY.capture();
        translationZ.capture();
        rotationX.capture();
        rotationY.capture();
        rotationZ.capture();
        scaleX.capture();
        scaleY.capture();
        lockToRoot.capture();
        pinToMesh.capture();
        pixelSnap.capture();
        zSort.capture();
    }

}


/// Armed parameter view


class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Deform, T: Node) : BaseInspector!(mode, T)
    if (!is(T: Composite) && !is(T: MeshGroup) && !is(T: Drawable) && !is(T: SimplePhysics) && !is(T: ExCamera) && !is(T: PathDeformer))
{
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }
    override
    void run(Parameter param, vec2u cursor)  {
        if (targets.length == 0) return;
        auto node = targets[0];
        if (node == incActivePuppet().root) return;
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
}
