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


class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: Composite) : BaseInspector!(mode, T) if (is(T: Composite)) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }

    override
    void run() {
        if (targets.length == 0) return;
        auto node = targets[0];
        if (incBeginCategory(__("Composite"))) {
            

            igSpacing();

            // BLENDING MODE
            import std.conv : text;
            import std.string : toStringz;


            incText(_("Tint (Multiply)"));
            if (_shared!tint(()=>igColorEdit3("###TINT", cast(float[3]*)tint.value.ptr))) {
                tint.apply();
            }

            incText(_("Tint (Screen)"));
            if (_shared!screenTint(()=>igColorEdit3("###S_TINT", cast(float[3]*)screenTint.value.ptr))) {
                screenTint.apply();
            }

            // Header for the Blending options for Parts
            incText(_("Blending"));
            if (_shared!blendingMode(() {
                    auto prevBlendingMode = blendingMode.value;
                    if (igBeginCombo("###Blending", __(blendingMode.value.text))) {

                        // Normal blending mode as used in Photoshop, generally
                        // the default blending mode photoshop starts a layer out as.
                        if (igSelectable(__("Normal"), blendingMode.value == BlendMode.Normal)) blendingMode.value = BlendMode.Normal;
                        
                        // Multiply blending mode, in which this texture's color data
                        // will be multiplied with the color data already in the framebuffer.
                        if (igSelectable(__("Multiply"), blendingMode.value == BlendMode.Multiply)) blendingMode.value = BlendMode.Multiply;
                                        
                        // Screen blending mode
                        if (igSelectable(__("Screen"), blendingMode.value == BlendMode.Screen)) blendingMode.value = BlendMode.Screen;

                        // Overlay blending mode
                        if (igSelectable(__("Overlay"), blendingMode.value == BlendMode.Overlay)) blendingMode.value = BlendMode.Overlay;

                        // Darken blending mode
                        if (igSelectable(__("Darken"), blendingMode.value == BlendMode.Darken)) blendingMode.value = BlendMode.Darken;

                        // Lighten blending mode
                        if (igSelectable(__("Lighten"), blendingMode.value == BlendMode.Lighten)) blendingMode.value = BlendMode.Lighten;
                                
                        // Color Dodge blending mode
                        if (igSelectable(__("Color Dodge"), blendingMode.value == BlendMode.ColorDodge)) blendingMode.value = BlendMode.ColorDodge;
                                
                        // Linear Dodge blending mode
                        if (igSelectable(__("Linear Dodge"), blendingMode.value == BlendMode.LinearDodge)) blendingMode.value = BlendMode.LinearDodge;
                                
                        // Add (Glow) blending mode
                        if (igSelectable(__("Add (Glow)"), blendingMode.value == BlendMode.AddGlow)) blendingMode.value = BlendMode.AddGlow;
                                
                        // Color Burn blending mode
                        if (igSelectable(__("Color Burn"), blendingMode.value == BlendMode.ColorBurn)) blendingMode.value = BlendMode.ColorBurn;
                                
                        // Hard Light blending mode
                        if (igSelectable(__("Hard Light"), blendingMode.value == BlendMode.HardLight)) blendingMode.value = BlendMode.HardLight;
                                
                        // Soft Light blending mode
                        if (igSelectable(__("Soft Light"), blendingMode.value == BlendMode.SoftLight)) blendingMode.value = BlendMode.SoftLight;
                                        
                        // Subtract blending mode
                        if (igSelectable(__("Subtract"), blendingMode.value == BlendMode.Subtract)) blendingMode.value = BlendMode.Subtract;
                                        
                        // Difference blending mode
                        if (igSelectable(__("Difference"), blendingMode.value == BlendMode.Difference)) blendingMode.value = BlendMode.Difference;
                                        
                        // Exclusion blending mode
                        if (igSelectable(__("Exclusion"), blendingMode.value == BlendMode.Exclusion)) blendingMode.value = BlendMode.Exclusion;
                                        
                        // Inverse blending mode
                        if (igSelectable(__("Inverse"), blendingMode.value == BlendMode.Inverse)) blendingMode.value = BlendMode.Inverse;
                        incTooltip(_("Inverts the color by a factor of the overlying color"));
                                        
                        // Destination In blending mode
                        if (igSelectable(__("Destination In"), blendingMode.value == BlendMode.DestinationIn)) blendingMode.value = BlendMode.DestinationIn;
                        
                        igEndCombo();
                    }
                    return prevBlendingMode != blendingMode.value;
                }
            )) {
                blendingMode.apply();
            }

            igSpacing();

            incText(_("Opacity"));
            if (_shared!opacity(()=>igSliderFloat("###Opacity", &opacity.value, 0, 1f, "%0.2f"))) {
                opacity.apply();
                foreach (n; targets)
                    n.notifyChange(n, NotifyReason.AttributeChanged);
            }
            igSpacing();
            igSpacing();

            igTextColored(CategoryTextColor, __("Masks"));
            igSpacing();

            // Threshold slider name for adjusting how transparent a pixel can be
            // before it gets discarded.
            incText(_("Threshold"));
            if (_shared!threshold(()=>igSliderFloat("###Threshold", &threshold.value, 0.0, 1.0, "%.2f"))) {
                threshold.apply();
            }
            
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
            if (ngCheckbox(__("Propagate MeshGroup"), &propagateMeshGroup)) {
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

    mixin MultiEdit;

    mixin(attribute!(vec3, "tint"));
    mixin(attribute!(vec3, "screenTint"));
    mixin(attribute!(BlendMode, "blendingMode"));
    mixin(attribute!(float, "opacity"));
    mixin(attribute!(float, "threshold"));

    override
    void capture(Node[] nodes) {
        super.capture(nodes);
        tint.capture();
        screenTint.capture();
        blendingMode.capture();
        opacity.capture();
        threshold.capture();
    }
}

/// Armed Parameter View

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Deform, T: Composite) : BaseInspector!(mode, T) {
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }

    override
    void run (Parameter param, vec2u cursor) {
        if (targets.length == 0) return;
        auto node = targets[0];

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
}