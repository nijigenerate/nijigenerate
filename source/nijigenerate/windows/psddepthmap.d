module nijigenerate.windows.psddepthmap;

import bindbc.imgui;
import i18n;
import nijigenerate;
import nijigenerate.commands;
import nijigenerate.commands.depth.map : ngApplyPsdDepthImportResult;
import nijigenerate.io.depthmap_psd;
import nijigenerate.windows.base;
import nijigenerate.widgets;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import std.algorithm.comparison : min;
import std.conv : to;
import std.exception : collectException;
import std.format : format;
import std.string : toStringz;

class PSDDepthMapWindow : Window {
private:
    string path;
    PsdDepthImportSettings settings;
    PsdDepthImportResult preview;
    string errorMessage;
    bool previewDirty = true;
    bool onlyProblemLayers;
    Texture[string] originalPreviewTextures;
    Texture[string] depthMaskPreviewTextures;

    enum PreviewSize = 160f;

    enum string[] ConvolutionNames = [
        "Nearest",
        "Box3x3",
        "Box5x5",
        "Gaussian3x3",
        "Gaussian5x5",
        "Median3x3",
        "Frontmost3x3",
        "Backmost3x3",
        "BoxCustom",
        "GaussianCustom",
        "MedianCustom",
        "FrontmostCustom",
        "BackmostCustom",
    ];

    enum string[] ChannelNames = [
        "AverageRGB",
        "R",
        "G",
        "B",
        "Luminance",
    ];

    enum string[] MissingPolicyNames = [
        "KeepExisting",
        "SetZero",
        "SetBack",
        "SkipGrid",
    ];

    void rebuildPreview() {
        disposePreviewTextures();
        preview = PsdDepthImportResult.init;
        errorMessage = null;
        auto puppet = incActivePuppet();
        if (puppet is null) {
            errorMessage = _("No active puppet");
            previewDirty = false;
            return;
        }
        auto ex = collectException(preview = ngBuildPsdDepthsFromPSD(puppet, path, settings));
        if (ex !is null) {
            errorMessage = ex.msg;
        }
        previewDirty = false;
    }

    void disposePreviewTextures() {
        foreach (key, texture; originalPreviewTextures) {
            if (texture !is null) texture.dispose();
        }
        foreach (key, texture; depthMaskPreviewTextures) {
            if (texture !is null) texture.dispose();
        }
        originalPreviewTextures = null;
        depthMaskPreviewTextures = null;
    }

    PsdDepthLayerPreview* findLayerPreview(string layerPath) {
        foreach (ref layerPreview; preview.layerPreviews) {
            if (layerPreview.layerPath == layerPath) return &layerPreview;
        }
        return null;
    }

    Texture layerPreviewTexture(string layerPath, bool depthMask) {
        auto layerPreview = findLayerPreview(layerPath);
        if (layerPreview is null) return null;
        auto existing = depthMask ? layerPath in depthMaskPreviewTextures : layerPath in originalPreviewTextures;
        if (existing !is null) return *existing;

        auto rgba = depthMask ? layerPreview.depthMaskRgba.dup : layerPreview.originalRgba.dup;
        inTexPremultiply(rgba);
        auto texture = new Texture(rgba, layerPreview.width, layerPreview.height);
        if (depthMask) depthMaskPreviewTextures[layerPath] = texture;
        else originalPreviewTextures[layerPath] = texture;
        return texture;
    }

    void drawLayerPreviewTooltip(string layerPath, bool depthMask) {
        auto layerPreview = findLayerPreview(layerPath);
        if (layerPreview is null) return;
        auto texture = layerPreviewTexture(layerPath, depthMask);
        if (texture is null) return;

        igBeginTooltip();
        incText(depthMask ? _("Depth Mask Preview") : _("Layer Preview"));
        incText("%s  %dx%d  (%d, %d)".format(
            layerPreview.layerName,
            layerPreview.width,
            layerPreview.height,
            layerPreview.left,
            layerPreview.top
        ));
        auto maxDimension = cast(float)(layerPreview.width > layerPreview.height ? layerPreview.width : layerPreview.height);
        auto scale = maxDimension > 0 ? min(PreviewSize / maxDimension, 1.0f) : 1.0f;
        igImage(
            cast(void*)texture.getTextureId(),
            ImVec2(cast(float)layerPreview.width * scale, cast(float)layerPreview.height * scale)
        );
        igEndTooltip();
    }

    void drawLayerPreviewHoverLabel(string label, string layerPath, bool depthMask) {
        incText(label);
        if (igIsItemHovered()) drawLayerPreviewTooltip(layerPath, depthMask);
    }

    bool drawEnumCombo(string label, ref PsdDepthConvolution value) {
        auto current = ngPsdDepthConvolutionName(value);
        bool changed;
        if (igBeginCombo(label.toStringz, current.toStringz)) {
            foreach (name; ConvolutionNames) {
                bool selected = name == current;
                if (igSelectable(name.toStringz, selected)) {
                    value = ngPsdDepthConvolutionFromString(name);
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    bool drawEnumCombo(string label, ref PsdDepthMissingPolicy value) {
        auto current = ngPsdDepthMissingPolicyName(value);
        bool changed;
        if (igBeginCombo(label.toStringz, current.toStringz)) {
            foreach (name; MissingPolicyNames) {
                bool selected = name == current;
                if (igSelectable(name.toStringz, selected)) {
                    value = ngPsdDepthMissingPolicyFromString(name);
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    bool drawEnumCombo(string label, ref PsdDepthChannel value) {
        auto current = ngPsdDepthChannelName(value);
        bool changed;
        if (igBeginCombo(label.toStringz, current.toStringz)) {
            foreach (name; ChannelNames) {
                bool selected = name == current;
                if (igSelectable(name.toStringz, selected)) {
                    value = ngPsdDepthChannelFromString(name);
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    bool isCustomConvolution() {
        final switch (settings.convolution) {
            case PsdDepthConvolution.Nearest:
            case PsdDepthConvolution.Box3x3:
            case PsdDepthConvolution.Box5x5:
            case PsdDepthConvolution.Gaussian3x3:
            case PsdDepthConvolution.Gaussian5x5:
            case PsdDepthConvolution.Median3x3:
            case PsdDepthConvolution.Frontmost3x3:
            case PsdDepthConvolution.Backmost3x3:
                return false;
            case PsdDepthConvolution.BoxCustom:
            case PsdDepthConvolution.GaussianCustom:
            case PsdDepthConvolution.MedianCustom:
            case PsdDepthConvolution.FrontmostCustom:
            case PsdDepthConvolution.BackmostCustom:
                return true;
        }
    }

    GridDeformer[] currentGrids() {
        auto puppet = incActivePuppet();
        if (puppet is null || puppet.root is null) return null;
        return puppet.findNodesType!GridDeformer(puppet.root);
    }

    string currentMappingLabel(ref PsdDepthLayerMapping mapping) {
        if (mapping.layerPath in settings.ignoredLayerPaths) return _("Ignore");
        if (auto overrideUuid = mapping.layerPath in settings.layerTargetGridUuidOverrides) {
            foreach (grid; currentGrids()) {
                if (grid.uuid.to!string == *overrideUuid) return grid.name.length ? grid.name : *overrideUuid;
            }
            return _("Missing Grid");
        }
        return _("Auto");
    }

    bool drawManualMappingCombo(ref PsdDepthLayerMapping mapping) {
        bool changed;
        auto current = currentMappingLabel(mapping);
        if (igBeginCombo(("###map" ~ mapping.layerPath).toStringz, current.toStringz)) {
            bool autoSelected = !(mapping.layerPath in settings.ignoredLayerPaths) &&
                !(mapping.layerPath in settings.layerTargetGridUuidOverrides);
            if (igSelectable(_("Auto").toStringz, autoSelected)) {
                settings.ignoredLayerPaths.remove(mapping.layerPath);
                settings.layerTargetGridUuidOverrides.remove(mapping.layerPath);
                changed = true;
            }
            bool ignoreSelected = (mapping.layerPath in settings.ignoredLayerPaths) !is null;
            if (igSelectable(_("Ignore").toStringz, ignoreSelected)) {
                settings.layerTargetGridUuidOverrides.remove(mapping.layerPath);
                settings.ignoredLayerPaths[mapping.layerPath] = true;
                changed = true;
            }
            igSeparator();
            foreach (grid; currentGrids()) {
                auto uuid = grid.uuid.to!string;
                bool selected = false;
                if (auto overrideUuid = mapping.layerPath in settings.layerTargetGridUuidOverrides) {
                    selected = *overrideUuid == uuid;
                }
                auto label = grid.name.length ? grid.name : uuid;
                if (igSelectable(label.toStringz, selected)) {
                    settings.ignoredLayerPaths.remove(mapping.layerPath);
                    settings.layerTargetGridUuidOverrides[mapping.layerPath] = uuid;
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    void drawOptions() {
        bool changed;
        changed = ngCheckbox(__("Invert Depth"), &settings.invert) || changed;
        incTooltip(_("Default: white is front and black is back."));
        changed = igDragFloat(__("Back Depth"), &settings.backDepth, 0.01f, -10.0f, 10.0f, "%.3f") || changed;
        changed = igDragFloat(__("Front Depth"), &settings.frontDepth, 0.01f, -10.0f, 10.0f, "%.3f") || changed;
        changed = igDragFloat(__("Depth Scale"), &settings.depthScale, 0.01f, 0.0f, 100.0f, "%.3f") || changed;
        incTooltip(_("Imported depth values are multiplied by this scale before applying."));
        changed = drawEnumCombo(_("Channel"), settings.channel) || changed;
        changed = drawEnumCombo(_("Sampling"), settings.convolution) || changed;
        if (isCustomConvolution()) {
            changed = igDragInt(__("Custom Radius"), &settings.customRadius, 0.1f, 1, 64) || changed;
        }
        changed = igDragFloat(__("Alpha Threshold"), &settings.alphaThreshold, 0.001f, 0.0f, 1.0f, "%.3f") || changed;
        changed = drawEnumCombo(_("Missing Vertex Pixel"), settings.missingPolicy) || changed;
        changed = ngCheckbox(__("Direct Grid Name Match"), &settings.matchDirectGridName) || changed;
        incTooltip(_("Also match PSD layer names directly against GridDeformer names."));
        changed = ngCheckbox(__("Only show problem layers"), &onlyProblemLayers) || changed;

        if (changed) previewDirty = true;
    }

    void drawMappings(float height) {
        if (igBeginTable("###PsdDepthMappings", 6, ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg | ImGuiTableFlags.ScrollY, ImVec2(0, height))) {
            igTableSetupColumn(__("PSD Layer"));
            igTableSetupColumn(__("Preview"));
            igTableSetupColumn(__("Matched Node"));
            igTableSetupColumn(__("GridDeformer"));
            igTableSetupColumn(__("Remap"));
            igTableSetupColumn(__("Status"));
            igTableHeadersRow();
            foreach (ref mapping; preview.mappings) {
                bool problem = !mapping.matched || mapping.ambiguous;
                if (onlyProblemLayers && !problem) continue;
                igTableNextRow();
                igTableNextColumn();
                incText(mapping.layerPath);
                igTableNextColumn();
                drawLayerPreviewHoverLabel(_("Layer"), mapping.layerPath, false);
                igSameLine(0, 8);
                drawLayerPreviewHoverLabel(_("Mask"), mapping.layerPath, true);
                igTableNextColumn();
                incText(mapping.matchedNodeName.length ? mapping.matchedNodeName : "-");
                igTableNextColumn();
                incText(mapping.targetGridName.length ? mapping.targetGridName : "-");
                igTableNextColumn();
                if (drawManualMappingCombo(mapping)) previewDirty = true;
                igTableNextColumn();
                incText(mapping.status);
            }
            igEndTable();
        }
    }

    void drawGridPreview() {
        if (preview.grids.length == 0) return;
        if (igBeginTable("###PsdDepthGridPreview", 6, ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg)) {
            igTableSetupColumn(__("GridDeformer"));
            igTableSetupColumn(__("Sampled"));
            igTableSetupColumn(__("Missing"));
            igTableSetupColumn(__("Min"));
            igTableSetupColumn(__("Max"));
            igTableSetupColumn(__("Status"));
            igTableHeadersRow();
            foreach (gridResult; preview.grids) {
                igTableNextRow();
                igTableNextColumn();
                incText(gridResult.grid !is null ? gridResult.grid.name : "-");
                igTableNextColumn();
                incText("%d".format(cast(int)gridResult.sampledVertices));
                igTableNextColumn();
                incText("%d".format(cast(int)gridResult.missingVertices));
                igTableNextColumn();
                incText("%.3f".format(gridResult.minDepth));
                igTableNextColumn();
                incText("%.3f".format(gridResult.maxDepth));
                igTableNextColumn();
                incText(gridResult.skipped ? _("Skipped") : _("Will Apply"));
            }
            igEndTable();
        }

        if (igBeginTable("###PsdDepthGridMasks", 5, ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg)) {
            igTableSetupColumn(__("GridDeformer"));
            igTableSetupColumn(__("Depth Layer"));
            igTableSetupColumn(__("Preview"));
            igTableSetupColumn(__("Sampled Vertices"));
            igTableSetupColumn(__("Selected Vertices"));
            igTableHeadersRow();
            foreach (gridResult; preview.grids) {
                foreach (layerMask; gridResult.layerMasks) {
                    igTableNextRow();
                    igTableNextColumn();
                    incText(gridResult.grid !is null ? gridResult.grid.name : "-");
                    igTableNextColumn();
                    incText(layerMask.layerPath);
                    igTableNextColumn();
                    drawLayerPreviewHoverLabel(_("Layer"), layerMask.layerPath, false);
                    igSameLine(0, 8);
                    drawLayerPreviewHoverLabel(_("Mask"), layerMask.layerPath, true);
                    igTableNextColumn();
                    incText("%d".format(cast(int)layerMask.sampledVertices));
                    igTableNextColumn();
                    incText("%d".format(cast(int)layerMask.selectedVertices));
                }
            }
            igEndTable();
        }
    }

    void apply() {
        if (previewDirty) rebuildPreview();
        if (errorMessage.length) {
            incDialog(__("Error"), errorMessage);
            return;
        }
        auto result = ngApplyPsdDepthImportResult(preview);
        if (!result.succeeded) {
            incDialog(__("Error"), result.message);
            return;
        }
        close();
    }

protected:
    override
    void onBeginUpdate() {
        flags |= ImGuiWindowFlags.NoSavedSettings;

        ImVec2 wpos = ImVec2(
            igGetMainViewport().Pos.x + (igGetMainViewport().Size.x / 2),
            igGetMainViewport().Pos.y + (igGetMainViewport().Size.y / 2),
        );
        ImVec2 uiSize = ImVec2(980, 760);
        igSetNextWindowPos(wpos, ImGuiCond.Appearing, ImVec2(0.5, 0.5));
        igSetNextWindowSize(uiSize, ImGuiCond.Appearing);
        igSetNextWindowSizeConstraints(ImVec2(760, 520), ImVec2(float.max, float.max));
        super.onBeginUpdate();
    }

    override
    void onUpdate() {
        if (previewDirty) rebuildPreview();

        auto space = incAvailableSpace();
        float footerHeight = 220;
        float mappingHeight = space.y - footerHeight;
        if (mappingHeight < 80) mappingHeight = 80;

        if (errorMessage.length) {
            incText(errorMessage);
        } else {
            incText(_("Matched Layers: %d  Unmatched: %d  Ambiguous: %d  Target GridDeformers: %d").format(
                cast(int)preview.matchedLayers,
                cast(int)preview.unmatchedLayers,
                cast(int)preview.ambiguousLayers,
                cast(int)preview.grids.length));
            drawMappings(mappingHeight);
            drawGridPreview();
        }

        igSeparator();
        if (igBeginTable("###PsdDepthOptions", 2, ImGuiTableFlags.SizingStretchProp)) {
            igTableSetupColumn("###Options", ImGuiTableColumnFlags.WidthStretch);
            igTableSetupColumn("###Actions", ImGuiTableColumnFlags.WidthFixed, 112);
            igTableNextRow();
            igTableNextColumn();
            drawOptions();
            igTableNextColumn();
            if (incButtonColored(__("Apply"), ImVec2(104, 26))) {
                apply();
            }
            if (incButtonColored(__("Cancel"), ImVec2(104, 26))) {
                close();
            }
            igEndTable();
        }
    }

    override
    void onClose() {
        disposePreviewTextures();
    }

public:
    this(string path) {
        this.path = path;
        super(_("PSD Depth Map Import"));
    }
}
