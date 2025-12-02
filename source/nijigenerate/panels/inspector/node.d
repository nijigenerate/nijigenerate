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
import nijigenerate.commands; // cmd!, Context
import nijigenerate.commands.inspector.apply_node : InspectorNodeApplyCommand; // enum ids
import nijilive;
import nijilive.core.nodes.common;
import nijilive.core.nodes.drivers; // SimplePhysics, Driver types
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
    if (!is(T: Composite) && !is(T: MeshGroup) && !is(T: Drawable) && !is(T: SimplePhysics) && !is(T: ExCamera) && !is(T: PathDeformer) && !is(T: GridDeformer))
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
                    auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                    cmd!(InspectorNodeApplyCommand.TranslationX)(ctx, translationX.value);
                }
                igPopID();

                igSameLine(0, 4);

                // Translation Y
                igPushID(1);
                    if (_shared!(translationY)(
                            ()=>incDragFloat("translation_y", &translationY.value, adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                        auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                        cmd!(InspectorNodeApplyCommand.TranslationY)(ctx, translationY.value);
                    }
                igPopID();

                igSameLine(0, 4);

                // Translation Z
                igPushID(2);
                    if (_shared!(translationZ)(
                            ()=>incDragFloat("translation_z", &translationZ.value, adjustSpeed, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                        auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                        cmd!(InspectorNodeApplyCommand.TranslationZ)(ctx, translationZ.value);
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
                        auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                        cmd!(InspectorNodeApplyCommand.LockToRoot)(ctx, lockToRoot.value);
                    }

                    if (_shared!pinToMesh(()=>ngCheckbox(__("Pin origin to parent mesh."), &pinToMesh.value))) {
                        auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                        cmd!(InspectorNodeApplyCommand.PinToMesh)(ctx, pinToMesh.value);
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
                        auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                        cmd!(InspectorNodeApplyCommand.RotationX)(ctx, rotationX.value);
                    }
                igPopID();
                
                igSameLine(0, 4);

                // Rotation Y
                igPushID(4);
                    rotationDegrees = degrees(rotationY.value);
                    if (_shared!(rotationY)(
                        ()=>incDragFloat("rotation_y", &rotationDegrees, adjustSpeed/100, -float.max, float.max, "%.2f°", ImGuiSliderFlags.NoRoundToFormat))) {
                        rotationY.value = radians(rotationDegrees);
                        auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                        cmd!(InspectorNodeApplyCommand.RotationY)(ctx, rotationY.value);
                    }
                igPopID();

                igSameLine(0, 4);

                // Rotation Z
                igPushID(5);
                    rotationDegrees = degrees(rotationZ.value);
                    if (_shared!(rotationZ)(
                        ()=>incDragFloat("rotation_z", &rotationDegrees, adjustSpeed/100, -float.max, float.max, "%.2f°", ImGuiSliderFlags.NoRoundToFormat))) {
                        rotationZ.value = radians(rotationDegrees);
                        auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                        cmd!(InspectorNodeApplyCommand.RotationZ)(ctx, rotationZ.value);
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
                        auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                        cmd!(InspectorNodeApplyCommand.ScaleX)(ctx, scaleX.value);
                    }
                igPopID();

                igSameLine(0, 4);

                // Scale Y
                igPushID(7);
                    if (_shared!(scaleY)(
                        ()=>incDragFloat("scale_y", &scaleY.value, adjustSpeed/100, -float.max, float.max, "%.2f", ImGuiSliderFlags.NoRoundToFormat))) {
                        auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                        cmd!(InspectorNodeApplyCommand.ScaleY)(ctx, scaleY.value);
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
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(InspectorNodeApplyCommand.PixelSnap)(ctx, pixelSnap.value);
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
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(InspectorNodeApplyCommand.ZSort)(ctx, zSort.value);
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
    if (!is(T: Composite) && !is(T: MeshGroup) && !is(T: Drawable) && !is(T: SimplePhysics) && !is(T: ExCamera) && !is(T: PathDeformer) && !is(T: GridDeformer))
{
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }
    override
    void run(Parameter param, vec2u cursor)  {
        if (targets.length == 0) return;
        auto node = targets[0];

        updateDeform(param, cursor);

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
                    _deform!translationX((s,v)=>ngInspectorDeformFloatDragVal(s, v, 1f));
                igPopID();

                igSameLine(0, 4);

                // Translation Y
                igPushID(1);
                    _deform!translationY((s,v)=>ngInspectorDeformFloatDragVal(s, v, 1f));
                igPopID();

                igSameLine(0, 4);

                // Translation Z
                igPushID(2);
                    _deform!translationZ((s,v)=>ngInspectorDeformFloatDragVal(s, v, 1f));
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
                    _deform!rotationX((s,v)=>ngInspectorDeformFloatDragVal(s, v, 0.05f, true));
                igPopID();

                igSameLine(0, 4);

                // Rotation Y
                igPushID(4);
                    _deform!rotationY((s,v)=>ngInspectorDeformFloatDragVal(s, v, 0.05f, true));
                igPopID();

                igSameLine(0, 4);

                // Rotation Z
                igPushID(5);
                    _deform!rotationZ((s,v)=>ngInspectorDeformFloatDragVal(s, v, 0.05f, true));
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
                    _deform!scaleX((s,v)=>ngInspectorDeformFloatDragVal(s, v, 0.1f));
                igPopID();

                igSameLine(0, 4);

                // Scale Y
                igPushID(7);
                    _deform!scaleY((s,v)=>ngInspectorDeformFloatDragVal(s, v, 0.1f));
                igPopID();

            igPopItemWidth();

            igSpacing();
            igSpacing();

            igTextColored(CategoryTextColor, __("Sorting"));
            _deform!zSort((s,v)=>ngInspectorDeformInputFloat(s, v, 0.01, 0.05));
        }
        incEndCategory();
    }

    mixin MultiEdit;
    mixin(deformation!("translationX", "transform.t.x"));
    mixin(deformation!("translationY", "transform.t.y"));
    mixin(deformation!("translationZ", "transform.t.z"));
    mixin(deformation!("rotationX", "transform.r.x"));
    mixin(deformation!("rotationY", "transform.r.y"));
    mixin(deformation!("rotationZ", "transform.r.z"));
    mixin(deformation!("scaleX", "transform.s.x"));
    mixin(deformation!("scaleY", "transform.s.y"));
    mixin(deformation!("zSort"));

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
        zSort.capture();
    }
}
