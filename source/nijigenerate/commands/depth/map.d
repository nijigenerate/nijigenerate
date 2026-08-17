module nijigenerate.commands.depth.map;

import core.thread : Thread;
import core.time : msecs;

import nijigenerate.actions.depth;
import nijigenerate.actions.depthbone : DepthRigBindingsChangeAction;
import nijigenerate.actions : AsyncGroupAction;
import nijigenerate.actions.asyncprogress : AsyncGroupActionProgress;
import nijigenerate.commands.base;
import nijigenerate.commands.depth.bone : ngBeginDepthBoneRefreshActionSink, ngEndDepthBoneRefreshActionSink,
    ngFlushDepthBoneDirty, ngHasPendingDepthBoneRefreshForSink, ngMarkDepthBoneDirtyForTarget,
    ngPendingDepthBoneRefreshWorkForSink;
import nijigenerate.core.actionstack : incActionPush, ngGuardActionStackScopes;
import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.ext.nodes.exdepthbone : ExDepthRigBinding, ExDepthRigRoot;
import nijigenerate.ext.nodes.exdepthops;
import nijigenerate.io.depthimage : DepthImageChannel, DepthImageConvolution, ngDepthDrawSmoothGridDepthValues,
    ngDepthImageSampleRgbaWithOpacity;
import nijigenerate.io.depthmap_psd;
import nijigenerate.io.depthsample : ngDepthSampleValueToDepth01;
import nijigenerate.project : incActivePuppet;
import nijigenerate.viewport.depth.common.targetview : DepthTargetView;
import nijigenerate.viewport.depth.draw.binding : DepthDrawBinding, DepthMergePolicy;
import nijigenerate.viewport.depth.draw.composer : DepthDrawComposeResult, ngComposeDepthDrawTarget;
import nijigenerate.viewport.depth.draw.coordinate : ngDepthDrawLayerPixelFromDocument;
import nijigenerate.viewport.depth.draw.gpu : DepthDrawGpuTargetComposeJob, DepthDrawGpuTargetComposePollResult,
    ngCancelDepthDrawGpuTargetCompose, ngPollDepthDrawGpuTargetCompose, ngSubmitDepthDrawGpuTargetCompose;
import nijigenerate.viewport.depth.draw.layer : DepthDrawLayer, DepthDrawRect;
import nijigenerate.viewport.depth.draw.pngexport : DepthDrawPngExportResult, ngExportDepthDrawPngSession;
import nijigenerate.viewport.depth.draw.session : DepthDrawSession;
import nijigenerate.viewport.depth.tools.operation : ngComputeDepthsFromOps;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import std.algorithm.comparison : max, min;
import std.conv : to;
import std.exception : enforce;
import std.json : JSONType, JSONValue;
import std.math : abs, isFinite, round;
import std.string : format;
import i18n;

enum DepthMapCommand {
    ListDepths,
    SetDepths,
    ClearDepths,
    ListDepthOps,
    SetDepthOps,
    AddDepthOp,
    UpdateDepthOp,
    RemoveDepthOp,
    MoveDepthOp,
    ClearDepthOps,
    ApplyDepthOps,
    ImportPSDDepths,
}

Command[DepthMapCommand] commands;

private DepthMappedNode requireDepthMapped(Node node) {
    auto mapped = cast(DepthMappedNode)node;
    enforce(mapped !is null, "target must support depth maps");
    return mapped;
}

private class PsdDepthImportChangeAction : AsyncGroupAction {
    override string describe() {
        return _("Imported PSD depth map");
    }

    override string describeUndo() {
        return _("PSD depth map import was reverted");
    }

    override string getName() {
        return this.stringof;
    }
}

private AsyncGroupActionProgress activePsdDepthImportProgress;

public struct PsdDepthComposedView {
private:
    PsdDepthImportResult* imported;
}

private struct PsdDepthGpuGridComposeWork {
    size_t gridIndex;
    PsdDepthGridResult sourceGrid;
    float[] oldDepths;
    string[string] layerIdToPath;
    DepthDrawLayer[] previewLayers;
    DepthDrawGpuTargetComposeJob job;
    bool completed;
}

struct PsdDepthGpuComposeWork {
private:
    PsdDepthGpuGridComposeWork[] grids;
    PsdDepthGridResult[] composedGrids;
    bool active;

public:
    bool isActive() const {
        return active;
    }
}

version (CommandBrowserDifferential) {
    PsdDepthComposedView ngPsdDepthComposedViewForRegression(ref PsdDepthImportResult imported) {
        PsdDepthComposedView composed;
        composed.imported = &imported;
        return composed;
    }

    bool ngPsdDepthImportProgressVisibleForRegression() {
        return activePsdDepthImportProgress !is null && activePsdDepthImportProgress.isVisible;
    }
}

private DepthOperationMappedNode requireDepthOperated(Node node) {
    auto operated = cast(DepthOperationMappedNode)node;
    enforce(operated !is null, "target must support depth operations");
    return operated;
}

private GridDeformer requireDepthGrid(Node node) {
    auto grid = cast(GridDeformer)node;
    enforce(grid !is null, "target must be a GridDeformer");
    return grid;
}

private float finiteFloat(float value, string name) {
    enforce(value.isFinite, "%s must be finite".format(name));
    return value;
}

private float jsonFloat(JSONValue value, string name, float fallback = 0.0f) {
    if (value.type == JSONType.null_) return fallback;
    final switch (value.type) {
        case JSONType.float_:
            return finiteFloat(cast(float)value.floating, name);
        case JSONType.integer:
            return finiteFloat(cast(float)value.integer, name);
        case JSONType.uinteger:
            return finiteFloat(cast(float)value.uinteger, name);
        case JSONType.null_:
            return fallback;
        case JSONType.object:
        case JSONType.array:
        case JSONType.string:
        case JSONType.true_:
        case JSONType.false_:
            enforce(false, "%s must be a number".format(name));
    }
    assert(0);
}

private size_t jsonSize(JSONValue value, string name, size_t fallback = 0) {
    if (value.type == JSONType.null_) return fallback;
    final switch (value.type) {
        case JSONType.integer:
            enforce(value.integer >= 0, "%s must be >= 0".format(name));
            return cast(size_t)value.integer;
        case JSONType.uinteger:
            return cast(size_t)value.uinteger;
        case JSONType.float_:
            enforce(value.floating >= 0 && value.floating == cast(long)value.floating, "%s must be a non-negative integer".format(name));
            return cast(size_t)value.floating;
        case JSONType.null_:
            return fallback;
        case JSONType.object:
        case JSONType.array:
        case JSONType.string:
        case JSONType.true_:
        case JSONType.false_:
            enforce(false, "%s must be a non-negative integer".format(name));
    }
    assert(0);
}

private vec2 jsonVec2(JSONValue value, string name, vec2 fallback = vec2(0, 0)) {
    if (value.type == JSONType.null_) return fallback;
    enforce(value.type == JSONType.array && value.array.length == 2, "%s must be [x, y]".format(name));
    return vec2(jsonFloat(value.array[0], name ~ "[0]"), jsonFloat(value.array[1], name ~ "[1]"));
}

private JSONValue vec2ToJson(vec2 value) {
    JSONValue result = JSONValue.emptyArray;
    result.array ~= JSONValue(cast(double)value.x);
    result.array ~= JSONValue(cast(double)value.y);
    return result;
}

JSONValue ngDepthOpToJson(ExDepthOp op) {
    JSONValue[string] obj;
    obj["type"] = JSONValue(op.typeName());
    final switch (op.type) {
        case ExDepthOpType.AttachedPoint:
            obj["index"] = JSONValue(cast(long)op.index);
            obj["amount"] = JSONValue(cast(double)op.amount);
            break;
        case ExDepthOpType.Ring:
            obj["p0"] = vec2ToJson(op.p0);
            obj["p1"] = vec2ToJson(op.p1);
            obj["amount"] = JSONValue(cast(double)op.amount);
            obj["width"] = JSONValue(cast(double)op.width);
            obj["hardness"] = JSONValue(cast(double)op.hardness);
            obj["p0Angle"] = JSONValue(cast(double)op.p0Angle);
            obj["p1Angle"] = JSONValue(cast(double)op.p1Angle);
            break;
        case ExDepthOpType.Plane:
            obj["center"] = vec2ToJson(op.center);
            obj["radiusX"] = JSONValue(cast(double)op.radiusX);
            obj["radiusY"] = JSONValue(cast(double)op.radiusY);
            obj["angle"] = JSONValue(cast(double)op.angle);
            obj["targetDepth"] = JSONValue(cast(double)op.targetDepth);
            obj["flattenStrength"] = JSONValue(cast(double)op.flattenStrength);
            break;
    }
    return JSONValue(obj);
}

private ExDepthOp depthOpFromJson(JSONValue value) {
    enforce(value.type == JSONType.object, "operation must be an object");
    enforce("type" in value.object, "operation.type is required");
    enforce(value["type"].type == JSONType.string, "operation.type must be a string");

    ExDepthOp op;
    auto type = value["type"].str;
    switch (type) {
        case "attached-point":
        case "AttachedPoint":
            op.type = ExDepthOpType.AttachedPoint;
            op.index = jsonSize(value.object.get("index", JSONValue(null)), "index");
            op.amount = jsonFloat(value.object.get("amount", JSONValue(0.0)), "amount");
            return op;
        case "ring":
        case "Ring":
            op.type = ExDepthOpType.Ring;
            op.p0 = jsonVec2(value.object.get("p0", JSONValue(null)), "p0");
            op.p1 = jsonVec2(value.object.get("p1", JSONValue(null)), "p1");
            op.amount = jsonFloat(value.object.get("amount", JSONValue(0.0)), "amount");
            op.width = max(0.5f, jsonFloat(value.object.get("width", JSONValue(1.0)), "width"));
            op.hardness = max(0.1f, jsonFloat(value.object.get("hardness", JSONValue(1.0)), "hardness"));
            op.p0Angle = jsonFloat(value.object.get("p0Angle", JSONValue(180.0)), "p0Angle");
            op.p1Angle = jsonFloat(value.object.get("p1Angle", JSONValue(0.0)), "p1Angle");
            return op;
        case "plane":
        case "Plane":
            op.type = ExDepthOpType.Plane;
            op.center = jsonVec2(value.object.get("center", JSONValue(null)), "center");
            op.radiusX = max(1.0f, jsonFloat(value.object.get("radiusX", JSONValue(1.0)), "radiusX"));
            op.radiusY = max(1.0f, jsonFloat(value.object.get("radiusY", JSONValue(1.0)), "radiusY"));
            op.angle = jsonFloat(value.object.get("angle", JSONValue(0.0)), "angle");
            op.targetDepth = jsonFloat(value.object.get("targetDepth", value.object.get("amount", JSONValue(0.0))), "targetDepth");
            op.flattenStrength = jsonFloat(value.object.get("flattenStrength", JSONValue(1.0)), "flattenStrength");
            return op;
        default:
            enforce(false, "unknown depth operation type: " ~ type);
    }
    assert(0);
}

private ExDepthOp[] depthOpsFromJson(JSONValue value) {
    enforce(value.type == JSONType.array, "operations must be an array");
    ExDepthOp[] result;
    foreach (entry; value.array) result ~= depthOpFromJson(entry);
    return result;
}

private JSONValue depthOpsToJson(ExDepthOp[] ops) {
    JSONValue result = JSONValue.emptyArray;
    foreach (i, op; ops) {
        auto obj = ngDepthOpToJson(op);
        obj.object["indexInList"] = JSONValue(cast(long)i);
        result.array ~= obj;
    }
    return result;
}

private JSONValue depthsToJson(float[] depths) {
    if (depths is null) return JSONValue(null);
    JSONValue result = JSONValue.emptyArray;
    foreach (depth; depths) result.array ~= JSONValue(cast(double)depth);
    return result;
}

DepthMappedChangeAction ngApplyDepthsChangeAction(Node target, float[] nextDepths, string reason) {
    auto mapped = requireDepthMapped(target);
    auto action = new DepthMappedChangeAction(target, reason);
    mapped.replaceDepths(nextDepths);
    target.notifyChange(target, NotifyReason.AttributeChanged);
    action.updateNewState();
    ngMarkDepthBoneDirtyForTarget(target, reason);
    return action;
}

private void replaceDepthsWithUndo(Node target, float[] nextDepths, string reason) {
    incActionPush(ngApplyDepthsChangeAction(target, nextDepths, reason));
}

private DepthOperationMappedChangeAction ngClearDepthOpsChangeAction(Node target, string reason) {
    auto action = ngClearDepthOperationsChangeAction(target);
    if (action is null) return null;
    ngMarkDepthBoneDirtyForTarget(target, reason);
    return action;
}

JSONValue ngPsdDepthImportSummaryToJson(PsdDepthImportResult imported, size_t changedGrids) {
    JSONValue[string] obj;
    obj["changedGrids"] = JSONValue(cast(long)changedGrids);
    obj["matchedLayers"] = JSONValue(cast(long)imported.matchedLayers);
    obj["unmatchedLayers"] = JSONValue(cast(long)imported.unmatchedLayers);
    obj["ambiguousLayers"] = JSONValue(cast(long)imported.ambiguousLayers);
    obj["skippedGrids"] = JSONValue(cast(long)imported.skippedGrids);
    obj["compositionMode"] = JSONValue(imported.compositionModeName);
    obj["compositionWidth"] = JSONValue(cast(long)imported.compositionWidth);
    obj["compositionHeight"] = JSONValue(cast(long)imported.compositionHeight);
    obj["colorSourceKind"] = JSONValue(imported.colorSource.kindName);
    obj["depthSourceKind"] = JSONValue(imported.depthSource.kindName);
    obj["colorLayerCount"] = JSONValue(cast(long)imported.colorLayerCount);
    obj["sourceDepthLayerCount"] = JSONValue(cast(long)imported.sourceDepthLayerCount);
    obj["composedLayerCount"] = JSONValue(cast(long)imported.composedLayerCount);
    obj["globalDepthScale"] = JSONValue(cast(double)imported.globalDepthScale);
    obj["globalDepthCentroid"] = JSONValue(cast(double)imported.globalDepthCentroid);
    obj["composedSourceMode"] = JSONValue(imported.compositionModeName);
    obj["composedSourceWidth"] = JSONValue(cast(long)imported.compositionWidth);
    obj["composedSourceHeight"] = JSONValue(cast(long)imported.compositionHeight);
    obj["composedSourceLayerCount"] = JSONValue(cast(long)imported.composedLayers.length);

    JSONValue compositionDiagnostics = JSONValue.emptyArray;
    foreach (diagnostic; imported.compositionDiagnostics) {
        JSONValue[string] entry;
        entry["type"] = JSONValue(diagnostic.type);
        entry["message"] = JSONValue(diagnostic.message);
        entry["layerPath"] = JSONValue(diagnostic.layerPath);
        entry["layerName"] = JSONValue(diagnostic.layerName);
        compositionDiagnostics.array ~= JSONValue(entry);
    }
    obj["compositionDiagnostics"] = compositionDiagnostics;

    JSONValue composedLayers = JSONValue.emptyArray;
    foreach (layer; imported.composedLayers) {
        JSONValue[string] entry;
        entry["id"] = JSONValue(layer.id);
        entry["layerPath"] = JSONValue(layer.layerPath);
        entry["layerName"] = JSONValue(layer.layerName);
        entry["colorLayerPath"] = JSONValue(layer.colorLayerPath);
        entry["colorLayerName"] = JSONValue(layer.colorLayerName);
        entry["left"] = JSONValue(cast(long)layer.left);
        entry["top"] = JSONValue(cast(long)layer.top);
        entry["width"] = JSONValue(cast(long)layer.width);
        entry["height"] = JSONValue(cast(long)layer.height);
        entry["visible"] = JSONValue(layer.visible);
        entry["enabled"] = JSONValue(layer.enabled);
        entry["depthEnabled"] = JSONValue(layer.depthEnabled);
        entry["targetGridName"] = JSONValue(layer.targetGridName);
        entry["targetGridUuid"] = JSONValue(layer.targetGridUuid);
        entry["depthMin01"] = JSONValue(cast(double)layer.depthStats.minDepth01);
        entry["depthMax01"] = JSONValue(cast(double)layer.depthStats.maxDepth01);
        entry["depthRange01"] = JSONValue(cast(double)layer.depthStats.rangeDepth01);
        entry["adjacentDelta01"] = JSONValue(cast(double)layer.depthStats.adjacentDelta01);
        entry["maskedPixels"] = JSONValue(cast(long)layer.depthStats.maskedPixels);
        entry["zeroPixels"] = JSONValue(cast(long)layer.depthStats.zeroPixels);
        entry["hasDepth"] = JSONValue(layer.depthStats.hasDepth);
        composedLayers.array ~= JSONValue(entry);
    }
    obj["composedLayers"] = composedLayers;

    JSONValue mappings = JSONValue.emptyArray;
    foreach (mapping; imported.mappings) {
        JSONValue[string] entry;
        entry["layerPath"] = JSONValue(mapping.layerPath);
        entry["layerName"] = JSONValue(mapping.layerName);
        entry["matchedNodeName"] = JSONValue(mapping.matchedNodeName);
        entry["targetGridName"] = JSONValue(mapping.targetGridName);
        entry["matched"] = JSONValue(mapping.matched);
        entry["ambiguous"] = JSONValue(mapping.ambiguous);
        entry["ignored"] = JSONValue(mapping.ignored);
        entry["manual"] = JSONValue(mapping.manual);
        entry["status"] = JSONValue(mapping.status);
        mappings.array ~= JSONValue(entry);
    }
    obj["mappings"] = mappings;

    return JSONValue(obj);
}

private DepthImageChannel psdDepthChannelToDepthImage(PsdDepthChannel channel) {
    final switch (channel) {
        case PsdDepthChannel.AverageRGB: return DepthImageChannel.AverageRGB;
        case PsdDepthChannel.R: return DepthImageChannel.R;
        case PsdDepthChannel.G: return DepthImageChannel.G;
        case PsdDepthChannel.B: return DepthImageChannel.B;
        case PsdDepthChannel.Luminance: return DepthImageChannel.Luminance;
    }
}

DepthImageConvolution ngPsdDepthConvolutionToDepthImage(PsdDepthConvolution convolution) {
    final switch (convolution) {
        case PsdDepthConvolution.Nearest: return DepthImageConvolution.Nearest;
        case PsdDepthConvolution.Box3x3: return DepthImageConvolution.Box3x3;
        case PsdDepthConvolution.Box5x5: return DepthImageConvolution.Box5x5;
        case PsdDepthConvolution.Gaussian3x3: return DepthImageConvolution.Gaussian3x3;
        case PsdDepthConvolution.Gaussian5x5: return DepthImageConvolution.Gaussian5x5;
        case PsdDepthConvolution.Median3x3: return DepthImageConvolution.Median3x3;
        case PsdDepthConvolution.Frontmost3x3: return DepthImageConvolution.Frontmost3x3;
        case PsdDepthConvolution.Backmost3x3: return DepthImageConvolution.Backmost3x3;
        case PsdDepthConvolution.BoxCustom: return DepthImageConvolution.BoxCustom;
        case PsdDepthConvolution.GaussianCustom: return DepthImageConvolution.GaussianCustom;
        case PsdDepthConvolution.MedianCustom: return DepthImageConvolution.MedianCustom;
        case PsdDepthConvolution.FrontmostCustom: return DepthImageConvolution.FrontmostCustom;
        case PsdDepthConvolution.BackmostCustom: return DepthImageConvolution.BackmostCustom;
    }
}

private PsdDepthComposedLayer* psdDepthComposedLayerByPath(
    ref PsdDepthImportResult imported,
    string layerPath,
    ulong targetGridUuid
) {
    if (targetGridUuid == 0) return null;
    foreach (ref layer; imported.composedLayers) {
        if (layer.layerPath == layerPath && layer.targetGridUuid == targetGridUuid) return &layer;
    }
    return null;
}

private DepthDrawLayer psdDepthComposedLayerToDepthDrawLayer(ref PsdDepthComposedLayer composedLayer) {
    DepthDrawLayer layer;
    layer.id = composedLayer.id.length ? composedLayer.id : composedLayer.layerPath;
    layer.sourcePath = composedLayer.sourcePath;
    layer.layerPath = composedLayer.layerPath;
    layer.displayName = composedLayer.layerName;
    layer.width = composedLayer.width;
    layer.height = composedLayer.height;
    layer.bounds = DepthDrawRect(composedLayer.left, composedLayer.top, composedLayer.width, composedLayer.height);
    layer.rgba = composedLayer.depthRgba.dup;
    layer.depthPixels = composedLayer.depthRgba.dup;
    layer.alphaMask = composedLayer.maskRgba.dup;
    layer.opacity = 1.0f;
    layer.visible = composedLayer.visible;
    layer.enabled = composedLayer.enabled;
    layer.zOffset = composedLayer.depthOffset;
    layer.zScale = composedLayer.depthScale;
    layer.backDepth = composedLayer.backDepth;
    layer.frontDepth = composedLayer.frontDepth;
    layer.sampleDepthScale = composedLayer.sourceDepthScale;
    layer.invert = composedLayer.invert;
    layer.channel = DepthImageChannel.AverageRGB;
    layer.convolution = ngPsdDepthConvolutionToDepthImage(composedLayer.convolution);
    layer.customRadius = composedLayer.customRadius;
    layer.alphaThreshold = composedLayer.alphaThreshold;
    return layer;
}

DepthDrawSession ngPsdDepthComposedSourceToDepthDrawSession(PsdDepthComposedSource source) {
    auto session = new DepthDrawSession();
    session.documentWidth = source.width;
    session.documentHeight = source.height;
    int order;
    foreach (ref composedLayer; source.layers) {
        DepthDrawLayer layer = psdDepthComposedLayerToDepthDrawLayer(composedLayer);
        if (layer.id.length == 0) layer.id = composedLayer.colorLayerPath.length ?
            composedLayer.colorLayerPath : composedLayer.layerPath;
        session.layers ~= layer;
        if (session.selectedLayerId.length == 0 && layer.enabled) {
            session.selectedLayerId = layer.id;
        }
        if (composedLayer.targetGridUuid != 0) {
            DepthDrawBinding binding;
            binding.layerId = layer.id;
            binding.targetNodeUuid = composedLayer.targetGridUuid;
            binding.targetGridUuid = composedLayer.targetGridUuid;
            binding.order = order++;
            binding.enabled = composedLayer.enabled;
            binding.useNormalLayerAlpha = false;
            binding.coverageThreshold = 0.0f;
            binding.mergePolicy = DepthMergePolicy.Frontmost;
            session.bindings ~= binding;
            if (session.selectedGridUuid == 0 && binding.enabled) {
                session.selectedGridUuid = binding.targetGridUuid;
            }
        }
    }
    return session;
}

DepthDrawSession ngPsdDepthImportResultToDepthDrawSession(PsdDepthImportResult imported) {
    return ngPsdDepthComposedSourceToDepthDrawSession(ngPsdDepthComposedSourceFromImportResult(imported));
}

DepthDrawPngExportResult ngExportPsdDepthComposedSourcePng(
    PsdDepthComposedSource source,
    string outputDir,
    string manifestPath
) {
    auto session = ngPsdDepthComposedSourceToDepthDrawSession(source);
    return ngExportDepthDrawPngSession(session, outputDir, manifestPath);
}

private void buildPsdDepthDrawCompositePreview(ref PsdDepthGridResult gridResult, DepthDrawLayer[] layers) {
    enum int MaxPreviewSize = 192;

    bool hasBounds;
    int left;
    int top;
    int right;
    int bottom;
    foreach (ref layer; layers) {
        if (!layer.enabled || !layer.visible || !layer.hasDepthPixels()) continue;
        auto layerRight = layer.bounds.left + layer.bounds.width;
        auto layerBottom = layer.bounds.top + layer.bounds.height;
        if (!hasBounds) {
            left = layer.bounds.left;
            top = layer.bounds.top;
            right = layerRight;
            bottom = layerBottom;
            hasBounds = true;
        } else {
            left = min(left, layer.bounds.left);
            top = min(top, layer.bounds.top);
            right = max(right, layerRight);
            bottom = max(bottom, layerBottom);
        }
    }
    if (!hasBounds || right <= left || bottom <= top) {
        gridResult.previewWidth = 0;
        gridResult.previewHeight = 0;
        gridResult.rawCompositePreviewRgba = null;
        gridResult.compositePreviewRgba = null;
        return;
    }

    auto sourceWidth = right - left;
    auto sourceHeight = bottom - top;
    auto scale = min(
        cast(float)MaxPreviewSize / cast(float)sourceWidth,
        cast(float)MaxPreviewSize / cast(float)sourceHeight
    );
    if (scale > 1.0f) scale = 1.0f;
    if (scale <= 0.0f) scale = 1.0f;

    auto width = max(1, cast(int)round(cast(float)sourceWidth * scale));
    auto height = max(1, cast(int)round(cast(float)sourceHeight * scale));
    gridResult.previewLeft = left;
    gridResult.previewTop = top;
    gridResult.previewWidth = width;
    gridResult.previewHeight = height;
    gridResult.rawCompositePreviewRgba.length = cast(size_t)width * cast(size_t)height * 4;
    gridResult.compositePreviewRgba.length = gridResult.rawCompositePreviewRgba.length;
    gridResult.rawCompositePreviewRgba[] = 0;
    gridResult.compositePreviewRgba[] = 0;

    foreach (py; 0 .. height) {
        foreach (px; 0 .. width) {
            auto documentPoint = vec2(
                cast(float)left + (cast(float)px + 0.5f) / scale,
                cast(float)top + (cast(float)py + 0.5f) / scale
            );
            bool hasSample;
            float bestDepth;
            float bestBackDepth = -1.0f;
            float bestFrontDepth = 1.0f;
            float bestDepthScale = 1.0f;
            foreach (ref layer; layers) {
                if (!layer.enabled || !layer.visible || !layer.hasDepthPixels()) continue;
                auto layerPoint = ngDepthDrawLayerPixelFromDocument(layer, documentPoint);
                auto sample = ngDepthImageSampleRgbaWithOpacity(
                    layer.depthPixels,
                    layer.width,
                    layer.height,
                    layerPoint.x,
                    layerPoint.y,
                    layer.opacity,
                    layer.sampleSettings()
                );
                if (!sample.valid) continue;
                auto depth = layer.applyZTransform(sample.value);
                if (!hasSample || depth > bestDepth) {
                    hasSample = true;
                    bestDepth = depth;
                    bestBackDepth = layer.backDepth;
                    bestFrontDepth = layer.frontDepth;
                    bestDepthScale = layer.sampleDepthScale;
                }
            }
            if (!hasSample) continue;
            auto depthIndex = cast(size_t)py * cast(size_t)width + cast(size_t)px;
            auto normalized = ngDepthSampleValueToDepth01(bestDepth, bestBackDepth, bestFrontDepth, bestDepthScale);
            auto gray = cast(ubyte)round(normalized * 255.0f);
            auto outIndex = depthIndex * 4;
            gridResult.rawCompositePreviewRgba[outIndex + 0] = gray;
            gridResult.rawCompositePreviewRgba[outIndex + 1] = gray;
            gridResult.rawCompositePreviewRgba[outIndex + 2] = gray;
            gridResult.rawCompositePreviewRgba[outIndex + 3] = 255;
            gridResult.compositePreviewRgba[outIndex + 0] = gray;
            gridResult.compositePreviewRgba[outIndex + 1] = gray;
            gridResult.compositePreviewRgba[outIndex + 2] = gray;
            gridResult.compositePreviewRgba[outIndex + 3] = 255;
        }
    }
}

private DepthDrawSession buildPsdDepthDrawSessionForGrid(
    ref PsdDepthImportResult imported,
    ref PsdDepthGridResult gridResult,
    out string[string] layerIdToPath,
    out DepthDrawLayer[] previewLayers
) {
    auto session = new DepthDrawSession();
    int order;
    foreach (layerMask; gridResult.layerMasks) {
        auto composedLayer = psdDepthComposedLayerByPath(imported, layerMask.layerPath, gridResult.grid.uuid);
        if (composedLayer is null || composedLayer.depthRgba.length == 0) continue;

        void appendLayer(ref PsdDepthComposedLayer source, string id) {
            auto layer = psdDepthComposedLayerToDepthDrawLayer(source);
            layer.id = id;
            session.layers ~= layer;
            previewLayers ~= layer;
            layerIdToPath[layer.id] = layerMask.layerPath;

            DepthDrawBinding binding;
            binding.layerId = layer.id;
            binding.targetNodeUuid = gridResult.grid.uuid;
            binding.targetGridUuid = gridResult.grid.uuid;
            binding.order = order++;
            binding.enabled = true;
            binding.useNormalLayerAlpha = false;
            binding.coverageThreshold = 0.0f;
            binding.mergePolicy = DepthMergePolicy.Frontmost;
            session.bindings ~= binding;
        }

        if (composedLayer.depthEnabled) {
            auto id = composedLayer.id.length ? composedLayer.id : composedLayer.layerPath;
            appendLayer(*composedLayer, id);
            continue;
        }

        size_t composedIndex = size_t.max;
        foreach (i, ref candidate; imported.composedLayers) {
            if (&candidate is composedLayer) {
                composedIndex = i;
                break;
            }
        }
        if (composedIndex == size_t.max) continue;

        ubyte[][size_t] depthPixelsBySource;
        size_t[] sourceIndices;
        foreach (y; 0 .. composedLayer.height) {
            foreach (x; 0 .. composedLayer.width) {
                auto targetIndex = (cast(size_t)y * cast(size_t)composedLayer.width + cast(size_t)x) * 4;
                if (targetIndex + 3 >= composedLayer.maskRgba.length ||
                    composedLayer.maskRgba[targetIndex + 3] == 0) continue;
                if (composedLayer.colorRgba.length >= targetIndex + 4 &&
                    composedLayer.colorRgba[targetIndex + 3] < 3) continue;

                ptrdiff_t sourceIndex;
                ubyte[4] sourcePixel;
                if (!ngPsdDepthResolvedPixelAt(imported, composedIndex,
                    composedLayer.left + x, composedLayer.top + y, sourceIndex, sourcePixel)) continue;
                auto sourceKey = cast(size_t)sourceIndex;
                auto pixels = sourceKey in depthPixelsBySource;
                if (pixels is null) {
                    depthPixelsBySource[sourceKey] = new ubyte[cast(size_t)composedLayer.width *
                        cast(size_t)composedLayer.height * 4];
                    sourceIndices ~= sourceKey;
                    pixels = sourceKey in depthPixelsBySource;
                }
                (*pixels)[targetIndex .. targetIndex + 4] = sourcePixel[];
            }
        }

        foreach (sourceIndex; sourceIndices) {
            auto resolved = imported.composedLayers[sourceIndex];
            resolved.id = (composedLayer.id.length ? composedLayer.id : composedLayer.layerPath) ~
                "\x1Fattached:" ~ sourceIndex.to!string;
            resolved.layerPath = composedLayer.layerPath;
            resolved.layerName = composedLayer.layerName;
            resolved.left = composedLayer.left;
            resolved.top = composedLayer.top;
            resolved.width = composedLayer.width;
            resolved.height = composedLayer.height;
            resolved.visible = composedLayer.visible;
            resolved.enabled = composedLayer.enabled;
            resolved.colorRgba = composedLayer.colorRgba;
            resolved.maskRgba = composedLayer.maskRgba;
            resolved.coverageMaskRgba = composedLayer.coverageMaskRgba;
            resolved.depthRgba = depthPixelsBySource[sourceIndex];
            appendLayer(resolved, resolved.id);
        }
    }
    return session;
}

private void updatePsdDepthGridRange(ref PsdDepthGridResult gridResult) {
    bool hasDepth;
    foreach (depth; gridResult.depths) {
        if (!depth.isFinite) continue;
        if (!hasDepth) {
            gridResult.minDepth = depth;
            gridResult.maxDepth = depth;
            hasDepth = true;
        } else {
            gridResult.minDepth = min(gridResult.minDepth, depth);
            gridResult.maxDepth = max(gridResult.maxDepth, depth);
        }
    }
    if (!hasDepth) {
        gridResult.minDepth = 0.0f;
        gridResult.maxDepth = 0.0f;
    }
}

private void applyPsdDepthMissingPolicy(
    ref PsdDepthGridResult composed,
    ref PsdDepthImportResult imported,
    const(float)[] existingDepths
) {
    Vec2Array vertices;
    if (composed.grid !is null) vertices = composed.grid.vertices;
    foreach (i, isMissing; composed.missingVertexMask) {
        if (!isMissing) continue;
        final switch (imported.missingPolicy) {
            case PsdDepthMissingPolicy.KeepExisting:
                bool foundSample;
                float nearestDepth;
                float nearestDistanceSquared;
                if (vertices.length == composed.depths.length && i < vertices.length) {
                    foreach (j, candidateDepth; composed.depths) {
                        if (j >= composed.missingVertexMask.length || composed.missingVertexMask[j] ||
                            !candidateDepth.isFinite) continue;
                        auto dx = vertices[j].x - vertices[i].x;
                        auto dy = vertices[j].y - vertices[i].y;
                        auto distanceSquared = dx * dx + dy * dy;
                        if (foundSample && distanceSquared >= nearestDistanceSquared) continue;
                        foundSample = true;
                        nearestDepth = candidateDepth;
                        nearestDistanceSquared = distanceSquared;
                    }
                }
                if (foundSample) {
                    composed.depths[i] = nearestDepth;
                } else if (i < existingDepths.length) {
                    composed.depths[i] = existingDepths[i];
                }
                break;
            case PsdDepthMissingPolicy.SetZero:
                composed.depths[i] = 0.0f;
                break;
            case PsdDepthMissingPolicy.SetBack:
                auto backDepth = imported.missingBackDepth * imported.globalDepthScale;
                composed.depths[i] = backDepth.isFinite ? backDepth : 0.0f;
                break;
            case PsdDepthMissingPolicy.SkipGrid:
                composed.skipped = true;
                composed.depths = existingDepths.dup;
                return;
        }
    }
}

private void smoothPsdDepthGridResult(ref PsdDepthGridResult composed, ref PsdDepthImportResult imported) {
    if (!imported.smoothWavySurface || composed.grid is null || composed.depths.length == 0) return;
    auto vertices = composed.grid.vertices;
    if (vertices.length != composed.depths.length) return;

    size_t cols = vertices.length;
    size_t rows = 1;
    if (cast(GridDeformer)composed.grid !is null && vertices.length > 1) {
        cols = 1;
        auto firstY = vertices[0].y;
        while (cols < vertices.length && abs(vertices[cols].y - firstY) <= 0.00001f) cols++;
        if (cols == 0 || vertices.length % cols != 0) {
            cols = vertices.length;
        } else {
            rows = vertices.length / cols;
        }
    }

    ubyte[] valid;
    int[] groups;
    valid.length = composed.depths.length;
    groups.length = composed.depths.length;
    groups[] = -1;
    int[string] groupByLayer;
    int nextGroup;
    foreach (i; 0 .. composed.depths.length) {
        if (i >= composed.winnerLayerPaths.length || composed.winnerLayerPaths[i].length == 0) continue;
        valid[i] = 1;
        auto path = composed.winnerLayerPaths[i];
        auto group = path in groupByLayer;
        if (group is null) {
            groupByLayer[path] = nextGroup;
            groups[i] = nextGroup++;
        } else {
            groups[i] = *group;
        }
    }
    float effectiveDepthScale = 0.0001f;
    foreach (ref layer; imported.composedLayers) {
        effectiveDepthScale = max(effectiveDepthScale,
            abs((layer.frontDepth - layer.backDepth) * layer.sourceDepthScale * layer.depthScale));
    }
    composed.depths = ngDepthDrawSmoothGridDepthValues(
        composed.depths, valid, groups, cast(int)cols, cast(int)rows, 18.0f, effectiveDepthScale);
}

bool ngComposePsdDepthTarget(
    ref PsdDepthImportResult imported,
    ref PsdDepthGridResult gridResult,
    out PsdDepthGridResult composed,
    out string error
) {
    composed = gridResult;
    error = null;
    if (gridResult.grid is null || gridResult.skipped) return true;
    auto depthMapped = cast(DepthMappedNode)gridResult.grid;
    if (depthMapped is null) {
        error = "PSD depth map composition target does not support depth maps";
        return false;
    }

    string[string] layerIdToPath;
    DepthDrawLayer[] previewLayers;
    auto session = buildPsdDepthDrawSessionForGrid(imported, gridResult, layerIdToPath, previewLayers);
    if (session.layers.length == 0) {
        error = "PSD depth map composition has no enabled source layers for target";
        return false;
    }

    auto target = new DepthTargetView(gridResult.grid);
    auto oldDepths = depthMapped.copyDepths();
    if (oldDepths is null || oldDepths.length != gridResult.grid.vertices.length) {
        oldDepths.length = gridResult.grid.vertices.length;
        oldDepths[] = 0.0f;
    }
    target.baseDepths = oldDepths.dup;
    target.depths = oldDepths.dup;

    auto result = ngComposeDepthDrawTarget(session, target, gridResult.documentWidth, gridResult.documentHeight);
    return finalizePsdDepthTarget(
        imported, gridResult, oldDepths, layerIdToPath, previewLayers, result, composed, error);
}

private bool finalizePsdDepthTarget(
    ref PsdDepthImportResult imported,
    ref PsdDepthGridResult gridResult,
    const(float)[] oldDepths,
    ref string[string] layerIdToPath,
    DepthDrawLayer[] previewLayers,
    ref DepthDrawComposeResult result,
    out PsdDepthGridResult composed,
    out string error
) {
    composed = gridResult;
    error = null;
    if (gridResult.grid is null || result.depths.length != gridResult.grid.vertices.length) {
        error = "PSD depth map composition depth count mismatch";
        return false;
    }
    composed.depths = result.depths.dup;
    composed.baseDepths = oldDepths.dup;
    composed.winnerLayerPaths.length = composed.depths.length;
    composed.missingVertexMask.length = composed.depths.length;
    composed.sampledVertices = 0;
    composed.missingVertices = 0;
    foreach (i; 0 .. composed.depths.length) {
        auto winnerId = i < result.winningLayerIds.length ? result.winningLayerIds[i] : null;
        auto hasWinner = winnerId.length > 0;
        composed.missingVertexMask[i] = !hasWinner;
        if (hasWinner) {
            composed.sampledVertices++;
            if (auto layerPath = winnerId in layerIdToPath) {
                composed.winnerLayerPaths[i] = *layerPath;
            } else {
                composed.winnerLayerPaths[i] = winnerId;
            }
        } else {
            composed.missingVertices++;
            composed.winnerLayerPaths[i] = null;
        }
    }
    applyPsdDepthMissingPolicy(composed, imported, oldDepths);
    if (!composed.skipped) smoothPsdDepthGridResult(composed, imported);
    updatePsdDepthGridRange(composed);
    foreach (ref layerMask; composed.layerMasks) {
        layerMask.sampledVertices = 0;
        layerMask.selectedVertices = 0;
        foreach (stats; result.layerStats) {
            string statsLayerPath = stats.layerId;
            if (auto path = stats.layerId in layerIdToPath) statsLayerPath = *path;
            if (statsLayerPath != layerMask.layerPath) continue;
            layerMask.sampledVertices += stats.sampledVertices;
            layerMask.selectedVertices += stats.winningVertices;
        }
    }
    buildPsdDepthDrawCompositePreview(composed, previewLayers);
    return true;
}

bool ngComposePsdDepthImportResult(
    ref PsdDepthImportResult imported,
    out PsdDepthComposedView composed,
    out string error
) {
    composed = PsdDepthComposedView.init;
    error = null;
    if (imported.gpuCompositionRequested) {
        if (!composePsdDepthImportGpu(imported, error)) return false;
    } else {
        foreach (i, ref gridResult; imported.grids) {
            PsdDepthGridResult composedGrid;
            if (!ngComposePsdDepthTarget(imported, gridResult, composedGrid, error)) return false;
            imported.grids[i] = composedGrid;
        }
    }
    composed.imported = &imported;
    return true;
}

bool ngSubmitPsdDepthImportGpu(
    ref PsdDepthImportResult imported,
    out PsdDepthGpuComposeWork work,
    out string error
) {
    work = PsdDepthGpuComposeWork.init;
    error = null;
    work.composedGrids = imported.grids.dup;
    foreach (i, ref gridResult; imported.grids) {
        if (gridResult.grid is null || gridResult.skipped) continue;
        auto depthMapped = cast(DepthMappedNode)gridResult.grid;
        if (depthMapped is null) {
            error = "PSD depth map GPU composition target does not support depth maps";
            ngCancelPsdDepthImportGpu(work);
            return false;
        }

        string[string] layerIdToPath;
        DepthDrawLayer[] previewLayers;
        auto session = buildPsdDepthDrawSessionForGrid(imported, gridResult, layerIdToPath, previewLayers);
        if (session.layers.length == 0) {
            error = "PSD depth map GPU composition has no enabled source layers for target";
            ngCancelPsdDepthImportGpu(work);
            return false;
        }

        auto target = new DepthTargetView(gridResult.grid);
        auto oldDepths = depthMapped.copyDepths();
        if (oldDepths is null || oldDepths.length != gridResult.grid.vertices.length) {
            oldDepths.length = gridResult.grid.vertices.length;
            oldDepths[] = 0.0f;
        }
        target.baseDepths = oldDepths.dup;
        target.depths = oldDepths.dup;

        DepthDrawGpuTargetComposeJob job;
        if (!ngSubmitDepthDrawGpuTargetCompose(session, target, gridResult.documentWidth, gridResult.documentHeight,
            job, error)) {
            ngCancelPsdDepthImportGpu(work);
            if (error.length == 0) error = "CPU fallback is disabled for PSD depth map GPU composition";
            else error = "CPU fallback is disabled for PSD depth map GPU composition: " ~ error;
            return false;
        }
        PsdDepthGpuGridComposeWork gridWork;
        gridWork.gridIndex = i;
        gridWork.sourceGrid = gridResult;
        gridWork.oldDepths = oldDepths;
        gridWork.layerIdToPath = layerIdToPath;
        gridWork.previewLayers = previewLayers;
        gridWork.job = job;
        work.grids ~= gridWork;
    }
    work.active = true;
    return true;
}

bool ngPollPsdDepthImportGpu(
    ref PsdDepthImportResult imported,
    ref PsdDepthGpuComposeWork work,
    out bool ready,
    out PsdDepthComposedView composed,
    out string error
) {
    ready = false;
    composed = PsdDepthComposedView.init;
    error = null;
    if (!work.active) {
        error = "PSD depth map GPU composition work is not active";
        return false;
    }

    bool pending;
    foreach (ref gridWork; work.grids) {
        if (gridWork.completed) continue;
        DepthDrawGpuTargetComposePollResult pollResult;
        if (!ngPollDepthDrawGpuTargetCompose(gridWork.job, pollResult, error)) {
            ngCancelPsdDepthImportGpu(work);
            return false;
        }
        if (!pollResult.ready) {
            pending = true;
            continue;
        }

        PsdDepthGridResult composedGrid;
        if (!finalizePsdDepthTarget(imported, gridWork.sourceGrid, gridWork.oldDepths,
            gridWork.layerIdToPath, gridWork.previewLayers, pollResult.result, composedGrid, error)) {
            ngCancelPsdDepthImportGpu(work);
            return false;
        }
        work.composedGrids[gridWork.gridIndex] = composedGrid;
        gridWork.completed = true;
        gridWork.job = DepthDrawGpuTargetComposeJob.init;
    }
    if (pending) return true;

    imported.grids = work.composedGrids;
    work = PsdDepthGpuComposeWork.init;
    composed.imported = &imported;
    ready = true;
    return true;
}

void ngCancelPsdDepthImportGpu(ref PsdDepthGpuComposeWork work) {
    foreach (ref gridWork; work.grids) {
        if (!gridWork.completed && gridWork.job.jobId != 0) {
            ngCancelDepthDrawGpuTargetCompose(gridWork.job);
        }
    }
    work = PsdDepthGpuComposeWork.init;
}

private bool composePsdDepthImportGpu(ref PsdDepthImportResult imported, out string error) {
    PsdDepthGpuComposeWork work;
    if (!ngSubmitPsdDepthImportGpu(imported, work, error)) return false;
    foreach (_; 0 .. 30_000) {
        bool ready;
        PsdDepthComposedView composed;
        if (!ngPollPsdDepthImportGpu(imported, work, ready, composed, error)) return false;
        if (ready) return true;
        Thread.sleep(1.msecs);
    }
    ngCancelPsdDepthImportGpu(work);
    error = "PSD depth map GPU composition timed out";
    return false;
}

ExCommandResult!JSONValue ngApplyPsdDepthImportResult(PsdDepthComposedView composed) {
    size_t changedGrids;
    string validationError;
    if (!ngCanApplyPsdDepthImportResult(composed, validationError)) {
        return ExCommandResult!JSONValue(false, JSONValue(null), validationError);
    }
    auto imported = composed.imported;
    foreach (ref gridResult; imported.grids) {
        ngNormalizeDepths(gridResult.depths);
        ngNormalizeDepths(gridResult.baseDepths);
        updatePsdDepthGridRange(gridResult);
    }

    if (imported.grids.length > 0) {
        ngGuardActionStackScopes();
        auto group = new PsdDepthImportChangeAction();
        ngBeginDepthBoneRefreshActionSink(group);
        scope(exit) ngEndDepthBoneRefreshActionSink(group);

        Node[ulong] appliedTargets;
        foreach (gridResult; imported.grids) {
            if (!gridResult.skipped && gridResult.grid !is null) {
                appliedTargets[gridResult.grid.uuid] = gridResult.grid;
            }
        }

        bool[ulong] sourceTransformChangedTargets;
        foreach (root; incActivePuppet().findNodesType!ExDepthRigRoot(incActivePuppet().root)) {
            auto oldBindings = copyDepthRigBindings(root.bindings);
            auto nextBindings = copyDepthRigBindings(root.bindings);
            bool bindingsChanged;
            foreach (ref binding; nextBindings) {
                if (binding.targetUuid !in appliedTargets) continue;
                foreach (ref setting; binding.sourceSettings) {
                    if (setting.depthScale == 1.0f && setting.depthOffset == 0.0f) continue;
                    setting.depthScale = 1.0f;
                    setting.depthOffset = 0.0f;
                    bindingsChanged = true;
                    sourceTransformChangedTargets[binding.targetUuid] = true;
                }
            }
            if (!bindingsChanged) continue;
            root.bindings = copyDepthRigBindings(nextBindings);
            group.addAction(new DepthRigBindingsChangeAction(
                "Import PSD Depth Map",
                root,
                oldBindings,
                nextBindings
            ));
        }

        foreach (gridResult; imported.grids) {
            if (gridResult.skipped) continue;
            if (gridResult.grid is null) continue;
            auto depthMapped = cast(DepthMappedNode)gridResult.grid;
            auto depthChanged = depthMapped.copyDepths() != gridResult.depths;
            auto depthOperated = cast(DepthOperationMappedNode)gridResult.grid;
            auto hasDepthOps = depthOperated !is null && depthOperated.copyDepthOps().length > 0;
            auto sourceTransformChanged = gridResult.grid.uuid in sourceTransformChangedTargets;
            if (!depthChanged && !hasDepthOps && sourceTransformChanged is null) continue;
            auto clearOps = ngClearDepthOpsChangeAction(gridResult.grid, "Import PSD Depth Map");
            if (clearOps !is null) group.addAction(clearOps);
            if (depthChanged) {
                auto depthAction = ngApplyDepthsChangeAction(gridResult.grid, gridResult.depths, "Import PSD Depth Map");
                group.addAction(depthAction);
            } else if (clearOps is null && sourceTransformChanged !is null) {
                ngMarkDepthBoneDirtyForTarget(gridResult.grid, "Import PSD Depth Map");
            }
            changedGrids++;
        }
        if (ngHasPendingDepthBoneRefreshForSink(group)) {
            if (!group.empty()) incActionPush(group);
            activePsdDepthImportProgress = new AsyncGroupActionProgress(
                group,
                _("Finalizing PSD depth import..."),
                _("PSD depth map imported"),
                null,
                { return ngPendingDepthBoneRefreshWorkForSink(group); },
                { ngFlushDepthBoneDirty(); },
            );
        } else if (!group.empty()) {
            incActionPush(group);
        }
    }

    return ExCommandResult!JSONValue(
        true,
        ngPsdDepthImportSummaryToJson(*imported, changedGrids),
        "PSD depth map imported"
    );
}

bool ngCanApplyPsdDepthImportResult(PsdDepthComposedView composed, out string error) {
    error = null;
    if (composed.imported is null) {
        error = "PSD depth import apply requires a composed view";
        return false;
    }
    if (activePsdDepthImportProgress !is null && activePsdDepthImportProgress.isActive) {
        error = "PSD depth map import is still finalizing";
        return false;
    }
    foreach (gridResult; composed.imported.grids) {
        if (gridResult.skipped || gridResult.grid is null) continue;
        if (gridResult.depths.length != gridResult.grid.vertices.length) {
            error = "Imported depths length must match target vertices";
            return false;
        }
        if (cast(DepthMappedNode)gridResult.grid is null) {
            error = "Imported target must support depth maps";
            return false;
        }
    }
    return true;
}

private ExDepthRigBinding[] copyDepthRigBindings(ExDepthRigBinding[] bindings) {
    auto result = bindings.dup;
    foreach (ref binding; result) {
        binding.sourceBoneUuids = binding.sourceBoneUuids.dup;
        binding.sourceSettings = binding.sourceSettings.dup;
        binding.influenceRule.multipliersByBoneUuid = binding.influenceRule.multipliersByBoneUuid.dup;
    }
    return result;
}

private void replaceDepthOpsWithUndo(Node target, ExDepthOp[] nextOps, string reason) {
    auto operated = requireDepthOperated(target);
    auto action = new DepthOperationMappedChangeAction(target);
    auto previousOps = operated.copyDepthOps();
    if (nextOps.length == 0) {
        operated.replaceDepthOpBaseDepths(null);
    } else if (previousOps.length == 0) {
        auto mapped = cast(DepthMappedNode)target;
        operated.replaceDepthOpBaseDepths(mapped is null ? null : mapped.copyDepths());
    }
    operated.replaceDepthOps(nextOps);
    action.updateNewState();
    incActionPush(action);
    ngMarkDepthBoneDirtyForTarget(target, reason);
}

@EffectApply
class ListDepthsCommand : ExCommand!(TW!(Node, "target", "DepthMapped target node")) {
    this() { super(_("List Depths"), _("List per-vertex depth values")); }

    override ExCommandResult!JSONValue run(Context ctx) {
        auto depths = requireDepthMapped(target).copyDepths();
        JSONValue[string] obj;
        obj["target"] = JSONValue(target.uuid);
        obj["depths"] = depthsToJson(depths);
        obj["count"] = JSONValue(depths is null ? -1L : cast(long)depths.length);
        return ExCommandResult!JSONValue(true, JSONValue(obj));
    }
}

@EffectApply
class SetDepthsCommand : ExCommand!(
    TW!(Node, "target", "DepthMapped target node"),
    TW!(float[], "depths", "Per-vertex depth values; length must match target vertices")
) {
    this() { super(_("Set Depths"), _("Set per-vertex depth values")); }

    override CommandResult run(Context ctx) {
        auto mapped = requireDepthMapped(target);
        auto deformable = cast(Deformable)target;
        enforce(deformable !is null, "target must be a Deformable");
        enforce(depths.length == deformable.vertices.length, "depths length must match target vertices");
        replaceDepthsWithUndo(target, depths, "Set Depths");
        return CommandResult(true);
    }
}

@EffectApply
class ClearDepthsCommand : ExCommand!(TW!(Node, "target", "DepthMapped target node")) {
    this() { super(_("Clear Depths"), _("Clear per-vertex depth values")); }

    override CommandResult run(Context ctx) {
        replaceDepthsWithUndo(target, null, "Clear Depths");
        return CommandResult(true);
    }
}

@EffectApply
class ListDepthOpsCommand : ExCommand!(TW!(Node, "target", "Depth operation target node")) {
    this() { super(_("List Depth Operations"), _("List saved depth operations")); }

    override ExCommandResult!JSONValue run(Context ctx) {
        auto ops = requireDepthOperated(target).copyDepthOps();
        JSONValue[string] obj;
        obj["target"] = JSONValue(target.uuid);
        obj["operations"] = depthOpsToJson(ops);
        obj["count"] = JSONValue(cast(long)ops.length);
        return ExCommandResult!JSONValue(true, JSONValue(obj));
    }
}

@EffectApply
class SetDepthOpsCommand : ExCommand!(
    TW!(Node, "target", "Depth operation target node"),
    TW!(JSONValue, "operations", "Depth operations array", false, CommandJsonSchema.depthOperations)
) {
    this() { super(_("Set Depth Operations"), _("Replace saved depth operations")); }

    override CommandResult run(Context ctx) {
        replaceDepthOpsWithUndo(target, depthOpsFromJson(operations), "Set Depth Operations");
        return CommandResult(true);
    }
}

@EffectApply
class AddDepthOpCommand : ExCommand!(
    TW!(Node, "target", "Depth operation target node"),
    TW!(JSONValue, "operation", "Depth operation object", false, CommandJsonSchema.depthOperation),
    TW!(int, "index", "Insertion index, or -1 to append")
) {
    this() { super(_("Add Depth Operation"), _("Add one saved depth operation")); }

    override CommandResult run(Context ctx) {
        auto operated = requireDepthOperated(target);
        auto ops = operated.copyDepthOps();
        auto op = depthOpFromJson(operation);
        if (index < 0 || index >= ops.length) {
            ops ~= op;
        } else {
            auto i = cast(size_t)index;
            ops = ops[0 .. i] ~ [op] ~ ops[i .. $];
        }
        replaceDepthOpsWithUndo(target, ops, "Add Depth Operation");
        return CommandResult(true);
    }
}

@EffectApply
class UpdateDepthOpCommand : ExCommand!(
    TW!(Node, "target", "Depth operation target node"),
    TW!(int, "index", "Operation index"),
    TW!(JSONValue, "operation", "Replacement depth operation object", false, CommandJsonSchema.depthOperation)
) {
    this() { super(_("Update Depth Operation"), _("Replace one saved depth operation")); }

    override CommandResult run(Context ctx) {
        auto operated = requireDepthOperated(target);
        auto ops = operated.copyDepthOps();
        enforce(index >= 0 && index < ops.length, "operation index out of range");
        ops[cast(size_t)index] = depthOpFromJson(operation);
        replaceDepthOpsWithUndo(target, ops, "Update Depth Operation");
        return CommandResult(true);
    }
}

@EffectApply
class RemoveDepthOpCommand : ExCommand!(
    TW!(Node, "target", "Depth operation target node"),
    TW!(int, "index", "Operation index")
) {
    this() { super(_("Remove Depth Operation"), _("Remove one saved depth operation")); }

    override CommandResult run(Context ctx) {
        auto operated = requireDepthOperated(target);
        auto ops = operated.copyDepthOps();
        enforce(index >= 0 && index < ops.length, "operation index out of range");
        auto i = cast(size_t)index;
        ops = ops[0 .. i] ~ ops[i + 1 .. $];
        replaceDepthOpsWithUndo(target, ops, "Remove Depth Operation");
        return CommandResult(true);
    }
}

@EffectApply
class MoveDepthOpCommand : ExCommand!(
    TW!(Node, "target", "Depth operation target node"),
    TW!(int, "fromIndex", "Current operation index"),
    TW!(int, "toIndex", "Destination operation index")
) {
    this() { super(_("Move Depth Operation"), _("Move one saved depth operation")); }

    override CommandResult run(Context ctx) {
        auto operated = requireDepthOperated(target);
        auto ops = operated.copyDepthOps();
        enforce(fromIndex >= 0 && fromIndex < ops.length, "fromIndex out of range");
        enforce(toIndex >= 0 && toIndex < ops.length, "toIndex out of range");
        auto op = ops[cast(size_t)fromIndex];
        auto from = cast(size_t)fromIndex;
        ops = ops[0 .. from] ~ ops[from + 1 .. $];
        auto to = cast(size_t)toIndex;
        ops = ops[0 .. to] ~ [op] ~ ops[to .. $];
        replaceDepthOpsWithUndo(target, ops, "Move Depth Operation");
        return CommandResult(true);
    }
}

@EffectApply
class ClearDepthOpsCommand : ExCommand!(TW!(Node, "target", "Depth operation target node")) {
    this() { super(_("Clear Depth Operations"), _("Clear saved depth operations")); }

    override CommandResult run(Context ctx) {
        replaceDepthOpsWithUndo(target, null, "Clear Depth Operations");
        return CommandResult(true);
    }
}

@EffectApply
class ApplyDepthOpsCommand : ExCommand!(TW!(Node, "target", "Depth operation target node")) {
    this() { super(_("Apply Depth Operations"), _("Bake saved depth operations into per-vertex depths")); }

    override CommandResult run(Context ctx) {
        auto grid = requireDepthGrid(target);
        auto operated = requireDepthOperated(target);
        auto ops = operated.copyDepthOps();
        replaceDepthsWithUndo(target,
            ngComputeDepthsFromOps(grid, ops, operated.copyDepthOpBaseDepths()),
            "Apply Depth Operations");
        return CommandResult(true);
    }
}

@EffectApply
class ImportPSDDepthsCommand : ExCommand!(
    TW!(string, "path", "Path to PSD or PNG depth source file"),
    TW!(bool, "invert", "Invert depth values; default is white as front"),
    TW!(float, "backDepth", "Depth value for black/back"),
    TW!(float, "frontDepth", "Depth value for white/front"),
    TW!(float, "depthScale", "Multiplier applied to imported depth values"),
    TW!(string, "convolution", "Sampling convolution mode"),
    TW!(string, "channel", "Depth channel"),
    TW!(int, "customRadius", "Custom convolution radius"),
    TW!(float, "alphaThreshold", "Minimum alpha for valid depth pixels"),
    TW!(string, "missingPolicy", "Policy for vertices with no sampled depth"),
    TW!(bool, "matchDirectGridName", "Allow direct PSD layer name to GridDeformer name matching")
) {
    this(
        string path,
        bool invert = false,
        float backDepth = -1.0f,
        float frontDepth = 1.0f,
        float depthScale = 1.0f,
        string convolution = "Gaussian3x3",
        string channel = "AverageRGB",
        int customRadius = 3,
        float alphaThreshold = 0.01f,
        string missingPolicy = "KeepExisting",
        bool matchDirectGridName = true
    ) {
        super(
            _("Import PSD Depth Map"),
            _("Import PSD or PNG grayscale depth data into mapped depth targets."),
            path,
            invert,
            backDepth,
            frontDepth,
            depthScale,
            convolution,
            channel,
            customRadius,
            alphaThreshold,
            missingPolicy,
            matchDirectGridName
        );
    }

    override
    ExCommandResult!JSONValue run(Context ctx) {
        auto puppet = incActivePuppet();
        if (puppet is null) return ExCommandResult!JSONValue(false, JSONValue(null), "No active puppet");
        if (!path.length) return ExCommandResult!JSONValue(false, JSONValue(null), "Path not provided");

        PsdDepthImportSettings settings;
        settings.invert = invert;
        settings.backDepth = backDepth;
        settings.frontDepth = frontDepth;
        settings.depthScale = depthScale;
        settings.alphaThreshold = alphaThreshold;
        settings.convolution = ngPsdDepthConvolutionFromString(convolution);
        settings.channel = ngPsdDepthChannelFromString(channel);
        settings.customRadius = customRadius;
        settings.missingPolicy = ngPsdDepthMissingPolicyFromString(missingPolicy);
        settings.matchDirectGridName = matchDirectGridName;

        auto imported = ngBuildPsdDepthsFromSource(puppet, path, settings);
        PsdDepthComposedView composed;
        string composeError;
        if (!ngComposePsdDepthImportResult(imported, composed, composeError)) {
            return ExCommandResult!JSONValue(
                false,
                ngPsdDepthImportSummaryToJson(imported, 0),
                "PSD depth map composition failed: " ~ composeError
            );
        }
        return ngApplyPsdDepthImportResult(composed);
    }
}

void ngInitCommands(T)() if (is(T == DepthMapCommand)) {
    auto listDepths = new ListDepthsCommand();
    ngRegisterCommandMeta(listDepths);
    commands[DepthMapCommand.ListDepths] = listDepths;

    auto setDepths = new SetDepthsCommand();
    ngRegisterCommandMeta(setDepths);
    commands[DepthMapCommand.SetDepths] = setDepths;

    auto clearDepths = new ClearDepthsCommand();
    ngRegisterCommandMeta(clearDepths);
    commands[DepthMapCommand.ClearDepths] = clearDepths;

    auto listOps = new ListDepthOpsCommand();
    ngRegisterCommandMeta(listOps);
    commands[DepthMapCommand.ListDepthOps] = listOps;

    auto setOps = new SetDepthOpsCommand();
    ngRegisterCommandMeta(setOps);
    commands[DepthMapCommand.SetDepthOps] = setOps;

    auto addOp = new AddDepthOpCommand();
    ngRegisterCommandMeta(addOp);
    commands[DepthMapCommand.AddDepthOp] = addOp;

    auto updateOp = new UpdateDepthOpCommand();
    ngRegisterCommandMeta(updateOp);
    commands[DepthMapCommand.UpdateDepthOp] = updateOp;

    auto removeOp = new RemoveDepthOpCommand();
    ngRegisterCommandMeta(removeOp);
    commands[DepthMapCommand.RemoveDepthOp] = removeOp;

    auto moveOp = new MoveDepthOpCommand();
    ngRegisterCommandMeta(moveOp);
    commands[DepthMapCommand.MoveDepthOp] = moveOp;

    auto clearOps = new ClearDepthOpsCommand();
    ngRegisterCommandMeta(clearOps);
    commands[DepthMapCommand.ClearDepthOps] = clearOps;

    auto applyOps = new ApplyDepthOpsCommand();
    ngRegisterCommandMeta(applyOps);
    commands[DepthMapCommand.ApplyDepthOps] = applyOps;

    auto importPsdDepths = new ImportPSDDepthsCommand(
        "", false, -1.0f, 1.0f, 1.0f, "Gaussian3x3", "AverageRGB", 3, 0.01f, "KeepExisting", true
    );
    ngRegisterCommandMeta(importPsdDepths);
    commands[DepthMapCommand.ImportPSDDepths] = importPsdDepths;
}
