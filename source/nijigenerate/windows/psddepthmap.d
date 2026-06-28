module nijigenerate.windows.psddepthmap;

import bindbc.imgui;
import i18n;
import nijigenerate;
import nijigenerate.commands;
import nijigenerate.commands.depth.map : ngApplyPsdDepthImportResult;
import nijigenerate.ext.nodes.expart;
import nijigenerate.io.depthmap_psd;
import nijigenerate.windows.base;
import nijigenerate.widgets;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import std.algorithm.comparison : max, min;
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
    ptrdiff_t selectedGridIndex;
    Texture[string] originalPreviewTextures;
    Texture[string] depthMaskPreviewTextures;
    Texture[string] compositePreviewTextures;

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
        foreach (key, texture; compositePreviewTextures) {
            if (texture !is null) texture.dispose();
        }
        originalPreviewTextures = null;
        depthMaskPreviewTextures = null;
        compositePreviewTextures = null;
    }

    PsdDepthLayerPreview* findLayerPreview(string layerPath) {
        foreach (ref layerPreview; preview.layerPreviews) {
            if (layerPreview.layerPath == layerPath) return &layerPreview;
        }
        return null;
    }

    PsdDepthLayerMapping* findMapping(string layerPath) {
        foreach (ref mapping; preview.mappings) {
            if (mapping.layerPath == layerPath) return &mapping;
        }
        return null;
    }

    bool hasOtherMappings() {
        foreach (ref mapping; preview.mappings) {
            if (!mapping.matched || mapping.targetGridUuid == 0) return true;
        }
        return false;
    }

    bool isOthersSelected() {
        return selectedGridIndex == cast(ptrdiff_t)preview.grids.length;
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

    Texture compositePreviewTexture(ref PsdDepthGridResult gridResult) {
        if (gridResult.grid is null || gridResult.compositePreviewRgba.length == 0 ||
            gridResult.previewWidth <= 0 || gridResult.previewHeight <= 0) return null;

        auto key = gridResult.grid.uuid.to!string;
        if (auto existing = key in compositePreviewTextures) return *existing;

        auto rgba = gridResult.compositePreviewRgba.dup;
        inTexPremultiply(rgba);
        auto texture = new Texture(rgba, gridResult.previewWidth, gridResult.previewHeight);
        compositePreviewTextures[key] = texture;
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

    void drawLayerPreviewHoverText(string label, string layerPath, bool depthMask) {
        incText(label.length ? label : "-");
        if (igIsItemHovered()) drawLayerPreviewTooltip(layerPath, depthMask);
    }

    ExPart matchedPart(ref PsdDepthLayerMapping mapping) {
        if (mapping.matchedNodeUuid == 0) return null;
        auto puppet = incActivePuppet();
        if (puppet is null || puppet.root is null) return null;
        foreach (part; puppet.findNodesType!ExPart(puppet.root)) {
            if (part.uuid == mapping.matchedNodeUuid) return part;
        }
        return null;
    }

    void drawMatchedNodeTooltip(ref PsdDepthLayerMapping mapping) {
        auto part = matchedPart(mapping);
        if (part is null || part.textures.length == 0 || part.textures[0] is null) return;

        auto texture = part.textures[0];
        igBeginTooltip();
        incText(_("Matched Node Preview"));
        incText(mapping.matchedNodeName);
        auto maxDimension = cast(float)max(texture.width, texture.height);
        auto scale = maxDimension > 0 ? min(PreviewSize / maxDimension, 1.0f) : 1.0f;
        igImage(
            cast(void*)texture.getTextureId(),
            ImVec2(cast(float)texture.width * scale, cast(float)texture.height * scale)
        );
        igEndTooltip();
    }

    void drawMatchedNodeText(ref PsdDepthLayerMapping mapping) {
        incText(mapping.matchedNodeName.length ? mapping.matchedNodeName : "-");
        if (igIsItemHovered()) drawMatchedNodeTooltip(mapping);
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

    void drawCompositePreviewTooltip(ref PsdDepthGridResult gridResult) {
        auto texture = compositePreviewTexture(gridResult);
        if (texture is null) return;

        igBeginTooltip();
        incText(_("Composite Depth Preview"));
        incText("%s  %dx%d  (%d, %d)".format(
            gridResult.grid !is null ? gridResult.grid.name : "-",
            gridResult.previewWidth,
            gridResult.previewHeight,
            gridResult.previewLeft,
            gridResult.previewTop
        ));
        auto maxDimension = cast(float)max(gridResult.previewWidth, gridResult.previewHeight);
        auto scale = maxDimension > 0 ? min(PreviewSize / maxDimension, 1.0f) : 1.0f;
        igImage(
            cast(void*)texture.getTextureId(),
            ImVec2(cast(float)gridResult.previewWidth * scale, cast(float)gridResult.previewHeight * scale)
        );
        igEndTooltip();
    }

    void ensureSelectedGridIndex() {
        auto other = hasOtherMappings();
        if (preview.grids.length == 0 && !other) {
            selectedGridIndex = -1;
            return;
        }
        auto maxIndex = cast(ptrdiff_t)preview.grids.length + (other ? 1 : 0);
        if (selectedGridIndex < 0 || selectedGridIndex >= maxIndex) {
            selectedGridIndex = 0;
        }
    }

    void drawGridList(float height) {
        ensureSelectedGridIndex();
        if (selectedGridIndex < 0) return;
        if (igBeginTable("###PsdDepthGridPreview", 3, ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg | ImGuiTableFlags.ScrollY, ImVec2(0, height))) {
            igTableSetupColumn(__("Use"), ImGuiTableColumnFlags.WidthFixed, 46);
            igTableSetupColumn(__("Preview"), ImGuiTableColumnFlags.WidthFixed, 116);
            igTableSetupColumn(__("GridDeformer"));
            igTableHeadersRow();
            foreach (i, ref gridResult; preview.grids) {
                igTableNextRow();
                igTableNextColumn();
                drawGridEnabledCheckbox(gridResult.grid);
                igTableNextColumn();
                auto texture = compositePreviewTexture(gridResult);
                auto selected = selectedGridIndex == cast(ptrdiff_t)i;
                incTextureSlotUntitled(("###gridPreview" ~ i.to!string), texture, ImVec2(104, 104), 24, ImGuiWindowFlags.NoInputs, selected);
                if (igIsItemHovered()) drawCompositePreviewTooltip(gridResult);
                igTableNextColumn();
                auto label = "%s\n%s: %d  %s: %d\n%s: %.3f  %s: %.3f\n%s".format(
                    gridResult.grid !is null ? gridResult.grid.name : "-",
                    _("Sampled"),
                    cast(int)gridResult.sampledVertices,
                    _("Missing"),
                    cast(int)gridResult.missingVertices,
                    _("Min"),
                    gridResult.minDepth,
                    _("Max"),
                    gridResult.maxDepth,
                    gridResult.skipped ? _("Skipped") : _("Will Apply")
                );
                if (igSelectable((label ~ "###gridRow" ~ i.to!string).toStringz, selected, ImGuiSelectableFlags.SpanAllColumns, ImVec2(0, 104))) {
                    selectedGridIndex = cast(ptrdiff_t)i;
                }
            }
            if (hasOtherMappings()) {
                igTableNextRow();
                igTableNextColumn();
                incText("-");
                igTableNextColumn();
                auto selected = isOthersSelected();
                incTextureSlotUntitled("###gridPreviewOthers", null, ImVec2(104, 104), 24, ImGuiWindowFlags.NoInputs, selected);
                igTableNextColumn();
                if (igSelectable((_("Others") ~ "\n" ~ _("Unmapped or ignored layers") ~ "###gridRowOthers").toStringz,
                    selected, ImGuiSelectableFlags.SpanAllColumns, ImVec2(0, 104))) {
                    selectedGridIndex = cast(ptrdiff_t)preview.grids.length;
                }
            }
            igEndTable();
        }
    }

    void drawGridEnabledCheckbox(GridDeformer grid) {
        if (grid is null) {
            incText("-");
            return;
        }

        auto key = grid.uuid.to!string;
        bool enabled = ngPsdDepthGridEnabled(settings, grid.uuid);
        if (ngCheckbox(("###useGrid" ~ key).toStringz, &enabled)) {
            if (enabled) {
                settings.disabledGridUuids.remove(key);
            } else {
                settings.disabledGridUuids[key] = true;
            }
            previewDirty = true;
        }
    }

    void drawGridLayerEnabledCheckbox(GridDeformer grid, string layerPath) {
        if (grid is null) {
            incText("-");
            return;
        }

        auto key = ngPsdDepthGridLayerKey(grid.uuid, layerPath);
        bool enabled = ngPsdDepthGridLayerEnabled(settings, grid.uuid, layerPath);
        if (ngCheckbox(("###useGridLayer" ~ key).toStringz, &enabled)) {
            if (enabled) {
                settings.disabledGridLayerKeys.remove(key);
            } else {
                settings.disabledGridLayerKeys[key] = true;
            }
            previewDirty = true;
        }
    }

    void drawMappingLayerRow(ref PsdDepthLayerMapping mapping, GridDeformer grid = null) {
        bool problem = !mapping.matched || mapping.ambiguous || mapping.ignored;
        if (onlyProblemLayers && !problem) return;
        igTableNextRow();
        igTableNextColumn();
        drawGridLayerEnabledCheckbox(grid, mapping.layerPath);
        igTableNextColumn();
        drawLayerPreviewHoverText(mapping.layerPath, mapping.layerPath, false);
        igTableNextColumn();
        drawLayerPreviewHoverText(mapping.layerName, mapping.layerPath, true);
        igTableNextColumn();
        drawMatchedNodeText(mapping);
        igTableNextColumn();
        if (drawManualMappingCombo(mapping)) previewDirty = true;
        igTableNextColumn();
        incText(mapping.status);
    }

    void drawSelectedGridLayers(float height) {
        ensureSelectedGridIndex();
        if (selectedGridIndex < 0) return;

        if (isOthersSelected()) {
            incText(_("GridDeformer: Others"));
        } else {
            auto gridResult = preview.grids[selectedGridIndex];
            incText(_("GridDeformer: %s").format(gridResult.grid !is null ? gridResult.grid.name : "-"));
        }

        if (igBeginTable("###PsdDepthGridMasks", 6, ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg | ImGuiTableFlags.ScrollY, ImVec2(0, height))) {
            igTableSetupColumn(__("Use"), ImGuiTableColumnFlags.WidthFixed, 46);
            igTableSetupColumn(__("Path"));
            igTableSetupColumn(__("Depth PSD Layer"));
            igTableSetupColumn(__("Matched Node"));
            igTableSetupColumn(__("Remap"));
            igTableSetupColumn(__("Status"));
            igTableHeadersRow();
            if (isOthersSelected()) {
                foreach (ref mapping; preview.mappings) {
                    if (mapping.matched && mapping.targetGridUuid != 0) continue;
                    drawMappingLayerRow(mapping);
                }
            } else {
                auto gridResult = preview.grids[selectedGridIndex];
                foreach (layerMask; gridResult.layerMasks) {
                    auto mapping = findMapping(layerMask.layerPath);
                    if (mapping is null) continue;
                    drawMappingLayerRow(*mapping, gridResult.grid);
                }
            }
            igEndTable();
        }
    }

    void drawGridPreview(float height) {
        if (preview.grids.length == 0 && !hasOtherMappings()) return;
        auto leftWidth = max(260.0f, incAvailableSpace().x * 0.38f);
        if (igBeginTable("###PsdDepthGridReviewLayout", 2, ImGuiTableFlags.Resizable | ImGuiTableFlags.SizingStretchProp, ImVec2(0, height))) {
            igTableSetupColumn(__("GridDeformers"), ImGuiTableColumnFlags.WidthFixed, leftWidth);
            igTableSetupColumn(__("Depth Layers"));
            igTableNextRow();
            igTableNextColumn();
            drawGridList(height);
            igTableNextColumn();
            drawSelectedGridLayers(height);
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
        float reviewHeight = space.y - footerHeight;
        if (reviewHeight < 120) reviewHeight = 120;

        if (errorMessage.length) {
            incText(errorMessage);
        } else {
            drawGridPreview(reviewHeight);
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
