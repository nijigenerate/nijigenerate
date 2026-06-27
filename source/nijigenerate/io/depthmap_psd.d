module nijigenerate.io.depthmap_psd;

import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.ext.nodes.expart;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import psd;
import std.algorithm : sort;
import std.algorithm.comparison : max;
import std.array : array;
import std.conv : to;
import std.exception : enforce;
import std.math : exp, isFinite, round;
import std.path : baseName;
import std.stdio : File;
import std.string : format;

enum PsdDepthConvolution {
    Nearest,
    Box3x3,
    Box5x5,
    Gaussian3x3,
    Gaussian5x5,
    Median3x3,
    Frontmost3x3,
    Backmost3x3,
    BoxCustom,
    GaussianCustom,
    MedianCustom,
    FrontmostCustom,
    BackmostCustom,
}

enum PsdDepthChannel {
    AverageRGB,
    R,
    G,
    B,
    Luminance,
}

enum PsdDepthMissingPolicy {
    KeepExisting,
    SetZero,
    SetBack,
    SkipGrid,
}

struct PsdDepthImportSettings {
    bool invert = false;
    float backDepth = -1.0f;
    float frontDepth = 1.0f;
    float depthScale = 1.0f;
    float alphaThreshold = 0.01f;
    bool matchDirectGridName = true;
    int customRadius = 3;
    PsdDepthConvolution convolution = PsdDepthConvolution.Gaussian3x3;
    PsdDepthChannel channel = PsdDepthChannel.AverageRGB;
    PsdDepthMissingPolicy missingPolicy = PsdDepthMissingPolicy.KeepExisting;
    string[string] layerTargetGridUuidOverrides;
    bool[string] ignoredLayerPaths;
}

struct PsdDepthLayerMapping {
    string layerPath;
    string layerName;
    string matchedNodeName;
    string targetGridName;
    ulong matchedNodeUuid;
    ulong targetGridUuid;
    bool matched;
    bool ambiguous;
    bool ignored;
    bool manual;
    string status;
}

struct PsdDepthGridResult {
    GridDeformer grid;
    float[] depths;
    PsdDepthGridLayerMask[] layerMasks;
    size_t sampledVertices;
    size_t missingVertices;
    float minDepth;
    float maxDepth;
    bool skipped;
}

struct PsdDepthGridLayerMask {
    string layerPath;
    string layerName;
    size_t sampledVertices;
    size_t selectedVertices;
}

struct PsdDepthLayerPreview {
    string layerPath;
    string layerName;
    int left;
    int top;
    int width;
    int height;
    ubyte[] originalRgba;
    ubyte[] depthMaskRgba;
}

struct PsdDepthImportResult {
    PsdDepthLayerMapping[] mappings;
    PsdDepthGridResult[] grids;
    PsdDepthLayerPreview[] layerPreviews;
    size_t matchedLayers;
    size_t unmatchedLayers;
    size_t ambiguousLayers;
    size_t skippedGrids;
}

struct PsdDepthSampleResult {
    bool valid;
    float value;
}

private struct DepthLayerImage {
    string layerPath;
    string layerName;
    int left;
    int top;
    int width;
    int height;
    ubyte[] data;
    GridDeformer grid;
}

private struct Candidate {
    Node node;
    int priority;
}

private struct DepthSample {
    bool valid;
    float value;
}

private struct GridAccum {
    GridDeformer grid;
    float[] best;
    bool[] has;
    string[] winnerLayerPaths;
    PsdDepthGridLayerMask[] layerMasks;
}

PsdDepthConvolution ngPsdDepthConvolutionFromString(string value) {
    switch (value) {
        case "Nearest": return PsdDepthConvolution.Nearest;
        case "Box3x3": return PsdDepthConvolution.Box3x3;
        case "Box5x5": return PsdDepthConvolution.Box5x5;
        case "Gaussian3x3": return PsdDepthConvolution.Gaussian3x3;
        case "Gaussian5x5": return PsdDepthConvolution.Gaussian5x5;
        case "Median3x3": return PsdDepthConvolution.Median3x3;
        case "Frontmost3x3": return PsdDepthConvolution.Frontmost3x3;
        case "Backmost3x3": return PsdDepthConvolution.Backmost3x3;
        case "BoxCustom": return PsdDepthConvolution.BoxCustom;
        case "GaussianCustom": return PsdDepthConvolution.GaussianCustom;
        case "MedianCustom": return PsdDepthConvolution.MedianCustom;
        case "FrontmostCustom": return PsdDepthConvolution.FrontmostCustom;
        case "BackmostCustom": return PsdDepthConvolution.BackmostCustom;
        default: throw new Exception("Unknown PSD depth convolution: " ~ value);
    }
}

PsdDepthChannel ngPsdDepthChannelFromString(string value) {
    switch (value) {
        case "AverageRGB": return PsdDepthChannel.AverageRGB;
        case "R": return PsdDepthChannel.R;
        case "G": return PsdDepthChannel.G;
        case "B": return PsdDepthChannel.B;
        case "Luminance": return PsdDepthChannel.Luminance;
        default: throw new Exception("Unknown PSD depth channel: " ~ value);
    }
}

PsdDepthMissingPolicy ngPsdDepthMissingPolicyFromString(string value) {
    switch (value) {
        case "KeepExisting": return PsdDepthMissingPolicy.KeepExisting;
        case "SetZero": return PsdDepthMissingPolicy.SetZero;
        case "SetBack": return PsdDepthMissingPolicy.SetBack;
        case "SkipGrid": return PsdDepthMissingPolicy.SkipGrid;
        default: throw new Exception("Unknown PSD depth missing policy: " ~ value);
    }
}

string ngPsdDepthConvolutionName(PsdDepthConvolution value) {
    final switch (value) {
        case PsdDepthConvolution.Nearest: return "Nearest";
        case PsdDepthConvolution.Box3x3: return "Box3x3";
        case PsdDepthConvolution.Box5x5: return "Box5x5";
        case PsdDepthConvolution.Gaussian3x3: return "Gaussian3x3";
        case PsdDepthConvolution.Gaussian5x5: return "Gaussian5x5";
        case PsdDepthConvolution.Median3x3: return "Median3x3";
        case PsdDepthConvolution.Frontmost3x3: return "Frontmost3x3";
        case PsdDepthConvolution.Backmost3x3: return "Backmost3x3";
        case PsdDepthConvolution.BoxCustom: return "BoxCustom";
        case PsdDepthConvolution.GaussianCustom: return "GaussianCustom";
        case PsdDepthConvolution.MedianCustom: return "MedianCustom";
        case PsdDepthConvolution.FrontmostCustom: return "FrontmostCustom";
        case PsdDepthConvolution.BackmostCustom: return "BackmostCustom";
    }
}

string ngPsdDepthChannelName(PsdDepthChannel value) {
    final switch (value) {
        case PsdDepthChannel.AverageRGB: return "AverageRGB";
        case PsdDepthChannel.R: return "R";
        case PsdDepthChannel.G: return "G";
        case PsdDepthChannel.B: return "B";
        case PsdDepthChannel.Luminance: return "Luminance";
    }
}

string ngPsdDepthMissingPolicyName(PsdDepthMissingPolicy value) {
    final switch (value) {
        case PsdDepthMissingPolicy.KeepExisting: return "KeepExisting";
        case PsdDepthMissingPolicy.SetZero: return "SetZero";
        case PsdDepthMissingPolicy.SetBack: return "SetBack";
        case PsdDepthMissingPolicy.SkipGrid: return "SkipGrid";
    }
}

private float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

private float pixelDepth01(const(ubyte)[] data, size_t index, ref PsdDepthImportSettings settings) {
    auto r = cast(float)data[index + 0];
    auto g = cast(float)data[index + 1];
    auto b = cast(float)data[index + 2];
    float depth01 = 0.0f;
    final switch (settings.channel) {
        case PsdDepthChannel.AverageRGB:
            depth01 = (r + g + b) / (255.0f * 3.0f);
            break;
        case PsdDepthChannel.R:
            depth01 = r / 255.0f;
            break;
        case PsdDepthChannel.G:
            depth01 = g / 255.0f;
            break;
        case PsdDepthChannel.B:
            depth01 = b / 255.0f;
            break;
        case PsdDepthChannel.Luminance:
            depth01 = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
            break;
    }
    if (settings.invert) depth01 = 1.0f - depth01;
    return depth01;
}

private float pixelDepth(const(ubyte)[] data, size_t index, ref PsdDepthImportSettings settings) {
    return lerp(settings.backDepth, settings.frontDepth, pixelDepth01(data, index, settings)) * settings.depthScale;
}

private ubyte[] buildDepthMaskPreview(const(ubyte)[] rgba, int width, int height, ref PsdDepthImportSettings settings) {
    ubyte[] result;
    auto pixels = cast(size_t)max(0, width * height);
    result.length = pixels * 4;
    foreach (i; 0 .. pixels) {
        auto index = i * 4;
        auto alpha = index + 3 < rgba.length ? cast(float)rgba[index + 3] / 255.0f : 0.0f;
        if (alpha <= settings.alphaThreshold) {
            result[index + 0] = 0;
            result[index + 1] = 0;
            result[index + 2] = 0;
            result[index + 3] = 0;
            continue;
        }
        auto gray = cast(ubyte)round(pixelDepth01(rgba, index, settings) * 255.0f);
        result[index + 0] = gray;
        result[index + 1] = gray;
        result[index + 2] = gray;
        result[index + 3] = rgba[index + 3];
    }
    return result;
}

private DepthSample samplePixel(ref DepthLayerImage layer, int x, int y, ref PsdDepthImportSettings settings) {
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return DepthSample(false, 0);
    auto index = (cast(size_t)y * cast(size_t)layer.width + cast(size_t)x) * 4;
    if (index + 3 >= layer.data.length) return DepthSample(false, 0);
    auto alpha = cast(float)layer.data[index + 3] / 255.0f;
    if (alpha <= settings.alphaThreshold) return DepthSample(false, 0);
    return DepthSample(true, pixelDepth(layer.data, index, settings));
}

private int iabs(int value) {
    return value < 0 ? -value : value;
}

private float kernelWeight(ref PsdDepthImportSettings settings, int dx, int dy) {
    auto convolution = settings.convolution;
    final switch (convolution) {
        case PsdDepthConvolution.Box3x3:
        case PsdDepthConvolution.Box5x5:
        case PsdDepthConvolution.BoxCustom:
            return 1.0f;
        case PsdDepthConvolution.Gaussian3x3:
            return cast(float)((2 - iabs(dx)) * (2 - iabs(dy)));
        case PsdDepthConvolution.Gaussian5x5:
            int pascal(int d) {
                final switch (iabs(d)) {
                    case 0: return 6;
                    case 1: return 4;
                    case 2: return 1;
                }
                return 0;
            }
            return cast(float)(pascal(dx) * pascal(dy));
        case PsdDepthConvolution.GaussianCustom:
            auto radius = settings.customRadius > 0 ? settings.customRadius : 1;
            auto sigma = max(1.0f, cast(float)radius / 2.0f);
            auto distance2 = cast(float)(dx * dx + dy * dy);
            return cast(float)exp(-distance2 / (2.0f * sigma * sigma));
        case PsdDepthConvolution.Nearest:
        case PsdDepthConvolution.Median3x3:
        case PsdDepthConvolution.Frontmost3x3:
        case PsdDepthConvolution.Backmost3x3:
        case PsdDepthConvolution.MedianCustom:
        case PsdDepthConvolution.FrontmostCustom:
        case PsdDepthConvolution.BackmostCustom:
            return 1.0f;
    }
}

private DepthSample weightedAverage(ref DepthLayerImage layer, int cx, int cy, int radius, ref PsdDepthImportSettings settings) {
    float total = 0.0f;
    float weightTotal = 0.0f;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            auto sample = samplePixel(layer, cx + dx, cy + dy, settings);
            if (!sample.valid) continue;
            auto weight = kernelWeight(settings, dx, dy);
            total += sample.value * weight;
            weightTotal += weight;
        }
    }
    if (weightTotal <= 0) return DepthSample(false, 0);
    return DepthSample(true, total / weightTotal);
}

private DepthSample medianSample(ref DepthLayerImage layer, int cx, int cy, int radius, ref PsdDepthImportSettings settings) {
    float[] values;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            auto sample = samplePixel(layer, cx + dx, cy + dy, settings);
            if (sample.valid) values ~= sample.value;
        }
    }
    if (values.length == 0) return DepthSample(false, 0);
    values.sort();
    return DepthSample(true, values[values.length / 2]);
}

private DepthSample extremeSample(ref DepthLayerImage layer, int cx, int cy, int radius, bool frontmost, ref PsdDepthImportSettings settings) {
    bool hasValue;
    float best = 0.0f;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            auto sample = samplePixel(layer, cx + dx, cy + dy, settings);
            if (!sample.valid) continue;
            if (!hasValue || (frontmost ? sample.value > best : sample.value < best)) {
                hasValue = true;
                best = sample.value;
            }
        }
    }
    return DepthSample(hasValue, hasValue ? best : 0);
}

private DepthSample sampleLayer(ref DepthLayerImage layer, float x, float y, ref PsdDepthImportSettings settings) {
    auto cx = cast(int)round(x);
    auto cy = cast(int)round(y);

    final switch (settings.convolution) {
        case PsdDepthConvolution.Nearest:
            return samplePixel(layer, cx, cy, settings);
        case PsdDepthConvolution.Box3x3:
            return weightedAverage(layer, cx, cy, 1, settings);
        case PsdDepthConvolution.Box5x5:
            return weightedAverage(layer, cx, cy, 2, settings);
        case PsdDepthConvolution.Gaussian3x3:
            return weightedAverage(layer, cx, cy, 1, settings);
        case PsdDepthConvolution.Gaussian5x5:
            return weightedAverage(layer, cx, cy, 2, settings);
        case PsdDepthConvolution.Median3x3:
            return medianSample(layer, cx, cy, 1, settings);
        case PsdDepthConvolution.Frontmost3x3:
            return extremeSample(layer, cx, cy, 1, true, settings);
        case PsdDepthConvolution.Backmost3x3:
            return extremeSample(layer, cx, cy, 1, false, settings);
        case PsdDepthConvolution.BoxCustom:
            return weightedAverage(layer, cx, cy, max(1, settings.customRadius), settings);
        case PsdDepthConvolution.GaussianCustom:
            return weightedAverage(layer, cx, cy, max(1, settings.customRadius), settings);
        case PsdDepthConvolution.MedianCustom:
            return medianSample(layer, cx, cy, max(1, settings.customRadius), settings);
        case PsdDepthConvolution.FrontmostCustom:
            return extremeSample(layer, cx, cy, max(1, settings.customRadius), true, settings);
        case PsdDepthConvolution.BackmostCustom:
            return extremeSample(layer, cx, cy, max(1, settings.customRadius), false, settings);
    }
}

private GridDeformer containingGrid(Node node) {
    auto cursor = node;
    while (cursor !is null) {
        if (auto grid = cast(GridDeformer)cursor) return grid;
        cursor = cursor.parent;
    }
    return null;
}

GridDeformer ngPsdDepthContainingGrid(Node node) {
    return containingGrid(node);
}

private Candidate[] matchCandidates(Puppet puppet, string layerPath, string layerName, ref PsdDepthImportSettings settings) {
    Candidate[] candidates;

    auto parts = puppet.findNodesType!ExPart(puppet.root);
    foreach (part; parts) {
        auto path = part.layerPath.length ? part.layerPath : ("/" ~ part.name);
        if (path == layerPath) {
            candidates ~= Candidate(part, 0);
        } else if (baseName(path) == layerName) {
            candidates ~= Candidate(part, 1);
        } else if (part.name == layerName) {
            candidates ~= Candidate(part, 2);
        }
    }

    if (settings.matchDirectGridName) {
        auto grids = puppet.findNodesType!GridDeformer(puppet.root);
        foreach (grid; grids) {
            if (grid.name == layerName) {
                candidates ~= Candidate(grid, 3);
            }
        }
    }

    if (candidates.length == 0) return candidates;
    int bestPriority = candidates[0].priority;
    foreach (candidate; candidates) {
        if (candidate.priority < bestPriority) bestPriority = candidate.priority;
    }

    Candidate[] best;
    foreach (candidate; candidates) {
        if (candidate.priority == bestPriority) best ~= candidate;
    }
    return best;
}

private size_t findAccum(ref GridAccum[] accums, GridDeformer grid) {
    foreach (i, ref accum; accums) {
        if (accum.grid is grid) return i;
    }
    auto vertices = grid.vertices;
    GridAccum accum;
    accum.grid = grid;
    accum.best.length = vertices.length;
    accum.has.length = vertices.length;
    accum.winnerLayerPaths.length = vertices.length;
    accums ~= accum;
    return accums.length - 1;
}

private size_t findLayerMask(ref GridAccum accum, string layerPath, string layerName) {
    foreach (i, ref mask; accum.layerMasks) {
        if (mask.layerPath == layerPath) return i;
    }
    PsdDepthGridLayerMask mask;
    mask.layerPath = layerPath;
    mask.layerName = layerName;
    accum.layerMasks ~= mask;
    return accum.layerMasks.length - 1;
}

private GridDeformer findGridByUuid(Puppet puppet, string uuid) {
    auto grids = puppet.findNodesType!GridDeformer(puppet.root);
    foreach (grid; grids) {
        if (grid.uuid.to!string == uuid) return grid;
    }
    return null;
}

private vec2 gridVertexDocumentPosition(GridDeformer grid, vec2 vertex, int documentWidth, int documentHeight) {
    auto world = grid.transform.matrix * vec4(vertex, 0, 1);
    return vec2(
        world.x + cast(float)documentWidth / 2.0f,
        world.y + cast(float)documentHeight / 2.0f
    );
}

vec2 ngPsdDepthGridVertexDocumentPosition(GridDeformer grid, vec2 vertex, int documentWidth, int documentHeight) {
    return gridVertexDocumentPosition(grid, vertex, documentWidth, documentHeight);
}

PsdDepthSampleResult ngPsdDepthSamplePixels(
    const(ubyte)[] rgba,
    int width,
    int height,
    float x,
    float y,
    PsdDepthImportSettings settings
) {
    enforce(width >= 0 && height >= 0, "Image dimensions must be non-negative");
    enforce(rgba.length >= cast(size_t)max(0, width * height) * 4, "RGBA buffer is smaller than dimensions");
    DepthLayerImage layer;
    layer.width = width;
    layer.height = height;
    layer.data = rgba.dup;
    auto sample = sampleLayer(layer, x, y, settings);
    return PsdDepthSampleResult(sample.valid, sample.value);
}

PsdDepthSampleResult ngPsdDepthFrontmost(PsdDepthSampleResult[] samples) {
    PsdDepthSampleResult best;
    foreach (sample; samples) {
        if (!sample.valid) continue;
        if (!best.valid || sample.value > best.value) best = sample;
    }
    return best;
}

private void finalizeGridResult(ref PsdDepthGridResult result, ref GridAccum accum, ref PsdDepthImportSettings settings) {
    auto mapped = cast(DepthMappedNode)accum.grid;
    float[] existing;
    if (mapped !is null) existing = mapped.copyDepths();
    if (existing is null || existing.length != accum.best.length) {
        existing.length = accum.best.length;
        existing[] = 0.0f;
    }

    result.grid = accum.grid;
    result.depths.length = accum.best.length;
    result.layerMasks = accum.layerMasks.dup;
    foreach (winnerLayerPath; accum.winnerLayerPaths) {
        if (winnerLayerPath.length == 0) continue;
        foreach (ref mask; result.layerMasks) {
            if (mask.layerPath == winnerLayerPath) {
                mask.selectedVertices++;
                break;
            }
        }
    }
    bool hasMinMax;
    foreach (i; 0 .. accum.best.length) {
        if (accum.has[i]) {
            result.depths[i] = accum.best[i];
            result.sampledVertices++;
            if (!hasMinMax || result.depths[i] < result.minDepth) result.minDepth = result.depths[i];
            if (!hasMinMax || result.depths[i] > result.maxDepth) result.maxDepth = result.depths[i];
            hasMinMax = true;
        } else {
            result.missingVertices++;
            final switch (settings.missingPolicy) {
                case PsdDepthMissingPolicy.KeepExisting:
                    result.depths[i] = existing[i];
                    break;
                case PsdDepthMissingPolicy.SetZero:
                    result.depths[i] = 0.0f;
                    break;
                case PsdDepthMissingPolicy.SetBack:
                    result.depths[i] = settings.backDepth * settings.depthScale;
                    break;
                case PsdDepthMissingPolicy.SkipGrid:
                    result.skipped = true;
                    result.depths = existing;
                    break;
            }
        }
    }
    if (!hasMinMax) {
        result.minDepth = 0.0f;
        result.maxDepth = 0.0f;
    }
}

PsdDepthImportResult ngBuildPsdDepthsFromPSD(Puppet puppet, string path, PsdDepthImportSettings settings) {
    enforce(puppet !is null && puppet.root !is null, "No active puppet");
    enforce(path.length > 0, "Path not provided");
    enforce(settings.backDepth.isFinite && settings.frontDepth.isFinite, "Depth range must be finite");
    enforce(settings.depthScale.isFinite, "Depth scale must be finite");
    enforce(settings.depthScale >= 0.0f, "Depth scale must be non-negative");
    enforce(settings.alphaThreshold >= 0.0f && settings.alphaThreshold <= 1.0f, "Alpha threshold must be in [0, 1]");
    enforce(settings.customRadius >= 1 && settings.customRadius <= 64, "Custom radius must be in [1, 64]");

    File file = File(path);
    scope(exit) file.close();
    auto document = parseDocument(file);
    scope(exit) destroy(document);

    PsdDepthImportResult result;
    DepthLayerImage[] layers;

    import std.array : join;
    string[] layerPathSegments;
    string calcSegment;
    foreach_reverse (layer; document.layers) {
        if (layer.type != LayerType.Any) {
            if (layer.name != "</Layer set>" && layer.name != "</Layer group>") {
                layerPathSegments ~= layer.name;
            } else if (layerPathSegments.length > 0) {
                layerPathSegments.length--;
            }
            calcSegment = layerPathSegments.length > 0 ? "/" ~ layerPathSegments.join("/") : "";
            continue;
        }

        auto layerPath = "%s/%s".format(calcSegment, layer.name);
        PsdDepthLayerMapping mapping;
        mapping.layerPath = layerPath;
        mapping.layerName = layer.name;

        GridDeformer grid;
        if (auto ignored = layerPath in settings.ignoredLayerPaths) {
            if (*ignored) {
                mapping.ignored = true;
                mapping.manual = true;
                mapping.status = "Ignored";
                result.mappings ~= mapping;
                continue;
            }
        }

        if (auto overrideUuid = layerPath in settings.layerTargetGridUuidOverrides) {
            grid = findGridByUuid(puppet, *overrideUuid);
            mapping.manual = true;
            if (grid is null) {
                mapping.status = "UnmatchedManualGrid";
                result.unmatchedLayers++;
                result.mappings ~= mapping;
                continue;
            }
            mapping.matched = true;
            mapping.matchedNodeName = grid.name;
            mapping.matchedNodeUuid = grid.uuid;
            mapping.targetGridName = grid.name;
            mapping.targetGridUuid = grid.uuid;
            mapping.status = "Manual";
        } else {
            auto candidates = matchCandidates(puppet, layerPath, layer.name, settings);
            if (candidates.length == 0) {
                mapping.status = "Unmatched";
                result.unmatchedLayers++;
                result.mappings ~= mapping;
                continue;
            }

            auto candidate = candidates[0];
            grid = containingGrid(candidate.node);
            mapping.matchedNodeName = candidate.node.name;
            mapping.matchedNodeUuid = candidate.node.uuid;
            mapping.ambiguous = candidates.length > 1;
            if (mapping.ambiguous) {
                mapping.status = "Ambiguous";
                result.ambiguousLayers++;
            } else {
                mapping.status = "Matched";
            }

            if (grid is null) {
                mapping.status = mapping.ambiguous ? "AmbiguousWithoutGrid" : "UnmatchedWithoutGrid";
                result.unmatchedLayers++;
                result.mappings ~= mapping;
                continue;
            }

            mapping.matched = true;
            mapping.targetGridName = grid.name;
            mapping.targetGridUuid = grid.uuid;
        }

        result.matchedLayers++;
        result.mappings ~= mapping;

        layer.extractLayerImage();
        if (layer.data.length == 0) continue;
        PsdDepthLayerPreview layerPreview;
        layerPreview.layerPath = layerPath;
        layerPreview.layerName = layer.name;
        layerPreview.left = layer.left;
        layerPreview.top = layer.top;
        layerPreview.width = layer.width;
        layerPreview.height = layer.height;
        layerPreview.originalRgba = layer.data.dup;
        layerPreview.depthMaskRgba = buildDepthMaskPreview(layer.data, layer.width, layer.height, settings);
        result.layerPreviews ~= layerPreview;

        DepthLayerImage image;
        image.layerPath = layerPath;
        image.layerName = layer.name;
        image.left = layer.left;
        image.top = layer.top;
        image.width = layer.width;
        image.height = layer.height;
        image.data = layer.data.dup;
        image.grid = grid;
        layers ~= image;
        layer.data = null;
    }

    GridAccum[] accums;
    foreach (ref layer; layers) {
        auto accumIndex = findAccum(accums, layer.grid);
        auto layerMaskIndex = findLayerMask(accums[accumIndex], layer.layerPath, layer.layerName);
        auto vertices = layer.grid.vertices;
        foreach (i; 0 .. vertices.length) {
            auto documentPoint = gridVertexDocumentPosition(layer.grid, vertices[i], document.width, document.height);
            auto layerX = documentPoint.x - cast(float)layer.left;
            auto layerY = documentPoint.y - cast(float)layer.top;
            auto sample = sampleLayer(layer, layerX, layerY, settings);
            if (!sample.valid) continue;
            accums[accumIndex].layerMasks[layerMaskIndex].sampledVertices++;
            if (!accums[accumIndex].has[i] || sample.value > accums[accumIndex].best[i]) {
                accums[accumIndex].has[i] = true;
                accums[accumIndex].best[i] = sample.value;
                accums[accumIndex].winnerLayerPaths[i] = layer.layerPath;
            }
        }
    }

    foreach (ref accum; accums) {
        PsdDepthGridResult gridResult;
        finalizeGridResult(gridResult, accum, settings);
        if (gridResult.skipped) result.skippedGrids++;
        result.grids ~= gridResult;
    }

    return result;
}
