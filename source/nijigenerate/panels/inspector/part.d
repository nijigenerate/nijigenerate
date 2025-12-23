module nijigenerate.panels.inspector.part;

import nijigenerate;
import nijigenerate.core;
import nijigenerate.panels.inspector.common;
import nijigenerate.widgets;
import nijigenerate.utils;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.commands; // cmd!, Context
import nijigenerate.commands.inspector.apply_node : InspectorNodeApplyCommand;
import nijigenerate.commands.node.mask : NodeMaskCommand;
import nijigenerate.commands.node.welding : NodeWeldingCommand;
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
    this(T[] nodes, ModelEditSubMode subMode) {
        super(nodes, subMode);
    }
    override
    void run() {
        if (targets.length == 0) return;
        auto node = targets[0];
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

            if (targets.length == 1) {
                incInspectorTextureSlot(node, TextureUsage.Albedo, _("Albedo"), elemSize);
                igSameLine(0, 4);
                incInspectorTextureSlot(node, TextureUsage.Emissive, _("Emissive"), elemSize);
                igSameLine(0, 4);
                incInspectorTextureSlot(node, TextureUsage.Bumpmap, _("Bumpmap"), elemSize);
                
                igSpacing();
                igSpacing();
            }

            incText(_("Tint (Multiply)"));
            if (_shared!tint(()=> igColorEdit3("###TINT",cast(float[3]*)tint.value.ptr))) {
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(InspectorNodeApplyCommand.PartTint)(ctx, tint.value);
            }

            incText(_("Tint (Screen)"));
            if (_shared!screenTint(()=>igColorEdit3("###S_TINT", cast(float[3]*)screenTint.value.ptr))) {
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(InspectorNodeApplyCommand.PartScreenTint)(ctx, screenTint.value);
            }

            incText(_("Emission Strength"));
            float strengthPerc = emissionStrength.value*100;
            if (_shared!emissionStrength(()=>igDragFloat("###S_EMISSION", &strengthPerc, 0.1, 0, float.max, "%%.0f%%"))) {
                emissionStrength.value = strengthPerc*0.01;
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(InspectorNodeApplyCommand.PartEmissionStrength)(ctx, emissionStrength.value);
            }

            // Padding
            igSpacing();
            igSpacing();
            igSpacing();

            // Header for the Blending options for Parts
            incText(_("Blending"));
            if (_shared!blendingMode(() {
                    bool result = false;
                    auto prevBlendingMode = blendingMode.value;
                    if (igBeginCombo("###Blending", __(blendingMode.value.text), ImGuiComboFlags.HeightLarge)) {
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
                                        
                        // Clip to Lower blending mode
                        if (igSelectable(__("Clip to Lower"), blendingMode.value == BlendMode.ClipToLower)) blendingMode.value = BlendMode.ClipToLower;
                        incTooltip(_("Special blending mode that causes (while respecting transparency) the part to be clipped to everything underneath"));
                                        
                        // Slice from Lower blending mode
                        if (igSelectable(__("Slice from Lower"), blendingMode.value == BlendMode.SliceFromLower)) blendingMode.value = BlendMode.SliceFromLower;
                        incTooltip(_("Special blending mode that causes (while respecting transparency) the part to be slice by everything underneath.\nBasically reverse Clip to Lower."));
                        igEndCombo();
                    }
                    if (blendingMode.value != prevBlendingMode) result = true;

                    return result;
                }
            ))
            {
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(InspectorNodeApplyCommand.PartBlendingMode)(ctx, blendingMode.value);
            }
 
            igSpacing();

            incText(_("Opacity"));
            if (_shared!opacity(()=>igSliderFloat("###Opacity", &opacity.value, 0, 1f, "%0.2f"))) {
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(InspectorNodeApplyCommand.PartOpacity)(ctx, opacity.value);
            }
            igSpacing();
            igSpacing();

            igTextColored(CategoryTextColor, __("Masks"));
            igSpacing();

            // Threshold slider name for adjusting how transparent a pixel can be
            // before it gets discarded.
            incText(_("Threshold"));
            if (_shared!maskAlphaThreshold(()=>igSliderFloat("###Threshold", &maskAlphaThreshold.value, 0.0, 1.0, "%%.2f"))) {
                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                cmd!(InspectorNodeApplyCommand.PartMaskAlphaThreshold)(ctx, maskAlphaThreshold.value);
            }

            if (DynamicComposite dcomposite = cast(DynamicComposite)node) {
                if (ngCheckbox(__("Resize automatically"), &autoResizedMesh.value)) {
                    auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                    cmd!(InspectorNodeApplyCommand.PartAutoResizedMesh)(ctx, autoResizedMesh.value);
                }
                incTooltip(_("Resize size automatically when child nodes are added or removed. Affect performance severly, not recommended."));
            }
            
            igSpacing();

            if (targets.length == 1) {
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
                                if (igMenuItem(__("Focus"))) {
                                    incFocusCamera(masker.maskSrc);
                                    incSelectNode(masker.maskSrc);
                                }
                                if (igBeginMenu(__("Mode"))) {
                                    auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                                    if (igMenuItem(__("Mask"), null, masker.mode == MaskingMode.Mask)) {
                                        cmd!(NodeMaskCommand.ChangeMaskMode)(ctx, masker.maskSrc, MaskingMode.Mask);
                                    }
                                    if (igMenuItem(__("Dodge"), null, masker.mode == MaskingMode.DodgeMask)) {
                                        cmd!(NodeMaskCommand.ChangeMaskMode)(ctx, masker.maskSrc, MaskingMode.DodgeMask);
                                    }
                                    igEndMenu();
                                }

                                if (igMenuItem(__("Delete"))) {
                                    auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                                    cmd!(NodeMaskCommand.RemoveMask)(ctx, node.masks[i].maskSrc);
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
                                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                                cmd!(NodeMaskCommand.AddMask)(ctx, payloadDrawable, MaskingMode.Mask);
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
                                if (igMenuItem(__("Focus"))) {
                                    incFocusCamera(welded.target);
                                    incSelectNode(welded.target);
                                }

                                if (igMenuItem(__("Delete"))) {
                                    auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                                    cmd!(NodeWeldingCommand.RemoveWelding)(ctx, node.welded[i].target);
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
                                if (igSliderFloat("###weight", &weight, 0, 1f, "%%0.2f")) {
                                    auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                                    cmd!(NodeWeldingCommand.ChangeWeldingWeight)(ctx, welded.target, weight);
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
                                auto ctx = new Context(); ctx.inspectors = [this]; ctx.nodes(cast(Node[])targets);
                                cmd!(NodeWeldingCommand.AddWelding)(ctx, payloadDrawable, 0.5f);
                            }
                        }
                    }
                    
                    igEndDragDropTarget();
                }
                // Padding
                igSpacing();
                igSpacing();
            }
            
        }
        incEndCategory();
    }

    mixin MultiEdit;

    mixin(attribute!(vec3, "tint"));
    mixin(attribute!(vec3, "screenTint"));
    mixin(attribute!(float, "emissionStrength"));
    mixin(attribute!(BlendMode, "blendingMode"));
    mixin(attribute!(float, "opacity"));
    mixin(attribute!(float, "maskAlphaThreshold"));
    mixin(attribute!(bool, "autoResizedMesh", 
        (x) { return "(x) {
            if (auto dcomp = cast(DynamicComposite)x) { 
                return dcomp.autoResizedMesh; 
            } else { 
                return false; 
            } 
        }("~x~")"; },
        (x, v) { return "(x, v) {
            if (auto dcomp = cast(DynamicComposite)x) {
                dcomp.autoResizedMesh = v;
            }
        }("~x~","~v~")"; }));

    override
    void capture(Node[] nodes) {
        super.capture(nodes);
        tint.capture();
        screenTint.capture();
        emissionStrength.capture();
        blendingMode.capture();
        opacity.capture();
        maskAlphaThreshold.capture();
        autoResizedMesh.capture();
    }
}

import nijigenerate.core.math.vertex : position;

ptrdiff_t[] incRegisterWeldedPoints(Drawable node, Drawable counterDrawable, float weight = 0.5) {
    ptrdiff_t[] indices;
    auto counterVerts = counterDrawable.vertices.toArray();
    foreach (i, v; node.vertices) {
        auto vv = (node.transform.matrix * vec4(position(v), 0, 1)).xy;
        auto minDistance = counterVerts.enumerate.minElement!(
            (a)=>((counterDrawable.transform.matrix * vec4(a.value, 0, 1))).xy.distance(vv)
        )();
        auto dist = (counterDrawable.transform.matrix * vec4(minDistance.value, 0, 1)).xy.distance(vv);
        if (dist < 4) {
            indices ~= minDistance.index;
        } else {
            indices ~= -1;
        }
    }
    incActionPush(new DrawableAddWeldingAction(node, counterDrawable, indices, weight));
    return indices;
}

/// Armed Parameter View

class NodeInspector(ModelEditSubMode mode: ModelEditSubMode.Deform, T: Part) : BaseInspector!(mode, T) {
    this(T[] nodes, ModelEditSubMode mode) {
        super(nodes, mode);
    }
    override
    void run(Parameter param, vec2u cursor) {
        if (targets.length == 0) return;
        auto node = targets[0];

        updateDeform(param, cursor);

        if (incBeginCategory(__("Part"))) {
            igBeginGroup();
                igIndent(16);
                    // Header for texture options    
                    if (incBeginCategory(__("Textures")))  {

                        igPushID(0);
                            incText(_("Tint (Multiply)"));
                            __deformRGB!(tintR, tintG, tintB);
                        igPopID();

                        igPushID(1);
                            incText(_("Tint (Screen)"));
                            __deformRGB!(screenTintR, screenTintG, screenTintB);
                        igPopID();

                        incText(_("Emission Strength"));
                        float strengthPerc = emissionStrength.value * 100;
                        if (igDragFloat("###S_EMISSION", &strengthPerc, 0.1, 0, float.max, "%.0f%%")) {
                            emissionStrength.value = strengthPerc * 0.01;
                            emissionStrength.apply();
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
            _deform!opacity((s,v)=>ngInspectorDeformSliderFloat(s, v, 0, 1f));
            igSpacing();
            igSpacing();

            // Threshold slider name for adjusting how transparent a pixel can be
            // before it gets discarded.
            incText(_("Threshold"));
            _deform!alphaThreshold((s,v)=>ngInspectorDeformSliderFloat(s, v, 0.0, 1.0));
        }
        incEndCategory();
    }

    mixin MultiEdit;
    mixin(deformation!("emissionStrength"));
    mixin(deformation!("opacity"));
    mixin(deformation!("alphaThreshold"));
    mixin(deformation!("tintR", "tint.r"));
    mixin(deformation!("tintG", "tint.g"));
    mixin(deformation!("tintB", "tint.b"));
    mixin(deformation!("screenTintR", "screenTint.r"));
    mixin(deformation!("screenTintG", "screenTint.g"));
    mixin(deformation!("screenTintB", "screenTint.b"));

    override
    void capture(Node[] nodes) {
        super.capture(nodes);
        emissionStrength.capture();
        opacity.capture();
        alphaThreshold.capture();
        tintR.capture();
        tintG.capture();
        tintB.capture();
        screenTintR.capture();
        screenTintG.capture();
        screenTintB.capture();
    }

}
