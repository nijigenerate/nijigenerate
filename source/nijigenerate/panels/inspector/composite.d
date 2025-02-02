module nijigenerate.panels.inspector.composite;

import nijigenerate;
import nijigenerate.core;
import nijigenerate.panels.inspector.common;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijilive;
import std.format;
import std.utf;
import std.string;
import std.algorithm;
import std.range;
import i18n;

/// Model View


void incInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: Composite)(T node) if (is(T: Composite)) {
    if (incBeginCategory(__("Composite"))) {
        

        igSpacing();

        // BLENDING MODE
        import std.conv : text;
        import std.string : toStringz;


        incText(_("Tint (Multiply)"));
        igColorEdit3("###TINT", cast(float[3]*)node.tint.ptr);

        incText(_("Tint (Screen)"));
        igColorEdit3("###S_TINT", cast(float[3]*)node.screenTint.ptr);

        // Header for the Blending options for Parts
        incText(_("Blending"));
        if (igBeginCombo("###Blending", __(node.blendingMode.text))) {

            // Normal blending mode as used in Photoshop, generally
            // the default blending mode photoshop starts a layer out as.
            if (igSelectable(__("Normal"), node.blendingMode == BlendMode.Normal)) node.blendingMode = BlendMode.Normal;
            
            // Multiply blending mode, in which this texture's color data
            // will be multiplied with the color data already in the framebuffer.
            if (igSelectable(__("Multiply"), node.blendingMode == BlendMode.Multiply)) node.blendingMode = BlendMode.Multiply;
                            
            // Screen blending mode
            if (igSelectable(__("Screen"), node.blendingMode == BlendMode.Screen)) node.blendingMode = BlendMode.Screen;

            // Overlay blending mode
            if (igSelectable(__("Overlay"), node.blendingMode == BlendMode.Overlay)) node.blendingMode = BlendMode.Overlay;

            // Darken blending mode
            if (igSelectable(__("Darken"), node.blendingMode == BlendMode.Darken)) node.blendingMode = BlendMode.Darken;

            // Lighten blending mode
            if (igSelectable(__("Lighten"), node.blendingMode == BlendMode.Lighten)) node.blendingMode = BlendMode.Lighten;
                    
            // Color Dodge blending mode
            if (igSelectable(__("Color Dodge"), node.blendingMode == BlendMode.ColorDodge)) node.blendingMode = BlendMode.ColorDodge;
                    
            // Linear Dodge blending mode
            if (igSelectable(__("Linear Dodge"), node.blendingMode == BlendMode.LinearDodge)) node.blendingMode = BlendMode.LinearDodge;
                    
            // Add (Glow) blending mode
            if (igSelectable(__("Add (Glow)"), node.blendingMode == BlendMode.AddGlow)) node.blendingMode = BlendMode.AddGlow;
                    
            // Color Burn blending mode
            if (igSelectable(__("Color Burn"), node.blendingMode == BlendMode.ColorBurn)) node.blendingMode = BlendMode.ColorBurn;
                    
            // Hard Light blending mode
            if (igSelectable(__("Hard Light"), node.blendingMode == BlendMode.HardLight)) node.blendingMode = BlendMode.HardLight;
                    
            // Soft Light blending mode
            if (igSelectable(__("Soft Light"), node.blendingMode == BlendMode.SoftLight)) node.blendingMode = BlendMode.SoftLight;
                            
            // Subtract blending mode
            if (igSelectable(__("Subtract"), node.blendingMode == BlendMode.Subtract)) node.blendingMode = BlendMode.Subtract;
                            
            // Difference blending mode
            if (igSelectable(__("Difference"), node.blendingMode == BlendMode.Difference)) node.blendingMode = BlendMode.Difference;
                            
            // Exclusion blending mode
            if (igSelectable(__("Exclusion"), node.blendingMode == BlendMode.Exclusion)) node.blendingMode = BlendMode.Exclusion;
                            
            // Inverse blending mode
            if (igSelectable(__("Inverse"), node.blendingMode == BlendMode.Inverse)) node.blendingMode = BlendMode.Inverse;
            incTooltip(_("Inverts the color by a factor of the overlying color"));
                            
            // Destination In blending mode
            if (igSelectable(__("Destination In"), node.blendingMode == BlendMode.DestinationIn)) node.blendingMode = BlendMode.DestinationIn;
            
            igEndCombo();
        }

        igSpacing();

        incText(_("Opacity"));
        if (igSliderFloat("###Opacity", &node.opacity, 0, 1f, "%0.2f")) {
            node.notifyChange(node, NotifyReason.AttributeChanged);
        }
        igSpacing();
        igSpacing();

        igTextColored(CategoryTextColor, __("Masks"));
        igSpacing();

        // Threshold slider name for adjusting how transparent a pixel can be
        // before it gets discarded.
        incText(_("Threshold"));
        igSliderFloat("###Threshold", &node.threshold, 0.0, 1.0, "%.2f");
        
        igSpacing();

        // The sources that the part gets masked by. Depending on the masking mode
        // either the sources will cut out things that don't overlap, or cut out
        // things that do.
        incText(_("Mask Sources"));
        if (igBeginListBox("###MaskSources", ImVec2(0, 128))) {
            if (node.masks.length == 0) {
                incText(_("(Drag a Part or Mask Here)"));
            }

            foreach(i; 0..node.masks.length) {
                MaskBinding* masker = &node.masks[i];
                igPushID(cast(int)i);
                    if (igBeginPopup("###MaskSettings")) {
                        if (igBeginMenu(__("Mode"))) {
                            if (igMenuItem(__("Mask"), null, masker.mode == MaskingMode.Mask)) masker.mode = MaskingMode.Mask;
                            if (igMenuItem(__("Dodge"), null, masker.mode == MaskingMode.DodgeMask)) masker.mode = MaskingMode.DodgeMask;
                            
                            igEndMenu();
                        }

                        if (igMenuItem(__("Delete"))) {
                            import std.algorithm.mutation : remove;
                            node.masks = node.masks.remove(i);
                            igEndPopup();
                            igPopID();
                            igEndListBox();
                            incEndCategory();
                            return;
                        }

                        igEndPopup();
                    }

                    if (masker.mode == MaskingMode.Mask) igSelectable(_("%s (Mask)").format(masker.maskSrc.name).toStringz);
                    else igSelectable(_("%s (Dodge)").format(masker.maskSrc.name).toStringz);

                    
                    if(igBeginDragDropTarget()) {
                        const(ImGuiPayload)* payload = igAcceptDragDropPayload("_MASKITEM");
                        if (payload !is null) {
                            if (MaskBinding* binding = cast(MaskBinding*)payload.Data) {
                                ptrdiff_t maskIdx = node.getMaskIdx(binding.maskSrcUUID);
                                if (maskIdx >= 0) {
                                    import std.algorithm.mutation : remove;

                                    node.masks = node.masks.remove(maskIdx);
                                    if (i == 0) node.masks = *binding ~ node.masks;
                                    else if (i+1 >= node.masks.length) node.masks ~= *binding;
                                    else node.masks = node.masks[0..i] ~ *binding ~ node.masks[i+1..$];
                                }
                            }
                        }
                        
                        igEndDragDropTarget();
                    }

                    // TODO: We really should account for left vs. right handedness
                    if (igIsItemClicked(ImGuiMouseButton.Right)) {
                        igOpenPopup("###MaskSettings");
                    }

                    if(igBeginDragDropSource(ImGuiDragDropFlags.SourceAllowNullID)) {
                        igSetDragDropPayload("_MASKITEM", cast(void*)masker, MaskBinding.sizeof, ImGuiCond.Always);
                        incText(masker.maskSrc.name);
                        igEndDragDropSource();
                    }
                igPopID();
            }
            igEndListBox();
        }

        if(igBeginDragDropTarget()) {
            const(ImGuiPayload)* payload = igAcceptDragDropPayload("_PUPPETNTREE");
            if (payload !is null) {
                if (Drawable payloadDrawable = cast(Drawable)*cast(Node*)payload.Data) {

                    // Make sure we don't mask against ourselves as well as don't double mask
                    if (payloadDrawable != node && !node.isMaskedBy(payloadDrawable)) {
                        node.masks ~= MaskBinding(payloadDrawable.uuid, MaskingMode.Mask, payloadDrawable);
                    }
                }
            }
            
            igEndDragDropTarget();
        }

        bool propagateMeshGroup = node.propagateMeshGroup;
        if (igCheckbox(__("Propagate MeshGroup"), &propagateMeshGroup)) {
            node.propagateMeshGroup = propagateMeshGroup;
            long offset = node.parent !is null? node.parent.children.countUntil(node): 0;
            if (node.parent !is null)
                node.reparent(node.parent, offset);
        }
        incTooltip(_("Allow ascendant MeshGroup to deform children of Composite"));

        // Padding
        igSpacing();
        igSpacing();
    }
    incEndCategory();
}

/// Armed Parameter View

void incInspector(ModelEditSubMode mode: ModelEditSubMode.Deform, T: Composite)(T node, Parameter param, vec2u cursor) {
    if (incBeginCategory(__("Composite"))) {
        igBeginGroup();
            igIndent(16);
                // Header for texture options    
                if (incBeginCategory(__("Textures")))  {

                    igPushID(0);
                        incText(_("Tint (Multiply)"));
                        incInspectorDeformColorEdit3(["tint.r", "tint.g", "tint.b"], node, param, cursor);
                    igPopID();
                    
                    igPushID(1);
                        incText(_("Tint (Screen)"));
                        incInspectorDeformColorEdit3(["screenTint.r", "screenTint.g", "screenTint.b"], node, param, cursor);
                    igPopID();
                    

                    // Padding
                    igSeparator();
                    igSpacing();
                    igSpacing();
                }
                incEndCategory();
            igUnindent();
        igEndGroup();

        incText(_("Opacity"));
        incInspectorDeformSliderFloat("###Opacity", "opacity", 0, 1f, node, param, cursor);
        igSpacing();
        igSpacing();
    }
    incEndCategory();
}