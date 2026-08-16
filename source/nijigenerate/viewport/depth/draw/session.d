module nijigenerate.viewport.depth.draw.session;

import nijigenerate.io.depthimage : DepthDrawAlphaDepthGapDetection, DepthDrawAlphaDepthGapFillResult,
    DepthDrawAlphaDepthFocusedRule, DepthImageChannel, DepthImageConvolution, ngDepthDrawAlphaMaskFromRgba,
    ngDepthDrawBuildLayerContourBandMask, ngDepthDrawDecodeDepthPixelsFromRgba, ngDepthDrawDetectAlphaDepthGaps,
    ngDepthDrawInpaintMaskedLayerDepth, ngDepthDrawMedianFillDepth;
import nijigenerate.viewport.depth.draw.binding;
import nijigenerate.viewport.depth.draw.coordinate : ngDepthDrawLayerDocumentBounds;
import nijigenerate.viewport.depth.draw.layer;
import nijilive.math : vec2;
import std.algorithm : sort;
import std.format : format;

struct DepthDrawReloadStateResult {
    size_t matchedLayers;
    size_t preservedBindings;
    bool preservedSelectedLayer;
    bool preservedSelectedTarget;
}

struct DepthDrawDisplayOptions {
    bool showNormalImage = true;
    bool showRawDepth = false;
    bool showCoverage = false;
    bool showComposite = true;
    bool showLayerPlanes = true;
    bool showDepthRanges = true;
    bool showMissingVertices = true;
    bool showWinningLayer = false;
    bool useGpuPreview = false;
}

struct DepthDrawLayerAlphaDepthGapFillSummary {
    bool succeeded;
    string layerId;
    DepthDrawAlphaDepthGapDetection detected;
    DepthDrawAlphaDepthGapFillResult filled;
}

struct DepthDrawLayerContourRepairSummary {
    bool succeeded;
    string layerId;
    size_t contourPixels;
    size_t filledPixels;
}

private DepthDrawLayerCleanupOperation[] cloneCleanupOperations(
    const(DepthDrawLayerCleanupOperation)[] operations
) {
    DepthDrawLayerCleanupOperation[] result;
    foreach (operation; operations) {
        DepthDrawLayerCleanupOperation copy;
        copy.kind = operation.kind;
        copy.contourThickness = operation.contourThickness;
        copy.focusedRules = operation.focusedRules.dup;
        result ~= copy;
    }
    return result;
}

private bool writeCleanupDepthPixel(ref DepthDrawLayer layer, size_t index, ubyte value) {
    auto offset = index * 4;
    bool changed;
    final switch (layer.channel) {
        case DepthImageChannel.R:
            changed = layer.depthPixels[offset] != value;
            layer.depthPixels[offset] = value;
            break;
        case DepthImageChannel.G:
            changed = layer.depthPixels[offset + 1] != value;
            layer.depthPixels[offset + 1] = value;
            break;
        case DepthImageChannel.B:
            changed = layer.depthPixels[offset + 2] != value;
            layer.depthPixels[offset + 2] = value;
            break;
        case DepthImageChannel.AverageRGB:
        case DepthImageChannel.Luminance:
            changed = layer.depthPixels[offset] != value ||
                layer.depthPixels[offset + 1] != value ||
                layer.depthPixels[offset + 2] != value;
            layer.depthPixels[offset] = value;
            layer.depthPixels[offset + 1] = value;
            layer.depthPixels[offset + 2] = value;
            break;
    }
    return changed;
}

class DepthDrawSession {
private:
    bool[ulong] previewDirtyTargets;
    ulong[ulong] previewTargetRevisions;

    ptrdiff_t findLayerIndex(string id) const {
        foreach (i, ref layer; layers) {
            if (layer.id == id) return cast(ptrdiff_t)i;
        }
        return -1;
    }

public:
    DepthDrawLayer[] layers;
    DepthDrawBinding[] bindings;

    string selectedLayerId;
    ulong selectedGridUuid;
    DepthDrawDisplayOptions display;
    string sourceIdentity;
    int documentWidth;
    int documentHeight;

    DepthDrawLayer* layerById(string id) {
        auto index = findLayerIndex(id);
        return index >= 0 ? &layers[cast(size_t)index] : null;
    }

    const(DepthDrawLayer)* layerById(string id) const {
        auto index = findLayerIndex(id);
        return index >= 0 ? &layers[cast(size_t)index] : null;
    }

    DepthDrawBinding[] bindingsForGrid(ulong gridUuid) const {
        DepthDrawBinding[] result;
        foreach (binding; bindings) {
            if (binding.appliesTo(gridUuid)) result ~= binding;
        }
        return result;
    }

    bool hasTargetGrid(ulong gridUuid) const {
        if (gridUuid == 0) return false;
        foreach (binding; bindings) {
            if (binding.targetGridUuid == gridUuid) return true;
        }
        return false;
    }

    void markTargetPreviewDirty(ulong gridUuid) {
        if (gridUuid == 0) return;
        auto revision = gridUuid in previewTargetRevisions;
        auto nextRevision = revision is null ? 1 : *revision + 1;
        if (nextRevision == 0) nextRevision = 1;
        previewTargetRevisions[gridUuid] = nextRevision;
        previewDirtyTargets[gridUuid] = true;
    }

    ulong targetPreviewRevision(ulong gridUuid) const {
        auto revision = gridUuid in previewTargetRevisions;
        return revision is null ? 0 : *revision;
    }

    void markLayerPreviewDirty(string layerId) {
        if (layerId.length == 0) return;
        foreach (binding; bindings) {
            if (!binding.enabled || binding.layerId != layerId) continue;
            markTargetPreviewDirty(binding.targetGridUuid);
        }
    }

    void markBindingPreviewDirty(const(DepthDrawBinding) binding) {
        if (!binding.enabled) return;
        markTargetPreviewDirty(binding.targetGridUuid);
    }

    void markAllPreviewDirty() {
        foreach (binding; bindings) {
            if (!binding.enabled) continue;
            markTargetPreviewDirty(binding.targetGridUuid);
        }
    }

    bool isTargetPreviewDirty(ulong gridUuid) const {
        return (gridUuid in previewDirtyTargets) !is null;
    }

    ulong[] dirtyTargetGridIds() const {
        auto result = previewDirtyTargets.keys;
        sort(result);
        return result;
    }

    void clearTargetPreviewDirty(ulong gridUuid) {
        previewDirtyTargets.remove(gridUuid);
    }

    void clearPreviewDirty() {
        previewDirtyTargets = null;
    }

    bool selectLayer(string id) {
        if (layerById(id) is null) return false;
        selectedLayerId = id;
        return true;
    }

    bool selectTargetGrid(ulong gridUuid) {
        if (!hasTargetGrid(gridUuid)) return false;
        selectedGridUuid = gridUuid;
        return true;
    }

    bool selectLayerPlaneAtDocumentPoint(vec2 documentPoint) {
        foreach_reverse (ref layer; layers) {
            if (!layer.enabled || !layer.visible) continue;
            vec2 minPoint;
            vec2 maxPoint;
            ngDepthDrawLayerDocumentBounds(layer, minPoint, maxPoint);
            if (documentPoint.x < minPoint.x || documentPoint.x > maxPoint.x ||
                documentPoint.y < minPoint.y || documentPoint.y > maxPoint.y) {
                continue;
            }
            selectedLayerId = layer.id;
            foreach (binding; bindings) {
                if (!binding.enabled || binding.layerId != layer.id) continue;
                selectedGridUuid = binding.targetGridUuid;
                break;
            }
            return true;
        }
        return false;
    }

    bool updateLayerXYTransform(string layerId, vec2 xyOffset, vec2 xyScale) {
        auto layer = layerById(layerId);
        if (layer is null) return false;
        layer.xyOffset = xyOffset;
        layer.xyScale = xyScale;
        markLayerPreviewDirty(layerId);
        return true;
    }

    bool updateLayerZTransform(string layerId, float backDepth, float frontDepth, bool invert, float zScale, float zOffset) {
        auto layer = layerById(layerId);
        if (layer is null) return false;
        layer.backDepth = backDepth;
        layer.frontDepth = frontDepth;
        layer.invert = invert;
        layer.zScale = zScale;
        layer.zOffset = zOffset;
        markLayerPreviewDirty(layerId);
        return true;
    }

    bool updateLayerSampling(string layerId, DepthImageChannel channel, DepthImageConvolution convolution,
            int customRadius, float alphaThreshold) {
        auto layer = layerById(layerId);
        if (layer is null) return false;
        if (display.useGpuPreview && convolution == DepthImageConvolution.MedianCustom) {
            convolution = DepthImageConvolution.Median3x3;
        }
        layer.channel = channel;
        layer.convolution = convolution;
        layer.customRadius = customRadius;
        layer.alphaThreshold = alphaThreshold;
        markLayerPreviewDirty(layerId);
        return true;
    }

    bool updateLayerVisibility(string layerId, bool visible, bool enabled) {
        auto layer = layerById(layerId);
        if (layer is null) return false;
        layer.visible = visible;
        layer.enabled = enabled;
        markLayerPreviewDirty(layerId);
        return true;
    }

    DepthDrawLayerAlphaDepthGapFillSummary applyLayerAlphaDepthGapFill(
        string layerId,
        const(DepthDrawAlphaDepthFocusedRule)[] focusedRules = null
    ) {
        DepthDrawLayerAlphaDepthGapFillSummary summary;
        summary.layerId = layerId;
        auto layer = layerById(layerId);
        if (layer is null || !layer.hasDepthPixels()) return summary;

        auto depth = ngDepthDrawDecodeDepthPixelsFromRgba(layer.depthPixels, layer.channel);
        auto alphaMask = layer.alphaMask.length == depth.length
            ? layer.alphaMask.dup
            : ngDepthDrawAlphaMaskFromRgba(layer.depthPixels);
        summary.detected = ngDepthDrawDetectAlphaDepthGaps(depth, alphaMask, layer.width, layer.height,
            cast(int)findLayerIndex(layerId), focusedRules);
        summary.filled = ngDepthDrawMedianFillDepth(depth, alphaMask, summary.detected.mask, layer.width, layer.height);
        if (summary.detected.total == 0 && summary.filled.filled == 0 && summary.filled.remaining == 0) {
            summary.succeeded = true;
            return summary;
        }

        bool changed;
        foreach (i, value; summary.filled.depth) {
            if (!summary.detected.mask[i] || depth[i] == value) continue;
            changed = writeCleanupDepthPixel(*layer, i, value) || changed;
        }
        layer.alphaMask = alphaMask;
        if (changed) {
            DepthDrawLayerCleanupOperation operation;
            operation.kind = DepthDrawLayerCleanupKind.AlphaDepthGapFill;
            operation.focusedRules = focusedRules.dup;
            layer.cleanupOperations ~= operation;
            markLayerPreviewDirty(layerId);
        }
        summary.succeeded = true;
        return summary;
    }

    DepthDrawLayerContourRepairSummary repairLayerContourDepth(string layerId, int thickness = 2) {
        DepthDrawLayerContourRepairSummary summary;
        summary.layerId = layerId;
        auto layer = layerById(layerId);
        if (layer is null || !layer.hasDepthPixels()) return summary;

        auto depth = ngDepthDrawDecodeDepthPixelsFromRgba(layer.depthPixels, layer.channel);
        auto mask = layer.alphaMask.length == depth.length
            ? layer.alphaMask.dup
            : ngDepthDrawAlphaMaskFromRgba(layer.depthPixels);
        auto contourBand = ngDepthDrawBuildLayerContourBandMask(mask, layer.width, layer.height, thickness);
        auto repairedSeed = depth.dup;
        foreach (i, value; contourBand) {
            if (value) {
                repairedSeed[i] = 0;
                summary.contourPixels += 1;
            }
        }
        auto repaired = ngDepthDrawInpaintMaskedLayerDepth(repairedSeed, mask, layer.width, layer.height);
        foreach (i, value; repaired.filledMask) {
            if (value) summary.filledPixels += 1;
        }
        bool changed;
        foreach (i, value; repaired.pixels) {
            if (!repaired.filledMask[i]) continue;
            changed = writeCleanupDepthPixel(*layer, i, value) || changed;
        }
        layer.alphaMask = mask;
        if (changed) {
            DepthDrawLayerCleanupOperation operation;
            operation.kind = DepthDrawLayerCleanupKind.ContourRepair;
            operation.contourThickness = thickness;
            layer.cleanupOperations ~= operation;
            markLayerPreviewDirty(layerId);
        }
        summary.succeeded = true;
        return summary;
    }

    bool replayLayerCleanupOperations(string layerId) {
        auto layer = layerById(layerId);
        if (layer is null || !layer.hasDepthPixels()) return false;
        auto operations = cloneCleanupOperations(layer.cleanupOperations);
        layer.cleanupOperations = null;
        auto layerIndex = cast(int)findLayerIndex(layerId);
        foreach (operation; operations) {
            final switch (operation.kind) {
                case DepthDrawLayerCleanupKind.AlphaDepthGapFill:
                    auto rules = operation.focusedRules.dup;
                    foreach (ref rule; rules) rule.layerIndex = layerIndex;
                    applyLayerAlphaDepthGapFill(layerId, rules);
                    break;
                case DepthDrawLayerCleanupKind.ContourRepair:
                    repairLayerContourDepth(layerId, operation.contourThickness);
                    break;
            }
        }
        layer = layerById(layerId);
        if (layer is null) return false;
        layer.cleanupOperations = operations;
        if (operations.length > 0) markLayerPreviewDirty(layerId);
        return true;
    }

    bool updateBindingSampling(string layerId, ulong targetGridUuid, bool useNormalLayerAlpha, float coverageThreshold) {
        foreach (ref binding; bindings) {
            if (binding.layerId != layerId || binding.targetGridUuid != targetGridUuid) continue;
            binding.useNormalLayerAlpha = useNormalLayerAlpha;
            binding.coverageThreshold = coverageThreshold;
            markBindingPreviewDirty(binding);
            return true;
        }
        return false;
    }

    bool updateBindingState(string layerId, ulong targetGridUuid, bool enabled, int order, DepthMergePolicy mergePolicy) {
        foreach (ref binding; bindings) {
            if (binding.layerId != layerId || binding.targetGridUuid != targetGridUuid) continue;
            binding.enabled = enabled;
            binding.order = order;
            binding.mergePolicy = mergePolicy;
            markTargetPreviewDirty(targetGridUuid);
            return true;
        }
        return false;
    }

    bool normalizeGpuPreviewSampling() {
        if (!display.useGpuPreview) return false;
        bool changed;
        foreach (ref layer; layers) {
            if (layer.convolution != DepthImageConvolution.MedianCustom) continue;
            layer.convolution = DepthImageConvolution.Median3x3;
            markLayerPreviewDirty(layer.id);
            changed = true;
        }
        return changed;
    }

    bool updateDisplayOptions(DepthDrawDisplayOptions nextDisplay) {
        auto displayChanged = display != nextDisplay;
        display = nextDisplay;
        auto samplingChanged = normalizeGpuPreviewSampling();
        if (!displayChanged && !samplingChanged) return false;
        markAllPreviewDirty();
        return true;
    }

    void clearSelection() {
        selectedLayerId = null;
        selectedGridUuid = 0;
    }

    void clear() {
        layers = null;
        bindings = null;
        sourceIdentity = null;
        documentWidth = 0;
        documentHeight = 0;
        clearSelection();
        clearPreviewDirty();
        previewTargetRevisions = null;
    }
}

private string reloadLayerKey(const(DepthDrawLayer) layer) {
    return "%s\0%s\0%s\0%s".format(layer.layerPath, layer.displayName, layer.width, layer.height);
}

private DepthDrawLayer* findReloadLayerByKey(
    DepthDrawSession session,
    const(DepthDrawLayer) previousLayer,
    bool[string] usedReloadedLayerIds
) {
    if (session is null) return null;
    auto key = reloadLayerKey(previousLayer);
    foreach (ref layer; session.layers) {
        if ((layer.id in usedReloadedLayerIds) is null && reloadLayerKey(layer) == key) return &layer;
    }
    foreach (ref layer; session.layers) {
        if ((layer.id in usedReloadedLayerIds) is null &&
            previousLayer.sourcePath.length > 0 && layer.sourcePath == previousLayer.sourcePath &&
            layer.width == previousLayer.width && layer.height == previousLayer.height) {
            return &layer;
        }
    }
    if (session.layers.length == 1 && (session.layers[0].id in usedReloadedLayerIds) is null) {
        return &session.layers[0];
    }
    return null;
}

DepthDrawReloadStateResult ngDepthDrawCarryReloadState(DepthDrawSession reloaded, DepthDrawSession previous) {
    DepthDrawReloadStateResult result;
    if (reloaded is null || previous is null) return result;

    reloaded.display = previous.display;
    string[string] layerIdMap;
    bool[string] usedReloadedLayerIds;
    foreach (previousLayer; previous.layers) {
        // PSD ids are positional (psd:0, psd:1, ...), so stable source
        // identity must win when layers are inserted or reordered.
        auto layer = findReloadLayerByKey(reloaded, previousLayer, usedReloadedLayerIds);
        if (layer is null) {
            auto sameIdLayer = reloaded.layerById(previousLayer.id);
            if (sameIdLayer !is null && (sameIdLayer.id in usedReloadedLayerIds) is null) layer = sameIdLayer;
        }
        if (layer is null) continue;

        auto reloadedId = layer.id;
        usedReloadedLayerIds[reloadedId] = true;
        layer.visible = previousLayer.visible;
        layer.enabled = previousLayer.enabled;
        layer.xyOffset = previousLayer.xyOffset;
        layer.xyScale = previousLayer.xyScale;
        layer.zOffset = previousLayer.zOffset;
        layer.zScale = previousLayer.zScale;
        layer.backDepth = previousLayer.backDepth;
        layer.frontDepth = previousLayer.frontDepth;
        layer.invert = previousLayer.invert;
        layer.channel = previousLayer.channel;
        layer.sampleDepthScale = previousLayer.sampleDepthScale;
        layer.convolution = previousLayer.convolution;
        layer.customRadius = previousLayer.customRadius;
        layer.alphaThreshold = previousLayer.alphaThreshold;
        layer.cleanupOperations = cloneCleanupOperations(previousLayer.cleanupOperations);
        reloaded.replayLayerCleanupOperations(reloadedId);
        layerIdMap[previousLayer.id] = reloadedId;
        result.matchedLayers++;
    }

    reloaded.bindings = null;
    foreach (binding; previous.bindings) {
        auto mappedId = binding.layerId in layerIdMap;
        if (mappedId is null) continue;
        auto preserved = binding;
        preserved.layerId = *mappedId;
        reloaded.bindings ~= preserved;
        result.preservedBindings++;
    }

    reloaded.selectedLayerId = null;
    if (auto mappedSelection = previous.selectedLayerId in layerIdMap) {
        if (reloaded.layerById(*mappedSelection) !is null) {
            reloaded.selectedLayerId = *mappedSelection;
            result.preservedSelectedLayer = true;
        }
    }
    reloaded.selectedGridUuid = 0;
    if (previous.selectedGridUuid != 0 && reloaded.hasTargetGrid(previous.selectedGridUuid)) {
        reloaded.selectedGridUuid = previous.selectedGridUuid;
        result.preservedSelectedTarget = true;
    }
    reloaded.normalizeGpuPreviewSampling();
    reloaded.markAllPreviewDirty();
    return result;
}
