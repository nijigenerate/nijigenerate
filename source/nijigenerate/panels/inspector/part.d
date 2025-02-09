module nijigenerate.panels.inspector.part;

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

void incInspectorTextureSlot(Part p, TextureUsage usage, string title, ImVec2 elemSize) {
    igPushID(p.uuid);
    igPushID(cast(uint)usage);
        import std.path : baseName, extension, setExtension;
        import std.uni : toLower;
        incTextureSlot(title, p.textures[usage], elemSize);

        void applyTextureToSlot(Part p, TextureUsage usage, string file) {
            switch(file.extension.toLower) {
                case ".png", ".tga", ".jpeg", ".jpg":

                    try {
                        ShallowTexture tex;
                        switch(usage) {
                            case TextureUsage.Albedo:
                                tex = ShallowTexture(file, 4);
                                break;
                            case TextureUsage.Emissive:
                                tex = ShallowTexture(file, 3);
                                break;
                            case TextureUsage.Bumpmap:
                                tex = ShallowTexture(file, 3);
                                break;
                            default:
                                tex = ShallowTexture(file);
                                break;
                        }

                        if (usage != TextureUsage.Albedo) {

                            // Error out if post processing textures don't match
                            if (tex.width != p.textures[0].width || tex.height != p.textures[0].height) {
                                incDialog(__("Error"), _("Size of given texture does not match the Albedo texture."));
                                break;
                            }
                        }

                        if (tex.convChannels == 4) {
                            inTexPremultiply(tex.data);
                        }
                        p.textures[usage] = new Texture(tex);
                        
                        if (usage == TextureUsage.Albedo) {
                            foreach(i, _; p.textures[1..$]) {
                                if (p.textures[i] && (p.textures[i].width != p.textures[0].width || p.textures[i].height != p.textures[0].height)) {
                                    p.textures[i] = null;
                                }
                            }
                        }
                    } catch(Exception ex) {
                        if (ex.msg[0..11] == "unsupported") {
                            incDialog(__("Error"), _("%s is not supported").format(file));
                        } else incDialog(__("Error"), ex.msg);
                    }


                    // We've added new stuff, rescan nodes
                    incActivePuppet().rescanNodes();
                    incActivePuppet().populateTextureSlots();
                    incActivePuppet().updateTextureState();
                    break;
                    
                default:
                    incDialog(__("Error"), _("%s is not supported").format(file)); 
                    break;
            }
        }

        // Only have dropdown if there's actually textures in the slot
        if (p.textures[usage]) {
            igOpenPopupOnItemClick("TEX_OPTIONS");
            if (igBeginPopup("TEX_OPTIONS")) {

                // Allow saving texture to file
                if (igMenuItem(__("Save to File"))) {
                    TFD_Filter[] filters = [
                        {["*.png"], "PNG File"}
                    ];
                    string file = incShowSaveDialog(filters, "texture.png");
                    if (file) {
                        if (file.extension.empty) {
                            file = file.setExtension("png");
                        }
                        p.textures[usage].save(file);
                    }
                }

                // Allow saving texture to file
                if (igMenuItem(__("Load from File"))) {
                    TFD_Filter[] filters = [
                        { ["*.png"], "Portable Network Graphics (*.png)" },
                        { ["*.jpeg", "*.jpg"], "JPEG Image (*.jpeg)" },
                        { ["*.tga"], "TARGA Graphics (*.tga)" }
                    ];

                    string file = incShowImportDialog(filters, _("Import..."));
                    if (file) {
                        applyTextureToSlot(p, usage, file);
                    }
                }

                if (usage != TextureUsage.Albedo) {
                    if (igMenuItem(__("Remove"))) {
                        p.textures[usage] = null;
                        
                        incActivePuppet().rescanNodes();
                        incActivePuppet().populateTextureSlots();
                    }
                } else {
                    // Option which causes the Albedo color to be the emission color.
                    // The item will glow the same color as it, itself is.
                    if (igMenuItem(__("Make Emissive"))) {
                        p.textures[TextureUsage.Emissive] = new Texture(
                            ShallowTexture(
                                p.textures[usage].getTextureData(true),
                                p.textures[usage].width,
                                p.textures[usage].height,
                                4,  // Input is RGBA
                                3   // Output should be RGB only
                            )
                        );

                        incActivePuppet().rescanNodes();
                        incActivePuppet().populateTextureSlots();
                        incActivePuppet().updateTextureState();
                    }
                }

                igEndPopup();
            }
        }

        // FILE DRAG & DROP
        if (igBeginDragDropTarget()) {
            const(ImGuiPayload)* payload = igAcceptDragDropPayload("__PARTS_DROP");
            if (payload !is null) {
                string[] files = *cast(string[]*)payload.Data;
                if (files.length > 0) {
                    applyTextureToSlot(p, usage, files[0]);
                }

                // Finish the file drag
                incFinishFileDrag();
            }

            igEndDragDropTarget();
        }
    igPopID();
    igPopID();
}

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Layout, T: Part) : BaseInspector!(mode, T) {
    override
    void run(T node) {
        if (incBeginCategory(__("Part"))) {
            if (!node.getMesh().isReady()) { 
                igSpacing();
                igTextColored(CategoryTextColor, __("Cannot inspect an unmeshed part"));
                incEndCategory();
                return;
            }
            igSpacing();

            // BLENDING MODE
            import std.conv : text;
            import std.string : toStringz;

            ImVec2 avail = incAvailableSpace();
            float availForTextureSlots = round((avail.x/3.0)-2.0);
            ImVec2 elemSize = ImVec2(availForTextureSlots, availForTextureSlots);

            incInspectorTextureSlot(node, TextureUsage.Albedo, _("Albedo"), elemSize);
            igSameLine(0, 4);
            incInspectorTextureSlot(node, TextureUsage.Emissive, _("Emissive"), elemSize);
            igSameLine(0, 4);
            incInspectorTextureSlot(node, TextureUsage.Bumpmap, _("Bumpmap"), elemSize);
            
            igSpacing();
            igSpacing();

            incText(_("Tint (Multiply)"));
            igColorEdit3("###TINT", cast(float[3]*)node.tint.ptr);

            incText(_("Tint (Screen)"));
            igColorEdit3("###S_TINT", cast(float[3]*)node.screenTint.ptr);

            incText(_("Emission Strength"));
            float strengthPerc = node.emissionStrength*100;
            if (igDragFloat("###S_EMISSION", &strengthPerc, 0.1, 0, float.max, "%.0f%%")) {
                node.emissionStrength = strengthPerc*0.01;
            }

            // Padding
            igSpacing();
            igSpacing();
            igSpacing();

            // Header for the Blending options for Parts
            incText(_("Blending"));
            if (igBeginCombo("###Blending", __(node.blendingMode.text), ImGuiComboFlags.HeightLarge)) {
                auto prevBlendingMode = node.blendingMode;
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
                                
                // Clip to Lower blending mode
                if (igSelectable(__("Clip to Lower"), node.blendingMode == BlendMode.ClipToLower)) node.blendingMode = BlendMode.ClipToLower;
                incTooltip(_("Special blending mode that causes (while respecting transparency) the part to be clipped to everything underneath"));
                                
                // Slice from Lower blending mode
                if (igSelectable(__("Slice from Lower"), node.blendingMode == BlendMode.SliceFromLower)) node.blendingMode = BlendMode.SliceFromLower;
                incTooltip(_("Special blending mode that causes (while respecting transparency) the part to be slice by everything underneath.\nBasically reverse Clip to Lower."));
                
                if (node.blendingMode != prevBlendingMode) {
                    node.notifyChange(node, NotifyReason.AttributeChanged);
                }
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
            igSliderFloat("###Threshold", &node.maskAlphaThreshold, 0.0, 1.0, "%.2f");

            if (DynamicComposite dcomposite = cast(DynamicComposite)node) {
                if (igCheckbox(__("Resize automatically"), &dcomposite.autoResizedMesh)) {
                }
                incTooltip(_("Resize size automatically when child nodes are added or removed. Affect performance severly, not recommended."));
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
                                if (igMenuItem(__("Mask"), null, masker.mode == MaskingMode.Mask)) {
                                    masker.mode = MaskingMode.Mask;
                                    node.notifyChange(node, NotifyReason.AttributeChanged);
                                }
                                if (igMenuItem(__("Dodge"), null, masker.mode == MaskingMode.DodgeMask)) {
                                    masker.mode = MaskingMode.DodgeMask;
                                    node.notifyChange(node, NotifyReason.AttributeChanged);
                                }
                                igEndMenu();
                            }

                            if (igMenuItem(__("Delete"))) {
                                incActionPush(new PartRemoveMaskAction(node.masks[i].maskSrc, node, node.masks[i].mode));
                                node.notifyChange(node, NotifyReason.StructureChanged);
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
                                        node.notifyChange(node, NotifyReason.StructureChanged);
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
                            incActionPush(new PartAddMaskAction(payloadDrawable, node, MaskingMode.Mask));
                        }
                    }
                }
                
                igEndDragDropTarget();
            }

            igSpacing();

            // The sources that the part gets masked by. Depending on the masking mode
            // either the sources will cut out things that don't overlap, or cut out
            // things that do.
            incText(_("Welding"));
            if (igBeginListBox("###Welding", ImVec2(0, 128))) {
                if (node.masks.length == 0) {
                    incText(_("(Drag a Part Here)"));
                }

                foreach(i; 0..node.welded.length) {
                    Drawable.WeldingLink* welded = &node.welded[i];
                    igPushID(cast(int)i);
                        if (igBeginPopup("###WeldedLink")) {

                            if (igMenuItem(__("Delete"))) {
                                incActionPush(new DrawableRemoveWeldingAction(node, node.welded[i].target, node.welded[i].indices, node.welded[i].weight));
                                node.notifyChange(node, NotifyReason.StructureChanged);
                                igEndPopup();
                                igPopID();
                                igEndListBox();
                                incEndCategory();
                                return;
                            }

                            igEndPopup();
                        }

                        igSelectable(welded.target.name.toStringz, false, ImGuiSelectableFlags.AllowItemOverlap, ImVec2(0, 17));
                        if (igIsItemClicked(ImGuiMouseButton.Right)) {
                            igOpenPopup("###WeldedLink");
                        }
                        igSameLine(0, 0);
                        if (igBeginChild("###%s".format(welded.target.name).toStringz, ImVec2(0, 15),false, ImGuiWindowFlags.NoScrollbar|ImGuiWindowFlags.AlwaysAutoResize)) {
                            incDummy(ImVec2(-64, 1));
                            igSameLine(0, 0);
                            auto weight = welded.weight;
                            igPushStyleVar(ImGuiStyleVar.FramePadding, ImVec2(0, 1));
                            igSetNextItemWidth(64);
                            if (igSliderFloat("###weight", &weight, 0, 1f, "%0.2f")) {
                                welded.weight = weight;
                                auto index = welded.target.welded.countUntil!"a.target == b"(node);
                                if (index != -1) {
                                    welded.target.welded[index].weight = 1 - weight;
                                }
                                node.notifyChange(node, NotifyReason.StructureChanged);
                            }
                            igPopStyleVar();
                        }
                        igEndChild();
                        /*
                        if(igBeginDragDropTarget()) {
                            const(ImGuiPayload)* payload = igAcceptDragDropPayload("_WELDINGITEM");
                            if (payload !is null) {
                            }
                            
                            igEndDragDropTarget();
                        }
                        */
                        // TODO: We really should account for left vs. right handedness
                        /*
                        if(igBeginDragDropSource(ImGuiDragDropFlags.SourceAllowNullID)) {
                            igSetDragDropPayload("_WELDINGITEM", cast(void*)welded, Drawable.WeldingLink.sizeof, ImGuiCond.Always);
                            incText(welded.target.name);
                            igEndDragDropSource();
                        }
                        */
                    igPopID();
                }
                igEndListBox();
            }

            if(igBeginDragDropTarget()) {
                const(ImGuiPayload)* payload = igAcceptDragDropPayload("_PUPPETNTREE");
                if (payload !is null) {
                    if (Drawable payloadDrawable = cast(Drawable)*cast(Node*)payload.Data) {

                        // Make sure we don't mask against ourselves as well as don't double mask
                        if (payloadDrawable != node && !node.isWeldedBy(payloadDrawable) && payloadDrawable.vertices.length != 0) {
                            incRegisterWeldedPoints(node, payloadDrawable);
                        }
                    }
                }
                
                igEndDragDropTarget();
            }
            
            // Padding
            igSpacing();
            igSpacing();
        }
        incEndCategory();
    }
}

ptrdiff_t[] incRegisterWeldedPoints(Drawable node, Drawable counterDrawable, float weight = 0.5) {
    ptrdiff_t[] indices;
    foreach (i, v; node.vertices) {
        auto vv = node.transform.matrix * vec4(v, 0, 1);
        auto minDistance = counterDrawable.vertices.enumerate.minElement!((a)=>(counterDrawable.transform.matrix * vec4(a.value, 0, 1)).distance(vv))();
        if ((counterDrawable.transform.matrix * vec4(minDistance[1], 0, 1)).distance(vv) < 4)
            indices ~= minDistance[0];
        else
            indices ~= -1;
    }
    incActionPush(new DrawableAddWeldingAction(node, counterDrawable, indices, weight));
    return indices;
}

/// Armed Parameter View

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Deform, T: Part) : BaseInspector!(mode, T) {
    override
    void run(T node, Parameter param, vec2u cursor) {
        if (incBeginCategory(__("Part"))) {
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

                        incText(_("Emission Strength"));
                        float strengthPerc = incInspectorDeformGetValue(node, param, "emissionStrength", cursor)*100;
                        if (igDragFloat("###S_EMISSION", &strengthPerc, 0.1, 0, float.max, "%.0f%%")) {
                            incInspectorDeformSetValue(node, param, "emissionStrength", cursor, strengthPerc*0.01);
                        }

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

            // Threshold slider name for adjusting how transparent a pixel can be
            // before it gets discarded.
            incText(_("Threshold"));
            incInspectorDeformSliderFloat("###Threshold", "alphaThreshold", 0.0, 1.0, node, param, cursor);
        }
        incEndCategory();
    }
}