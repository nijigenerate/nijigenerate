module nijigenerate.windows.psddepthmap;

import bindbc.imgui;
import i18n;
import nijigenerate;
import nijigenerate.commands;
import nijigenerate.commands.depth.map : ngApplyPsdDepthImportResult;
import nijigenerate.ext.nodes.exdepthmapped : DepthMappedNode;
import nijigenerate.ext.nodes.expart;
import nijigenerate.io : TFD_Filter, incShowImportDialog;
import nijigenerate.io.depthmap_psd;
import nijigenerate.viewport.depth.camera : DepthCamera3D, projectDepthPoint, updateDepthCamera3D;
import nijigenerate.viewport.depth.common.targetview : DepthTargetView;
import nijigenerate.windows.base;
import nijigenerate.widgets;
import nijilive;
import nijilive.core.nodes.deformable : Deformable;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijilive.core.nodes.deformer.path : PathDeformer;
import std.algorithm.comparison : max, min;
import std.algorithm : clamp;
import std.conv : to;
import std.exception : collectException;
import std.format : format;
import std.string : join, toLower, toStringz;

struct PsdDepth3DAdjustGeometryStats {
    size_t layerPlanes;
    size_t depthRangeLines;
    size_t targetWireLines;
    size_t sampledPoints;
    size_t missingPoints;
}

class PSDDepthMapWindow : Window {
private:
    string path;
    PsdDepthImportSettings settings;
    PsdDepthImportResult preview;
    string errorMessage;
    string lastApplyErrorMessage;
    bool previewDirty = true;
    bool onlyProblemLayers;
    ptrdiff_t selectedGridIndex;
    Texture[string] originalPreviewTextures;
    Texture[string] depthMaskPreviewTextures;
    Texture[string] surfaceMaskDiagnosticTextures;
    Texture[string] rawDepthDiagnosticTextures;
    Texture[string] renderMaskDiagnosticTextures;
    Texture[string] rawCompositePreviewTextures;
    Texture threeDAdjustPreviewTexture;
    int threeDAdjustPreviewWidth;
    int threeDAdjustPreviewHeight;
    DepthCamera3D threeDAdjustCamera;
    ulong threeDAdjustCameraTargetUuid;
    int threeDAdjustCameraLeft = int.min;
    int threeDAdjustCameraTop = int.min;
    int threeDAdjustCameraRight = int.min;
    int threeDAdjustCameraBottom = int.min;

    enum PreviewSize = 160f;
    enum PsdDepth3DAdjustMeshStep = 2;
    enum PsdDepth3DAdjustDepthScale = 0.22f;

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
        lastApplyErrorMessage = null;
        auto puppet = incActivePuppet();
        if (puppet is null) {
            errorMessage = _("No active puppet");
            previewDirty = false;
            return;
        }
        auto ex = collectException(preview = ngBuildPsdDepthsFromSource(puppet, path, settings));
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
        foreach (key, texture; surfaceMaskDiagnosticTextures) {
            if (texture !is null) texture.dispose();
        }
        foreach (key, texture; rawDepthDiagnosticTextures) {
            if (texture !is null) texture.dispose();
        }
        foreach (key, texture; renderMaskDiagnosticTextures) {
            if (texture !is null) texture.dispose();
        }
        foreach (key, texture; rawCompositePreviewTextures) {
            if (texture !is null) texture.dispose();
        }
        if (threeDAdjustPreviewTexture !is null) threeDAdjustPreviewTexture.dispose();
        originalPreviewTextures = null;
        depthMaskPreviewTextures = null;
        surfaceMaskDiagnosticTextures = null;
        rawDepthDiagnosticTextures = null;
        renderMaskDiagnosticTextures = null;
        rawCompositePreviewTextures = null;
        threeDAdjustPreviewTexture = null;
        threeDAdjustPreviewWidth = 0;
        threeDAdjustPreviewHeight = 0;
        threeDAdjustCameraLeft = int.min;
        threeDAdjustCameraTop = int.min;
        threeDAdjustCameraRight = int.min;
        threeDAdjustCameraBottom = int.min;
    }

    PsdDepthLayerPreview* findLayerPreview(string layerPath) {
        foreach (ref layerPreview; preview.layerPreviews) {
            if (layerPreview.layerPath == layerPath) return &layerPreview;
        }
        return null;
    }

    PsdDepthComposedLayer* findComposedLayer(string layerPath) {
        foreach (ref layer; preview.composedLayers) {
            if (layer.layerPath == layerPath || layer.id == layerPath || layer.colorLayerPath == layerPath) return &layer;
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
        foreach (ref layer; preview.composedLayers) {
            if (layer.targetGridUuid == 0) return true;
        }
        return false;
    }

    string firstOtherLayerPath() {
        foreach (ref mapping; preview.mappings) {
            if (mapping.matched && mapping.targetGridUuid != 0) continue;
            if (findLayerPreview(mapping.layerPath) !is null) return mapping.layerPath;
        }
        foreach (ref layer; preview.composedLayers) {
            if (layer.targetGridUuid != 0) continue;
            if (findLayerPreview(layer.layerPath) !is null) return layer.layerPath;
        }
        foreach (ref layerPreview; preview.layerPreviews) {
            return layerPreview.layerPath;
        }
        return null;
    }

    Texture firstOtherPreviewTexture() {
        auto layerPath = firstOtherLayerPath();
        return layerPath.length ? layerPreviewTexture(layerPath, true) : null;
    }

    bool isOthersSelected() {
        return selectedGridIndex == cast(ptrdiff_t)preview.grids.length;
    }

    string[] diagnostics() {
        string[] lines;
        if (errorMessage.length) {
            lines ~= _("Preview failed: %s").format(errorMessage);
            return lines;
        }
        if (lastApplyErrorMessage.length) {
            lines ~= _("GPU/apply failed: %s").format(lastApplyErrorMessage);
        }
        if (preview.compositionModeName.length) {
            lines ~= _("Composition: %s  %dx%d  color layers: %d  depth layers: %d  composed layers: %d  scale: %.3f").format(
                preview.compositionModeName,
                preview.compositionWidth,
                preview.compositionHeight,
                cast(int)preview.colorLayerCount,
                cast(int)preview.sourceDepthLayerCount,
                cast(int)preview.composedLayerCount,
                preview.globalDepthScale
            );
        }
        if (preview.colorSource.kindName.length || preview.depthSource.kindName.length) {
            lines ~= _("Sources: color=%s(%d) depth=%s(%d)").format(
                preview.colorSource.kindName.length ? preview.colorSource.kindName : "-",
                cast(int)preview.colorSource.layers.length,
                preview.depthSource.kindName.length ? preview.depthSource.kindName : "-",
                cast(int)preview.depthSource.layers.length
            );
        }
        foreach (diagnostic; preview.compositionDiagnostics) {
            auto subject = diagnostic.layerPath.length ? diagnostic.layerPath : diagnostic.layerName;
            if (subject.length) {
                lines ~= _("%s: %s").format(diagnostic.type, subject);
            } else if (diagnostic.message.length) {
                lines ~= _("%s: %s").format(diagnostic.type, diagnostic.message);
            } else {
                lines ~= diagnostic.type;
            }
        }
        if (preview.layerPreviews.length == 0) {
            lines ~= _("No source layers were loaded from the selected depth source.");
        }
        if (preview.mappings.length > 0 && preview.grids.length == 0) {
            lines ~= _("No mapped targets were found. Remap a source layer to a GridDeformer or PathDeformer.");
        }
        foreach (mapping; preview.mappings) {
            if (mapping.status == "UnmatchedWithoutGrid" || mapping.status == "AmbiguousWithoutGrid") {
                lines ~= _("Unsupported target type for layer: %s").format(mapping.layerPath);
            } else if (mapping.status == "UnmatchedManualGrid") {
                lines ~= _("Mapped target is missing for layer: %s").format(mapping.layerPath);
            }
        }
        if (preview.gpuCompositionRequested) {
            lines ~= _("GPU Composition is selected. Apply must submit GPU work; CPU fallback is disabled.");
        }
        return lines;
    }

    void drawDiagnostics() {
        auto lines = diagnostics();
        foreach (line; lines) incText(line);
    }

    Texture layerPreviewTexture(string layerPath, bool depthMask) {
        auto layerPreview = findLayerPreview(layerPath);
        if (layerPreview is null) return null;
        auto existing = depthMask ? layerPath in depthMaskPreviewTextures : layerPath in originalPreviewTextures;
        if (existing !is null) return *existing;

        auto rgba = depthMask ? layerPreview.depthMaskRgba.dup : layerPreview.originalRgba.dup;
        auto textureWidth = depthMask || layerPreview.originalWidth <= 0 ? layerPreview.width : layerPreview.originalWidth;
        auto textureHeight = depthMask || layerPreview.originalHeight <= 0 ? layerPreview.height : layerPreview.originalHeight;
        inTexPremultiply(rgba);
        auto texture = new Texture(rgba, textureWidth, textureHeight);
        if (depthMask) depthMaskPreviewTextures[layerPath] = texture;
        else originalPreviewTextures[layerPath] = texture;
        return texture;
    }

    Texture compositePreviewTexture(ref PsdDepthGridResult gridResult) {
        auto source = gridResult.rawCompositePreviewRgba;
        if (gridResult.grid is null || source.length == 0 ||
            gridResult.previewWidth <= 0 || gridResult.previewHeight <= 0) return null;

        auto key = gridResult.grid.uuid.to!string;
        auto existing = key in rawCompositePreviewTextures;
        if (existing) return *existing;

        auto rgba = source.dup;
        inTexPremultiply(rgba);
        auto texture = new Texture(rgba, gridResult.previewWidth, gridResult.previewHeight);
        rawCompositePreviewTextures[key] = texture;
        return texture;
    }

    Texture diagnosticLayerTexture(string layerPath, string kind) {
        auto layerPreview = findLayerPreview(layerPath);
        if (layerPreview is null || layerPreview.width <= 0 || layerPreview.height <= 0) return null;

        Texture[string]* store;
        if (kind == "surface") {
            store = &surfaceMaskDiagnosticTextures;
        } else if (kind == "depth") {
            store = &rawDepthDiagnosticTextures;
        } else if (kind == "render") {
            store = &renderMaskDiagnosticTextures;
        } else {
            return null;
        }

        auto existing = layerPath in *store;
        if (existing !is null) return *existing;

        auto pixelCount = cast(size_t)layerPreview.width * cast(size_t)layerPreview.height;
        ubyte[] rgba;
        rgba.length = pixelCount * 4;
        foreach (i; 0 .. pixelCount) {
            auto index = i * 4;
            ubyte gray;
            if (kind == "surface") {
                if (index + 3 < layerPreview.originalRgba.length && layerPreview.originalRgba[index + 3] >= 3) {
                    rgba[index + 0] = layerPreview.originalRgba[index + 0];
                    rgba[index + 1] = layerPreview.originalRgba[index + 1];
                    rgba[index + 2] = layerPreview.originalRgba[index + 2];
                    rgba[index + 3] = 255;
                    continue;
                }
            } else if (kind == "depth") {
                if (index < layerPreview.depthRgba.length) {
                    gray = layerPreview.depthRgba[index];
                }
            } else {
                if (index < layerPreview.depthMaskRgba.length) {
                    gray = layerPreview.depthMaskRgba[index];
                }
            }
            rgba[index + 0] = gray;
            rgba[index + 1] = gray;
            rgba[index + 2] = gray;
            rgba[index + 3] = 255;
        }

        auto texture = new Texture(rgba, layerPreview.width, layerPreview.height);
        (*store)[layerPath] = texture;
        return texture;
    }

    void drawDiagnosticLayerImage(string label, string layerPath, string kind, float size) {
        auto layerPreview = findLayerPreview(layerPath);
        auto texture = diagnosticLayerTexture(layerPath, kind);
        if (layerPreview is null || texture is null) return;

        incText(label);
        auto maxDimension = cast(float)max(layerPreview.width, layerPreview.height);
        auto scale = maxDimension > 0 ? min(size / maxDimension, 1.0f) : 1.0f;
        igImage(
            cast(void*)texture.getTextureId(),
            ImVec2(cast(float)layerPreview.width * scale, cast(float)layerPreview.height * scale)
        );
        if (igIsItemHovered()) {
            igBeginTooltip();
            incText(label);
            incText("%s  %dx%d  (%d, %d)".format(
                layerPreview.layerName,
                layerPreview.width,
                layerPreview.height,
                layerPreview.left,
                layerPreview.top
            ));
            auto tooltipScale = maxDimension > 0 ? min(PreviewSize / maxDimension, 1.0f) : 1.0f;
            igImage(
                cast(void*)texture.getTextureId(),
                ImVec2(cast(float)layerPreview.width * tooltipScale, cast(float)layerPreview.height * tooltipScale)
            );
            igEndTooltip();
        }
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

    Deformable[] currentTargets() {
        auto puppet = incActivePuppet();
        if (puppet is null || puppet.root is null) return null;
        Deformable[] targets;
        foreach (grid; puppet.findNodesType!GridDeformer(puppet.root)) targets ~= grid;
        foreach (path; puppet.findNodesType!PathDeformer(puppet.root)) targets ~= path;
        return targets;
    }

    string currentMappingLabel(ref PsdDepthLayerMapping mapping) {
        if (mapping.layerPath in settings.ignoredLayerPaths) return _("Ignore");
        if (auto overrideUuid = mapping.layerPath in settings.layerTargetGridUuidOverrides) {
            foreach (grid; currentTargets()) {
                if (grid.uuid.to!string == *overrideUuid) return grid.name.length ? grid.name : *overrideUuid;
            }
            return _("Missing Target");
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
            foreach (grid; currentTargets()) {
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
        incText(_("Color Source"));
        igSameLine();
        auto colorLabel = settings.colorSourcePath.length ? settings.colorSourcePath : _("<active target/art>");
        incText("%s".format(colorLabel));
        igSameLine();
        TFD_Filter[] colorFilters = [
            { ["*.png", "*.psd"], "Color source (*.png, *.psd)" },
            { ["*.png"], "Portable Network Graphics (*.png)" },
            { ["*.psd"], "Photoshop Document (*.psd)" }
        ];
        if (igButton(__("Select Color Source"))) {
            auto colorPath = incShowImportDialog(colorFilters, _("Select Color Source..."));
            if (colorPath.length) {
                settings.colorSourcePath = colorPath;
                changed = true;
            }
        }
        if (settings.colorSourcePath.length) {
            igSameLine();
            if (igButton(__("Clear Color Source"))) {
                settings.colorSourcePath = null;
                changed = true;
            }
        }
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
        changed = ngCheckbox(__("GPU Composition"), &settings.useGpuComposition) || changed;
        incTooltip(_("When enabled, apply must use the GPU composition path. CPU fallback is treated as an error."));
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
        incText(_("Raw PSD Depth"));
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
            igTableSetupColumn(__("Target"));
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
                auto label = "%s\n%s: %d  %s: %d\n%s: %d\n%s: %.3f  %s: %.3f\n%s".format(
                    gridResult.grid !is null ? gridResult.grid.name : "-",
                    _("Sampled"),
                    cast(int)gridResult.sampledVertices,
                    _("Missing"),
                    cast(int)gridResult.missingVertices,
                    "Coverage",
                    cast(int)gridResult.coverageSources,
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
                auto texture = firstOtherPreviewTexture();
                incTextureSlotUntitled("###gridPreviewOthers", texture, ImVec2(104, 104), 24, ImGuiWindowFlags.NoInputs, selected);
                if (igIsItemHovered()) {
                    auto layerPath = firstOtherLayerPath();
                    if (layerPath.length) drawLayerPreviewTooltip(layerPath, true);
                }
                igTableNextColumn();
                if (igSelectable((_("Others") ~ "\n" ~ _("Unmapped or ignored layers") ~ "###gridRowOthers").toStringz,
                    selected, ImGuiSelectableFlags.SpanAllColumns, ImVec2(0, 104))) {
                    selectedGridIndex = cast(ptrdiff_t)preview.grids.length;
                }
            }
            igEndTable();
        }
    }

    void drawGridEnabledCheckbox(Deformable grid) {
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

    void drawComposedLayerEnabledCheckbox(string layerPath) {
        bool enabled = ngPsdDepthComposedLayerEnabled(settings, layerPath);
        if (ngCheckbox(("###useComposedLayer" ~ layerPath).toStringz, &enabled)) {
            if (enabled) {
                settings.disabledComposedLayerPaths.remove(layerPath);
                settings.hiddenComposedLayerPaths.remove(layerPath);
            } else {
                settings.disabledComposedLayerPaths[layerPath] = true;
            }
            previewDirty = true;
        }
    }

    PsdDepthLayerTransform layerTransform(string layerPath) {
        return ngPsdDepthLayerTransform(settings, layerPath);
    }

    void storeLayerTransform(string layerPath, PsdDepthLayerTransform transform, bool changed) {
        if (!changed) return;
        settings.layerTransforms[layerPath] = transform;
        previewDirty = true;
    }

    void drawLayerXYOffsetControls(string layerPath) {
        auto transform = ngPsdDepthLayerTransform(settings, layerPath);
        bool changed;
        igSetNextItemWidth(84);
        changed = igDragFloat(("###xyOffsetX" ~ layerPath).toStringz,
            &transform.xyOffsetX, 0.1f, -100000.0f, 100000.0f, "%.2f") || changed;
        igSameLine(0, 4);
        igSetNextItemWidth(84);
        changed = igDragFloat(("###xyOffsetY" ~ layerPath).toStringz,
            &transform.xyOffsetY, 0.1f, -100000.0f, 100000.0f, "%.2f") || changed;
        storeLayerTransform(layerPath, transform, changed);
    }

    void drawLayerXYScaleControls(string layerPath) {
        auto transform = ngPsdDepthLayerTransform(settings, layerPath);
        bool changed;
        igSetNextItemWidth(84);
        changed = igDragFloat(("###xyScaleX" ~ layerPath).toStringz,
            &transform.xyScaleX, 0.01f, 0.001f, 1000.0f, "%.3f") || changed;
        igSameLine(0, 4);
        igSetNextItemWidth(84);
        changed = igDragFloat(("###xyScaleY" ~ layerPath).toStringz,
            &transform.xyScaleY, 0.01f, 0.001f, 1000.0f, "%.3f") || changed;
        storeLayerTransform(layerPath, transform, changed);
    }

    void drawLayerZControls(string layerPath) {
        auto transform = ngPsdDepthLayerTransform(settings, layerPath);
        bool changed;
        igSetNextItemWidth(84);
        changed = igDragFloat(("###zOffset" ~ layerPath).toStringz,
            &transform.zOffset, 0.01f, -100.0f, 100.0f, "%.3f") || changed;
        igSameLine(0, 4);
        igSetNextItemWidth(84);
        changed = igDragFloat(("###zScale" ~ layerPath).toStringz,
            &transform.zScale, 0.01f, -100.0f, 100.0f, "%.3f") || changed;

        changed = ngCheckbox(("###invertLayer" ~ layerPath).toStringz, &transform.invert) || changed;
        igSameLine(0, 8);
        if (igButton((_("Reset") ~ "###resetTransform" ~ layerPath).toStringz, ImVec2(72, 0))) {
            transform = PsdDepthLayerTransform();
            changed = true;
        }
        storeLayerTransform(layerPath, transform, changed);
    }

    bool fitLayerBoundsToTarget(ref PsdDepthGridResult gridResult, string layerPath) {
        auto previewLayer = findLayerPreview(layerPath);
        if (previewLayer is null || gridResult.grid is null) return false;
        auto targetView = new DepthTargetView(gridResult.grid);
        auto targetVertices = targetView.getVertices();
        if (previewLayer.width <= 0 || previewLayer.height <= 0 || targetVertices.length == 0) return false;

        bool hasBounds;
        float minX;
        float maxX;
        float minY;
        float maxY;
        foreach (vertex; targetVertices) {
            auto documentPoint = ngPsdDepthTargetPointDocumentPosition(
                gridResult.grid,
                vertex,
                gridResult.documentWidth,
                gridResult.documentHeight
            );
            if (!hasBounds) {
                minX = maxX = documentPoint.x;
                minY = maxY = documentPoint.y;
                hasBounds = true;
            } else {
                minX = min(minX, documentPoint.x);
                maxX = max(maxX, documentPoint.x);
                minY = min(minY, documentPoint.y);
                maxY = max(maxY, documentPoint.y);
            }
        }
        if (!hasBounds || maxX <= minX || maxY <= minY) return false;

        auto transform = ngPsdDepthLayerTransform(settings, layerPath);
        transform.xyScaleX = (maxX - minX) / cast(float)previewLayer.width;
        transform.xyScaleY = (maxY - minY) / cast(float)previewLayer.height;
        transform.xyOffsetX = minX - cast(float)previewLayer.left;
        transform.xyOffsetY = minY - cast(float)previewLayer.top;
        settings.layerTransforms[layerPath] = transform;
        previewDirty = true;
        return true;
    }

    void drawMappingLayerRow(ref PsdDepthLayerMapping mapping, Deformable grid = null, PsdDepthGridLayerMask* layerMask = null) {
        bool problem = !mapping.matched || mapping.ambiguous || mapping.ignored;
        if (onlyProblemLayers && !problem) return;
        auto previewLayer = findLayerPreview(mapping.layerPath);
        igTableNextRow();
        igTableNextColumn();
        drawComposedLayerEnabledCheckbox(mapping.layerPath);
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
        igTableNextColumn();
        incText(layerMask !is null ? layerMask.sampledVertices.to!string : "-");
        igTableNextColumn();
        incText(layerMask !is null ? layerMask.selectedVertices.to!string : "-");
        igTableNextColumn();
        incText(previewLayer !is null ? previewLayer.depthStats.maskedPixels.to!string : "-");
        igTableNextColumn();
        incText(previewLayer !is null ? previewLayer.depthStats.zeroPixels.to!string : "-");
        igTableNextColumn();
        incText(previewLayer !is null && previewLayer.depthStats.hasDepth ?
            "%.3f".format(previewLayer.depthStats.rangeDepth01) : "-");
    }

    void drawSelectedGridLayers(float height) {
        ensureSelectedGridIndex();
        if (selectedGridIndex < 0) return;

        if (isOthersSelected()) {
            incText(_("Target: Others"));
        } else {
            auto gridResult = preview.grids[selectedGridIndex];
            incText(_("Target: %s").format(gridResult.grid !is null ? gridResult.grid.name : "-"));
        }

        if (igBeginTable("###PsdDepthGridMasks", 11, ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg | ImGuiTableFlags.ScrollY, ImVec2(0, height))) {
            igTableSetupColumn(__("Use"), ImGuiTableColumnFlags.WidthFixed, 46);
            igTableSetupColumn(__("Path"));
            igTableSetupColumn(__("Depth Layer"));
            igTableSetupColumn(__("Matched Node"));
            igTableSetupColumn(__("Remap"));
            igTableSetupColumn(__("Status"));
            igTableSetupColumn(__("Sampled"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Selected"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Mask px"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Zero px"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Range"), ImGuiTableColumnFlags.WidthFixed, 64);
            igTableHeadersRow();
            if (isOthersSelected()) {
                foreach (ref mapping; preview.mappings) {
                    if (mapping.matched && mapping.targetGridUuid != 0) continue;
                    drawMappingLayerRow(mapping);
                }
            } else {
                auto gridResult = preview.grids[selectedGridIndex];
                foreach (ref layerMask; gridResult.layerMasks) {
                    auto mapping = findMapping(layerMask.layerPath);
                    if (mapping is null) continue;
                    drawMappingLayerRow(*mapping, gridResult.grid, &layerMask);
                }
            }
            igEndTable();
        }
    }

    void drawComposedSourceLayers(float height) {
        if (preview.composedLayers.length == 0) return;
        incText(_("Composed Source Layers"));
        if (igBeginTable("###PsdDepthComposedSourceLayers", 8,
            ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg | ImGuiTableFlags.ScrollY,
            ImVec2(0, height))) {
            igTableSetupColumn(__("Use"), ImGuiTableColumnFlags.WidthFixed, 46);
            igTableSetupColumn(__("Preview"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Source Layer"));
            igTableSetupColumn(__("Target"));
            igTableSetupColumn(__("Mask px"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Zero px"), ImGuiTableColumnFlags.WidthFixed, 72);
            igTableSetupColumn(__("Range"), ImGuiTableColumnFlags.WidthFixed, 64);
            igTableSetupColumn(__("Status"), ImGuiTableColumnFlags.WidthFixed, 92);
            igTableHeadersRow();
            foreach (i, ref layer; preview.composedLayers) {
                auto layerPath = layer.layerPath.length ? layer.layerPath : layer.id;
                igTableNextRow();
                igTableNextColumn();
                drawComposedLayerEnabledCheckbox(layerPath);
                igTableNextColumn();
                auto texture = layerPreviewTexture(layerPath, true);
                incTextureSlotUntitled(("###composedSourceLayerPreview" ~ i.to!string), texture,
                    ImVec2(60, 60), 18, ImGuiWindowFlags.NoInputs, false);
                if (igIsItemHovered() && texture !is null) drawLayerPreviewTooltip(layerPath, true);
                igTableNextColumn();
                drawLayerPreviewHoverText(layer.colorLayerName.length ? layer.colorLayerName : layer.layerName,
                    layerPath, true);
                igTableNextColumn();
                incText(layer.targetGridName.length ? layer.targetGridName : "-");
                igTableNextColumn();
                incText(layer.depthStats.maskedPixels.to!string);
                igTableNextColumn();
                incText(layer.depthStats.zeroPixels.to!string);
                igTableNextColumn();
                incText(layer.depthStats.hasDepth ? "%.3f".format(layer.depthStats.rangeDepth01) : "-");
                igTableNextColumn();
                incText(layer.enabled ? (layer.targetGridUuid != 0 ? _("Bound") : _("Unbound")) : _("Disabled"));
            }
            igEndTable();
        }
    }

    void drawGridPreview(float height) {
        if (preview.grids.length == 0 && !hasOtherMappings()) return;
        auto leftWidth = max(260.0f, incAvailableSpace().x * 0.38f);
        if (igBeginTable("###PsdDepthGridReviewLayout", 2, ImGuiTableFlags.Resizable | ImGuiTableFlags.SizingStretchProp, ImVec2(0, height))) {
            igTableSetupColumn(__("Targets"), ImGuiTableColumnFlags.WidthFixed, leftWidth);
            igTableSetupColumn(__("Depth Layers"));
            igTableNextRow();
            igTableNextColumn();
            drawGridList(height);
            igTableNextColumn();
            drawSelectedGridLayers(height);
            igEndTable();
        }
    }

    void drawSourceMappingTab(float height) {
        auto lines = diagnostics();
        if (lines.length) {
            drawDiagnostics();
            if (errorMessage.length) return;
            igSeparator();
        }
        if (preview.grids.length == 0 && !hasOtherMappings() && preview.layerPreviews.length == 0) {
            incText(_("No mapped targets were found. Remap a source layer to a GridDeformer or PathDeformer."));
            return;
        }
        if (preview.composedLayers.length > 0) {
            auto sourceHeight = min(220.0f, max(120.0f, height * 0.34f));
            drawComposedSourceLayers(sourceHeight);
            igSeparator();
            drawGridPreview(max(120.0f, height - sourceHeight - 16.0f));
        } else {
            drawGridPreview(height);
        }
    }

    void draw3DAdjustLayerControlsPanel() {
        size_t countRgbaAlphaPixels(const(ubyte)[] rgba) {
            size_t count;
            foreach (i; 0 .. rgba.length / 4) {
                if (rgba[i * 4 + 3] >= 3) count++;
            }
            return count;
        }

        size_t countDepthPixels(const(ubyte)[] rgba, bool requireAlpha) {
            size_t count;
            foreach (i; 0 .. rgba.length / 4) {
                auto index = i * 4;
                if (requireAlpha && rgba[index + 3] == 0) continue;
                if (rgba[index] > 0) count++;
            }
            return count;
        }

        void countSurfaceDepthOverlap(ref PsdDepthLayerPreview layerPreview, out size_t surface, out size_t depth, out size_t overlap) {
            auto pixels = min(layerPreview.originalRgba.length, layerPreview.depthRgba.length) / 4;
            foreach (i; 0 .. pixels) {
                auto index = i * 4;
                auto hasSurface = layerPreview.originalRgba[index + 3] >= 3;
                auto hasDepth = layerPreview.depthRgba[index] > 0;
                if (hasSurface) surface++;
                if (hasDepth) depth++;
                if (hasSurface && hasDepth) overlap++;
            }
        }

        size_t countFlippedYOverlap(ref PsdDepthLayerPreview layerPreview) {
            size_t overlap;
            auto pixels = min(layerPreview.originalRgba.length, layerPreview.flippedDepthRgba.length) / 4;
            foreach (i; 0 .. pixels) {
                auto index = i * 4;
                if (layerPreview.originalRgba[index + 3] >= 3 && layerPreview.flippedDepthRgba[index] > 0) overlap++;
            }
            return overlap;
        }

        incText("%s: %d".format(_("Layers"), cast(int)preview.layerPreviews.length));
        incText("%s: %s".format(_("Composition"), preview.compositionModeName));
        igSeparator();

        foreach (ref layerPreview; preview.layerPreviews) {
            drawLayerPreviewHoverText(layerPreview.layerName, layerPreview.layerPath, true);
            incText("%s: %dx%d  (%d, %d)".format(
                _("Layer"),
                layerPreview.width,
                layerPreview.height,
                layerPreview.left,
                layerPreview.top
            ));
            size_t surfaceCount;
            size_t depthCount;
            size_t overlapCount;
            countSurfaceDepthOverlap(layerPreview, surfaceCount, depthCount, overlapCount);
            auto flippedYOverlap = countFlippedYOverlap(layerPreview);
            incText("3D: c %d/%d dm %d raw %d ov %d yf %d img %dx%d".format(
                cast(int)surfaceCount,
                cast(int)(layerPreview.originalRgba.length / 4),
                cast(int)countDepthPixels(layerPreview.depthMaskRgba, true),
                cast(int)depthCount,
                cast(int)overlapCount,
                cast(int)flippedYOverlap,
                layerPreview.originalWidth,
                layerPreview.originalHeight
            ));
            if (igBeginTable(("###PsdDepth3DDiag" ~ layerPreview.layerPath).toStringz, 3,
                ImGuiTableFlags.SizingFixedFit)) {
                igTableNextRow();
                igTableNextColumn();
                drawDiagnosticLayerImage("color", layerPreview.layerPath, "surface", 72.0f);
                igTableNextColumn();
                drawDiagnosticLayerImage("depth", layerPreview.layerPath, "depth", 72.0f);
                igTableNextColumn();
                drawDiagnosticLayerImage("render", layerPreview.layerPath, "render", 72.0f);
                igEndTable();
            }

            drawComposedLayerEnabledCheckbox(layerPreview.layerPath);
            incText(_("XY Offset"));
            drawLayerXYOffsetControls(layerPreview.layerPath);
            incText(_("XY Scale"));
            drawLayerXYScaleControls(layerPreview.layerPath);
            incText(_("Z / Invert"));
            drawLayerZControls(layerPreview.layerPath);
            igSeparator();
        }
    }

    void resetPsdDepth3DAdjustCameraToBounds(float sourceWidth, float sourceHeight, ImVec2 canvasSize) {
        threeDAdjustCamera.yaw = 0.42f;
        threeDAdjustCamera.pitch = 0.35f;
        threeDAdjustCamera.zoom = min(
            (canvasSize.x - 80.0f) / max(1.0f, sourceWidth),
            (canvasSize.y - 80.0f) / max(1.0f, sourceHeight)
        );
        threeDAdjustCamera.zoom = clamp(threeDAdjustCamera.zoom, 0.1f, 8.0f);
        threeDAdjustCamera.pan = vec2(0);
    }

    void draw3DAdjustRelationshipCanvas(float height) {
        auto space = incAvailableSpace();
        auto canvasHeight = max(260.0f, height - 8.0f);
        auto canvasSize = ImVec2(max(320.0f, space.x), canvasHeight);
        ImVec2 origin;
        igGetCursorScreenPos(&origin);
        auto drawList = igGetWindowDrawList();
        auto bg = igGetColorU32(ImVec4(0.08f, 0.09f, 0.10f, 1.0f));
        auto border = igGetColorU32(ImVec4(0.45f, 0.48f, 0.50f, 1.0f));
        auto textureTint = igGetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        auto canvasMax = ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y);
        ImDrawList_AddRectFilled(drawList, origin, canvasMax, bg, 4.0f);
        ImDrawList_AddRect(drawList, origin, canvasMax, border, 4.0f, ImDrawFlags.None, 1.0f);
        igInvisibleButton("###psdDepth3DAdjustViewport", canvasSize);
        auto io = igGetIO();
        if (igIsItemHovered()) {
            auto usesWheel = io.MouseWheel != 0;
            updateDepthCamera3D(
                threeDAdjustCamera,
                io,
                io.MouseDown[1] && !io.KeyShift,
                (io.MouseDown[1] && io.KeyShift) || io.MouseDown[2],
                usesWheel
            );
            if (usesWheel) igSetItemUsingMouseWheel();
        }

        int left;
        int top;
        int right;
        int bottom;
        bool hasBounds;
        void includeBounds(int x0, int y0, int x1, int y1) {
            if (!hasBounds) {
                left = x0;
                top = y0;
                right = x1;
                bottom = y1;
                hasBounds = true;
            } else {
                left = min(left, x0);
                top = min(top, y0);
                right = max(right, x1);
                bottom = max(bottom, y1);
            }
        }
        bool hasRenderPixels(ref PsdDepthLayerPreview layerPreview) {
            foreach (i; 0 .. layerPreview.depthMaskRgba.length / 4) {
                auto index = i * 4;
                if (layerPreview.depthMaskRgba[index] > 0) return true;
            }
            return false;
        }
        foreach (ref layerPreview; preview.layerPreviews) {
            if (!hasRenderPixels(layerPreview)) continue;
            includeBounds(
                layerPreview.left,
                layerPreview.top,
                layerPreview.left + layerPreview.width,
                layerPreview.top + layerPreview.height
            );
        }
        if (!hasBounds) includeBounds(0, 0, max(1, preview.compositionWidth), max(1, preview.compositionHeight));
        if (right <= left) right = left + 1;
        if (bottom <= top) bottom = top + 1;
        auto centerX = (cast(float)left + cast(float)right) * 0.5f;
        auto centerY = (cast(float)top + cast(float)bottom) * 0.5f;
        auto sourceW = cast(float)(right - left);
        auto sourceH = cast(float)(bottom - top);
        enum ulong PsdDepth3DAdjustSceneCameraKey = ulong.max;
        if (threeDAdjustCameraTargetUuid != PsdDepth3DAdjustSceneCameraKey ||
            threeDAdjustCameraLeft != left ||
            threeDAdjustCameraTop != top ||
            threeDAdjustCameraRight != right ||
            threeDAdjustCameraBottom != bottom) {
            threeDAdjustCameraTargetUuid = PsdDepth3DAdjustSceneCameraKey;
            threeDAdjustCameraLeft = left;
            threeDAdjustCameraTop = top;
            threeDAdjustCameraRight = right;
            threeDAdjustCameraBottom = bottom;
            resetPsdDepth3DAdjustCameraToBounds(sourceW, sourceH, canvasSize);
        }
        auto depthDisplayScale = max(1.0f, sourceH);

        auto framebufferWidth = max(1, cast(int)canvasSize.x);
        auto framebufferHeight = max(1, cast(int)canvasSize.y);
        ubyte[] framebuffer;
        float[] zBuffer;
        framebuffer.length = cast(size_t)framebufferWidth * cast(size_t)framebufferHeight * 4;
        zBuffer.length = cast(size_t)framebufferWidth * cast(size_t)framebufferHeight;
        foreach (i; 0 .. cast(size_t)framebufferWidth * cast(size_t)framebufferHeight) {
            auto pixelIndex = i * 4;
            framebuffer[pixelIndex + 0] = 20;
            framebuffer[pixelIndex + 1] = 23;
            framebuffer[pixelIndex + 2] = 26;
            framebuffer[pixelIndex + 3] = 255;
            zBuffer[i] = float.max;
        }
        size_t renderedPixels;

        float cameraDepthForPoint(vec2 point, float depth) {
            import std.math : cos, sin;

            float cy = cos(threeDAdjustCamera.yaw);
            float sy = sin(threeDAdjustCamera.yaw);
            float cp = cos(threeDAdjustCamera.pitch);
            float sp = sin(threeDAdjustCamera.pitch);

            float x = point.x;
            float y = point.y;
            float z = depth;

            float rz = -x * sy + z * cy;
            return y * sp + rz * cp;
        }

        ImVec2 projectDocumentPoint(vec2 documentPoint, float depth) {
            auto projected = projectDepthPoint(
                vec2(documentPoint.x - centerX, documentPoint.y - centerY),
                -depth * depthDisplayScale,
                threeDAdjustCamera
            );
            return ImVec2(
                origin.x + canvasSize.x * 0.5f + projected.x,
                origin.y + canvasSize.y * 0.5f + projected.y
            );
        }

        float edgeFunction(float ax, float ay, float bx, float by, float cx, float cy) {
            return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
        }

        ubyte sampleCoverage(ref PsdDepthLayerPreview layerPreview, float u, float v) {
            auto textureWidth = layerPreview.width;
            auto textureHeight = layerPreview.height;
            if (textureWidth <= 0 || textureHeight <= 0) return 0;
            auto x = clamp(cast(int)(u * cast(float)textureWidth), 0, textureWidth - 1);
            auto y = clamp(cast(int)(v * cast(float)textureHeight), 0, textureHeight - 1);
            auto index = (cast(size_t)y * cast(size_t)textureWidth + cast(size_t)x) * 4;
            if (index + 3 < layerPreview.originalRgba.length) return layerPreview.originalRgba[index + 3];
            return 255;
        }

        bool sampleColor(ref PsdDepthLayerPreview layerPreview, float u, float v, out ubyte r, out ubyte g, out ubyte b, out ubyte a) {
            r = g = b = a = 0;
            auto textureWidth = layerPreview.width;
            auto textureHeight = layerPreview.height;
            if (textureWidth <= 0 || textureHeight <= 0) return false;
            auto x = clamp(cast(int)(u * cast(float)textureWidth), 0, textureWidth - 1);
            auto y = clamp(cast(int)(v * cast(float)textureHeight), 0, textureHeight - 1);
            auto index = (cast(size_t)y * cast(size_t)textureWidth + cast(size_t)x) * 4;
            if (index + 3 >= layerPreview.originalRgba.length) return false;
            r = layerPreview.originalRgba[index + 0];
            g = layerPreview.originalRgba[index + 1];
            b = layerPreview.originalRgba[index + 2];
            a = layerPreview.originalRgba[index + 3];
            return true;
        }

        void rasterizeImageTriangle(
            ref PsdDepthLayerPreview layerPreview,
            ImVec2 p0,
            ImVec2 p1,
            ImVec2 p2,
            ImVec2 uv0,
            ImVec2 uv1,
            ImVec2 uv2,
            float z0,
            float z1,
            float z2
        ) {
            import std.math : ceil, floor;

            auto x0 = p0.x - origin.x;
            auto y0 = p0.y - origin.y;
            auto x1 = p1.x - origin.x;
            auto y1 = p1.y - origin.y;
            auto x2 = p2.x - origin.x;
            auto y2 = p2.y - origin.y;
            auto area = edgeFunction(x0, y0, x1, y1, x2, y2);
            if (area == 0.0f) return;

            auto minX = clamp(cast(int)floor(min(min(x0, x1), x2)), 0, framebufferWidth - 1);
            auto maxX = clamp(cast(int)ceil(max(max(x0, x1), x2)), 0, framebufferWidth - 1);
            auto minY = clamp(cast(int)floor(min(min(y0, y1), y2)), 0, framebufferHeight - 1);
            auto maxY = clamp(cast(int)ceil(max(max(y0, y1), y2)), 0, framebufferHeight - 1);

            for (int y = minY; y <= maxY; y++) {
                for (int x = minX; x <= maxX; x++) {
                    auto px = cast(float)x + 0.5f;
                    auto py = cast(float)y + 0.5f;
                    auto w0 = edgeFunction(x1, y1, x2, y2, px, py);
                    auto w1 = edgeFunction(x2, y2, x0, y0, px, py);
                    auto w2 = edgeFunction(x0, y0, x1, y1, px, py);
                    if (area > 0.0f) {
                        if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
                    } else {
                        if (w0 > 0.0f || w1 > 0.0f || w2 > 0.0f) continue;
                    }
                    w0 /= area;
                    w1 /= area;
                    w2 /= area;
                    auto z = z0 * w0 + z1 * w1 + z2 * w2;
                    auto zIndex = cast(size_t)y * cast(size_t)framebufferWidth + cast(size_t)x;
                    if (z >= zBuffer[zIndex]) continue;

                    auto u = uv0.x * w0 + uv1.x * w1 + uv2.x * w2;
                    auto v = uv0.y * w0 + uv1.y * w1 + uv2.y * w2;
                    if (sampleCoverage(layerPreview, u, v) < 128) continue;

                    ubyte sr;
                    ubyte sg;
                    ubyte sb;
                    ubyte sa;
                    if (!sampleColor(layerPreview, u, v, sr, sg, sb, sa) || sa < 3) continue;

                    auto pixelIndex = zIndex * 4;
                    auto alpha = cast(float)sa / 255.0f;
                    framebuffer[pixelIndex + 0] = cast(ubyte)clamp(cast(int)(cast(float)sr * alpha +
                        cast(float)framebuffer[pixelIndex + 0] * (1.0f - alpha) + 0.5f), 0, 255);
                    framebuffer[pixelIndex + 1] = cast(ubyte)clamp(cast(int)(cast(float)sg * alpha +
                        cast(float)framebuffer[pixelIndex + 1] * (1.0f - alpha) + 0.5f), 0, 255);
                    framebuffer[pixelIndex + 2] = cast(ubyte)clamp(cast(int)(cast(float)sb * alpha +
                        cast(float)framebuffer[pixelIndex + 2] * (1.0f - alpha) + 0.5f), 0, 255);
                    framebuffer[pixelIndex + 3] = 255;
                    zBuffer[zIndex] = z;
                }
            }
        }

        foreach (ref layerPreview; preview.layerPreviews) {
            if (!hasRenderPixels(layerPreview)) continue;
            if (layerPreview.width <= 1 || layerPreview.height <= 1) continue;
            auto transform = ngPsdDepthLayerTransform(settings, layerPreview.layerPath);
            auto step = 1;
            auto cols = cast(int)((layerPreview.width - 1) / step) + 1;
            auto rows = cast(int)((layerPreview.height - 1) / step) + 1;
            if (cols < 2 || rows < 2) continue;

            ImVec2[] points;
            ImVec2[] uvs;
            float[] cameraDepths;
            bool[] validPoints;
            points.length = cast(size_t)cols * cast(size_t)rows;
            uvs.length = points.length;
            cameraDepths.length = points.length;
            validPoints.length = points.length;

            size_t vertexIndex(int x, int y) {
                return cast(size_t)y * cast(size_t)cols + cast(size_t)x;
            }

            ubyte sourceDepthByte(size_t index) {
                if (index + 3 >= layerPreview.depthRgba.length) {
                    return 0;
                }

                float value;
                if (layerPreview.depthRgba[index + 3] == 0) {
                    value = 255.0f;
                } else {
                    auto r = cast(float)layerPreview.depthRgba[index + 0];
                    auto g = cast(float)layerPreview.depthRgba[index + 1];
                    auto b = cast(float)layerPreview.depthRgba[index + 2];
                    switch (layerPreview.channel) {
                        case PsdDepthChannel.R:
                            value = r;
                            break;
                        case PsdDepthChannel.G:
                            value = g;
                            break;
                        case PsdDepthChannel.B:
                            value = b;
                            break;
                        case PsdDepthChannel.Luminance:
                            value = r * 0.2126f + g * 0.7152f + b * 0.0722f;
                            break;
                        case PsdDepthChannel.AverageRGB:
                        default:
                            value = (r + g + b) / 3.0f;
                            break;
                    }
                }
                if (layerPreview.invert) value = 255.0f - value;
                return cast(ubyte)clamp(cast(int)(value + 0.5f), 0, 255);
            }

            bool renderDepthMaskAt(int x, int y, out ubyte depthByte) {
                depthByte = 0;
                if (x < 0 || y < 0 || x >= layerPreview.width || y >= layerPreview.height) return false;
                auto index = (cast(size_t)y * cast(size_t)layerPreview.width + cast(size_t)x) * 4;
                if (index >= layerPreview.depthMaskRgba.length) {
                    return false;
                }
                depthByte = layerPreview.depthMaskRgba[index + 0];
                return depthByte > 0;
            }

            bool depthAt(int x, int y, out float depth) {
                depth = 0.0f;
                ubyte rawDepthByte;
                if (!renderDepthMaskAt(x, y, rawDepthByte)) return false;
                auto transformedDepthByte = cast(int)(cast(float)rawDepthByte * transform.zScale + transform.zOffset + 0.5f);
                transformedDepthByte = clamp(transformedDepthByte, 1, 255);
                if (transform.invert) transformedDepthByte = 255 - transformedDepthByte;
                depth = (cast(float)transformedDepthByte / 255.0f) * PsdDepth3DAdjustDepthScale;
                return true;
            }

            void rasterizeDepthPixel(int px, int py, float depth) {
                auto colorIndex = (cast(size_t)py * cast(size_t)layerPreview.width + cast(size_t)px) * 4;
                if (colorIndex + 3 >= layerPreview.originalRgba.length) return;
                auto sa = layerPreview.originalRgba[colorIndex + 3];
                if (sa < 3) return;

                auto documentPoint = vec2(
                    cast(float)layerPreview.left + (cast(float)px + transform.xyOffsetX) * transform.xyScaleX,
                    cast(float)layerPreview.top + (cast(float)py + transform.xyOffsetY) * transform.xyScaleY
                );
                auto projected = projectDocumentPoint(documentPoint, depth);
                auto sx = cast(int)round(projected.x - origin.x);
                auto sy = cast(int)round(projected.y - origin.y);
                if (sx < -1 || sy < -1 || sx > framebufferWidth || sy > framebufferHeight) return;

                auto cameraPoint = vec2(documentPoint.x - centerX, documentPoint.y - centerY);
                auto z = cameraDepthForPoint(cameraPoint, -depth * depthDisplayScale);
                auto radius = max(1, step / 2);
                for (int dy = -radius; dy <= radius; dy++) {
                    auto y = sy + dy;
                    if (y < 0 || y >= framebufferHeight) continue;
                    for (int dx = -radius; dx <= radius; dx++) {
                        auto x = sx + dx;
                        if (x < 0 || x >= framebufferWidth) continue;
                        auto zIndex = cast(size_t)y * cast(size_t)framebufferWidth + cast(size_t)x;
                        if (z >= zBuffer[zIndex]) continue;

                        auto alpha = cast(float)sa / 255.0f;
                        auto pixelIndex = zIndex * 4;
                        auto sr = layerPreview.originalRgba[colorIndex + 0];
                        auto sg = layerPreview.originalRgba[colorIndex + 1];
                        auto sb = layerPreview.originalRgba[colorIndex + 2];
                        framebuffer[pixelIndex + 0] = cast(ubyte)clamp(cast(int)(cast(float)sr * alpha +
                            cast(float)framebuffer[pixelIndex + 0] * (1.0f - alpha) + 0.5f), 0, 255);
                        framebuffer[pixelIndex + 1] = cast(ubyte)clamp(cast(int)(cast(float)sg * alpha +
                            cast(float)framebuffer[pixelIndex + 1] * (1.0f - alpha) + 0.5f), 0, 255);
                        framebuffer[pixelIndex + 2] = cast(ubyte)clamp(cast(int)(cast(float)sb * alpha +
                            cast(float)framebuffer[pixelIndex + 2] * (1.0f - alpha) + 0.5f), 0, 255);
                        framebuffer[pixelIndex + 3] = 255;
                        zBuffer[zIndex] = z;
                        renderedPixels++;
                    }
                }
            }

            for (int y = 0; y < layerPreview.height; y += step) {
                for (int x = 0; x < layerPreview.width; x += step) {
                    float depth;
                    if (!depthAt(x, y, depth)) continue;
                    rasterizeDepthPixel(x, y, depth);
                }
            }
        }

        inTexPremultiply(framebuffer);
        if (threeDAdjustPreviewTexture is null ||
            threeDAdjustPreviewWidth != framebufferWidth ||
            threeDAdjustPreviewHeight != framebufferHeight) {
            if (threeDAdjustPreviewTexture !is null) threeDAdjustPreviewTexture.dispose();
            threeDAdjustPreviewTexture = new Texture(framebuffer, framebufferWidth, framebufferHeight, 4, 4, false, false);
            threeDAdjustPreviewWidth = framebufferWidth;
            threeDAdjustPreviewHeight = framebufferHeight;
        } else {
            threeDAdjustPreviewTexture.setData(framebuffer, 4);
        }
        ImDrawList_AddImage(
            drawList,
            cast(void*)threeDAdjustPreviewTexture.getTextureId(),
            origin,
            canvasMax,
            ImVec2(0, 0),
            ImVec2(1, 1),
            textureTint
        );
        ImDrawList_AddRect(drawList, origin, canvasMax, border, 4.0f, ImDrawFlags.None, 1.0f);
    }

    void draw3DAdjustTargetPreview(float height) {
        auto available = incAvailableSpace();
        auto controlWidth = min(360.0f, max(300.0f, available.x * 0.34f));
        if (igBeginTable("###PsdDepth3DRelationshipLayout", 2,
            ImGuiTableFlags.Resizable | ImGuiTableFlags.SizingStretchProp, ImVec2(0, height))) {
            igTableSetupColumn(__("3D View"), ImGuiTableColumnFlags.WidthStretch);
            igTableSetupColumn(__("Adjust"), ImGuiTableColumnFlags.WidthFixed, controlWidth);
            igTableNextRow();
            igTableNextColumn();
            draw3DAdjustRelationshipCanvas(height);
            igTableNextColumn();
            draw3DAdjustLayerControlsPanel();
            igEndTable();
        }
    }

    PsdDepth3DAdjustGeometryStats threeDAdjustGeometryStats(ref PsdDepthGridResult gridResult) {
        PsdDepth3DAdjustGeometryStats stats;
        if (gridResult.grid is null) return stats;
        auto targetView = new DepthTargetView(gridResult.grid);
        auto targetVertices = targetView.getVertices();
        foreach (layerMask; gridResult.layerMasks) {
            if (findLayerPreview(layerMask.layerPath) is null) continue;
            stats.layerPlanes++;
            stats.depthRangeLines += 2;
        }
        if (targetVertices.length > 1) stats.targetWireLines = targetVertices.length - 1;
        foreach (i; 0 .. targetVertices.length) {
            if (i < gridResult.missingVertexMask.length && gridResult.missingVertexMask[i]) {
                stats.missingPoints++;
            } else {
                stats.sampledPoints++;
            }
        }
        return stats;
    }

    void draw3DAdjustTab(float height) {
        auto lines = diagnostics();
        if (lines.length && (errorMessage.length || preview.grids.length == 0)) {
            drawDiagnostics();
            return;
        }
        if (preview.layerPreviews.length == 0) {
            incText(_("No composed source layers were loaded."));
            return;
        }
        if (lines.length) {
            drawDiagnostics();
            igSeparator();
        }
        draw3DAdjustTargetPreview(height);
    }

    void apply() {
        if (previewDirty) rebuildPreview();
        if (errorMessage.length) {
            incDialog(__("Error"), errorMessage);
            return;
        }
        auto result = ngApplyPsdDepthImportResult(preview);
        if (!result.succeeded) {
            lastApplyErrorMessage = result.message;
            incDialog(__("Error"), result.message);
            return;
        }
        lastApplyErrorMessage = null;
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

        if (igBeginTabBar("###PsdDepthMapImportTabs")) {
            if (igBeginTabItem(__("Source / Mapping"))) {
                drawSourceMappingTab(reviewHeight);
                igEndTabItem();
            }
            if (igBeginTabItem(__("3D Adjust"))) {
                draw3DAdjustTab(reviewHeight);
                igEndTabItem();
            }
            igEndTabBar();
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

    version (RegressionSmoke) {
        void rebuildPreviewForRegressionSmoke() {
            rebuildPreview();
        }

        string loadErrorForRegressionSmoke() const {
            return errorMessage;
        }

        size_t previewGridCountForRegressionSmoke() const {
            return preview.grids.length;
        }

        bool remapLayerNameToGridForRegressionSmoke(string layerName, ulong gridUuid) {
            foreach (mapping; preview.mappings) {
                if (mapping.layerName.toLower != layerName.toLower) continue;
                settings.layerTargetGridUuidOverrides[mapping.layerPath] = gridUuid.to!string;
                rebuildPreview();
                return true;
            }
            return false;
        }

        bool remapAnyLayerToSampledGridForRegressionSmoke(string gridName, ulong gridUuid) {
            string[] layerPaths;
            foreach (mapping; preview.mappings) {
                if (mapping.layerPath.length == 0) continue;
                layerPaths ~= mapping.layerPath;
            }
            foreach (layerPath; layerPaths) {
                settings.layerTargetGridUuidOverrides[layerPath] = gridUuid.to!string;
                rebuildPreview();
                if (hasSampledPreviewGridForRegressionSmoke(gridName)) return true;
            }
            return false;
        }

        bool hasSampledPreviewGridForRegressionSmoke(string gridName) {
            foreach (gridResult; preview.grids) {
                if (gridResult.grid is null || gridResult.grid.name != gridName) continue;
                return !gridResult.skipped &&
                    gridResult.depths.length > 0 &&
                    gridResult.sampledVertices > 0 &&
                    gridResult.previewWidth > 0 &&
                    gridResult.previewHeight > 0 &&
                    gridResult.rawCompositePreviewRgba.length > 0;
            }
            return false;
        }

        bool has3DAdjustGeometryForRegressionSmoke(string gridName) {
            foreach (ref gridResult; preview.grids) {
                if (gridResult.grid is null || gridResult.grid.name != gridName) continue;
                auto stats = threeDAdjustGeometryStats(gridResult);
                return stats.layerPlanes > 0 &&
                    stats.depthRangeLines > 0 &&
                    stats.targetWireLines > 0 &&
                    stats.sampledPoints + stats.missingPoints == gridResult.grid.vertices.length &&
                    stats.sampledPoints > 0;
            }
            return false;
        }

        string[] diagnosticsForRegressionSmoke() {
            return diagnostics();
        }

        string previewSummaryForRegressionSmoke() {
            string[] lines;
            foreach (mapping; preview.mappings) {
                lines ~= "mapping layer=%s target=%s status=%s matched=%s manual=%s".format(
                    mapping.layerPath,
                    mapping.targetGridName,
                    mapping.status,
                    mapping.matched,
                    mapping.manual);
            }
            foreach (gridResult; preview.grids) {
                lines ~= "grid=%s depths=%s sampled=%s missing=%s preview=%sx%s raw=%s skipped=%s coverage=%s".format(
                    gridResult.grid !is null ? gridResult.grid.name : "<null>",
                    gridResult.depths.length,
                    gridResult.sampledVertices,
                    gridResult.missingVertices,
                    gridResult.previewWidth,
                    gridResult.previewHeight,
                    gridResult.rawCompositePreviewRgba.length,
                    gridResult.skipped,
                    gridResult.coverageSources);
            }
            return lines.join(" | ");
        }

        bool applyForRegressionSmoke(out string message) {
            if (previewDirty) rebuildPreview();
            if (errorMessage.length) {
                message = errorMessage;
                return false;
            }
            auto result = ngApplyPsdDepthImportResult(preview);
            message = result.message;
            return result.succeeded;
        }

        void setGpuCompositionForRegressionSmoke(bool enabled) {
            settings.useGpuComposition = enabled;
            previewDirty = true;
            rebuildPreview();
        }

        PsdDepthImportResult previewForRegressionSmoke() {
            if (previewDirty) rebuildPreview();
            return preview;
        }

        bool hasAppliedDepthsForRegressionSmoke(string gridName) {
            foreach (gridResult; preview.grids) {
                if (gridResult.grid is null || gridResult.grid.name != gridName) continue;
                auto mapped = cast(DepthMappedNode)gridResult.grid;
                if (mapped is null) return false;
                auto depths = mapped.copyDepths();
                return depths !is null && depths.length == gridResult.depths.length && depths == gridResult.depths;
            }
            return false;
        }
    }
}
