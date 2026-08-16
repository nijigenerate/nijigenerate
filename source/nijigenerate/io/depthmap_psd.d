module nijigenerate.io.depthmap_psd;

import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.ext.nodes.expart;
import nijigenerate.io.depthimage : DepthDrawPruneLayer, DepthDrawSplitLayer, ngDepthDrawAlphaMaskFromRgba,
    ngDepthDrawBuildLayerContourBandMask, ngDepthDrawBuildVisibleLayerMap, ngDepthDrawDecodeDepthPixelsFromRgba,
    ngDepthDrawDecodeGrayscaleDepthPixelsFromRgba,
    ngDepthDrawDetectAlphaDepthGaps, ngDepthDrawInpaintMaskedLayerDepth, ngDepthDrawMedianFillDepth,
    ngDepthDrawApplyPsdMaskToAlpha, ngDepthDrawPruneForeignDepthSeeds, ngDepthDrawSeedLayerDepthPixels,
    ngDepthImageCompositeAlpha,
    ngDepthImageCoverageAlphaAt, ngDepthImageCoverageAlphaAtUv, ngDepthImageCoverageReliableAt;
import nijigenerate.io.psdlayers : ngPsdLayerGroupStates;
import nijigenerate.io.depthsample : DepthSampleAggregate, DepthSampleChannel, DepthSampleConvolution, DepthSamplePoint,
    ngDepthSampleAcceptsAlpha, ngDepthSampleAlphaByte, ngDepthSampleConvolve, ngDepthSampleEffectiveAlpha,
    ngDepthSampleMissingPoint, ngDepthSampleOpacity01, ngDepthSamplePixelDepth, ngDepthSamplePixelDepth01,
    ngDepthSampleValueToDepth01;
import nijilive;
import nijilive.core.nodes.deformable : Deformable;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijilive.core.nodes.deformer.path : PathDeformer;
import psd;
import std.algorithm.comparison : max, min;
import std.algorithm.sorting : sort;
import std.array : array;
import std.conv : to;
import std.exception : enforce;
import std.math : abs, ceil, floor, isFinite, round;
import std.path : baseName, extension, stripExtension;
import std.stdio : File;
import std.string : format, toLower;

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

enum PsdDepthCompositionMode {
    Unknown,
    OneToOne,
    NToOne,
    NToN,
    UnsupportedOneToN,
}

enum PsdDepthCompositeSourceKind {
    Unknown,
    ActiveArtTargets,
    FlatImage,
    PsdLayers,
}

private bool psdLayerVisible(ref Layer layer) {
    return (layer.flags & LayerFlags.Visible) == 0;
}

private string uniquePsdLayerPath(string path, ref size_t[string] occurrences) {
    auto previous = path in occurrences;
    auto occurrence = previous is null ? 1 : *previous + 1;
    occurrences[path] = occurrence;
    return occurrence == 1 ? path : "%s [#%s]".format(path, occurrence);
}

struct PsdDepthImportSettings {
    bool invert = false;
    float backDepth = -1.0f;
    float frontDepth = 1.0f;
    float depthScale = 1.0f;
    float alphaThreshold = 0.01f;
    bool matchDirectGridName = true;
    size_t explicitColorLayerCount;
    int customRadius = 3;
    PsdDepthConvolution convolution = PsdDepthConvolution.Gaussian3x3;
    PsdDepthChannel channel = PsdDepthChannel.AverageRGB;
    PsdDepthMissingPolicy missingPolicy = PsdDepthMissingPolicy.KeepExisting;
    bool zeroDepthIsMissing;
    bool repairContourBand;
    bool smoothWavySurface;
    bool useGpuComposition;
    string colorSourcePath;
    string[string] layerTargetGridUuidOverrides;
    bool[string] ignoredLayerPaths;
    bool[string] disabledGridUuids;
    bool[string] disabledGridLayerKeys;
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
    Deformable grid;
    float[] depths;
    float[] baseDepths;
    string[] winnerLayerPaths;
    PsdDepthGridLayerMask[] layerMasks;
    size_t coverageSources;
    int documentWidth;
    int documentHeight;
    int previewLeft;
    int previewTop;
    int previewWidth;
    int previewHeight;
    ubyte[] rawCompositePreviewRgba;
    ubyte[] compositePreviewRgba;
    bool[] missingVertexMask;
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

struct PsdDepthLayerDepthStats {
    size_t maskedPixels;
    size_t zeroPixels;
    float minDepth01;
    float maxDepth01;
    float rangeDepth01;
    float adjacentDelta01;
    bool hasDepth;
}

struct PsdDepthCompositionDiagnostic {
    string type;
    string message;
    string layerPath;
    string layerName;
    size_t count;
}

struct PsdDepthCompositeSourceLayer {
    string id;
    string name;
    int left;
    int top;
    int width;
    int height;
    bool visible = true;
    bool hasPixels;
    float opacity = 1.0f;
    size_t maskedPixels;
    ulong targetGridUuid;
    string targetGridName;
    ubyte[] rgba;
    ubyte[] maskRgba;
}

struct PsdDepthCompositeSource {
    PsdDepthCompositeSourceKind kind;
    string kindName;
    string sourcePath;
    int width;
    int height;
    PsdDepthCompositeSourceLayer[] layers;
}

alias PsdDepthColorComposite = PsdDepthCompositeSource;
alias PsdDepthDepthComposite = PsdDepthCompositeSource;

struct PsdDepthComposedLayer {
    string id;
    string layerPath;
    string layerName;
    string colorLayerPath;
    string colorLayerName;
    string sourcePath;
    string depthLayerName;
    int left;
    int top;
    int width;
    int height;
    bool visible = true;
    bool enabled = true;
    bool depthEnabled = true;
    bool invert;
    float depthOffset = 0.0f;
    float depthScale = 1.0f;
    float backDepth = -1.0f;
    float frontDepth = 1.0f;
    float sourceDepthScale = 1.0f;
    float alphaThreshold = 0.01f;
    int customRadius = 3;
    PsdDepthConvolution convolution = PsdDepthConvolution.Gaussian3x3;
    bool outlierPruneEnabled;
    bool puppetFitEnabled = true;
    string puppetBindingOverride;
    ulong targetGridUuid;
    string targetGridName;
    PsdDepthLayerDepthStats depthStats;
    ubyte[] colorRgba;
    ubyte[] depthRgba;
    ubyte[] maskRgba;
    ubyte[] coverageMaskRgba;
}

struct PsdDepthComposedSource {
    PsdDepthCompositionMode mode;
    string modeName;
    int width;
    int height;
    float globalDepthScale = 1.0f;
    float globalDepthCentroid = 0.0f;
    PsdDepthColorComposite colorSource;
    PsdDepthDepthComposite depthSource;
    PsdDepthComposedLayer[] layers;
    PsdDepthCompositionDiagnostic[] diagnostics;
}

struct PsdDepthImportResult {
    PsdDepthCompositionMode compositionMode;
    string compositionModeName;
    int compositionWidth;
    int compositionHeight;
    float globalDepthScale = 1.0f;
    float globalDepthCentroid = 0.0f;
    PsdDepthMissingPolicy missingPolicy = PsdDepthMissingPolicy.KeepExisting;
    float missingBackDepth = -1.0f;
    size_t colorLayerCount;
    size_t sourceDepthLayerCount;
    size_t composedLayerCount;
    PsdDepthCompositeSource colorSource;
    PsdDepthCompositeSource depthSource;
    PsdDepthCompositionDiagnostic[] compositionDiagnostics;
    PsdDepthComposedLayer[] composedLayers;
    PsdDepthLayerMapping[] mappings;
    PsdDepthGridResult[] grids;
    size_t matchedLayers;
    size_t unmatchedLayers;
    size_t ambiguousLayers;
    size_t skippedGrids;
    bool gpuCompositionRequested;
    bool smoothWavySurface;
}

private PsdDepthCompositeSourceLayer cloneCompositeSourceLayer(PsdDepthCompositeSourceLayer layer) {
    layer.rgba = layer.rgba.dup;
    layer.maskRgba = layer.maskRgba.dup;
    return layer;
}

private PsdDepthCompositeSource cloneCompositeSource(PsdDepthCompositeSource source) {
    auto layers = source.layers;
    source.layers.length = layers.length;
    foreach (i, layer; layers) source.layers[i] = cloneCompositeSourceLayer(layer);
    return source;
}

private PsdDepthComposedLayer cloneComposedLayer(PsdDepthComposedLayer layer) {
    layer.colorRgba = layer.colorRgba.dup;
    layer.depthRgba = layer.depthRgba.dup;
    layer.maskRgba = layer.maskRgba.dup;
    layer.coverageMaskRgba = layer.coverageMaskRgba.dup;
    return layer;
}

struct PsdDepthSampleResult {
    bool valid;
    float value;
}

private struct CoverageSource {
    int width;
    int height;
    int channels;
    float opacity = 1.0f;
    ubyte[] data;
    mat4 worldToLocal;
    bool hasDocumentRect;
    int documentLeft;
    int documentTop;
    int documentWidth;
    int documentHeight;
    mat4 localToDocumentWorld;
    Vec2Array vertices;
    Vec2Array uvs;
    ushort[] indices;
    vec2 origin;
}

private struct DepthLayerImage {
    string layerPath;
    string layerName;
    int left;
    int top;
    int width;
    int height;
    int documentWidth;
    int documentHeight;
    float opacity = 1.0f;
    ubyte[] data;
    float[] coverageGateCache;
    float[] coverageAlphaCache;
    int coverageWidth;
    int coverageHeight;
    int coverageChannels;
    float coverageOpacity = 1.0f;
    ubyte[] coverageData;
    bool coverageUsesMesh;
    mat4 coverageWorldToLocal;
    CoverageSource[] coverageSources;
    Deformable grid;
}

private struct Candidate {
    Node node;
    int priority;
    float overlapScore;
}

private struct DepthSample {
    bool valid;
    float value;
    float weight;
}

private struct ColorLayerPreviewRgba {
    ubyte[] rgba;
    int width;
    int height;
}

private struct GridAccum {
    Deformable grid;
    float[] best;
    float[] baseBest;
    bool[] has;
    string[] winnerLayerPaths;
    PsdDepthGridLayerMask[] layerMasks;
}

private struct PngComposedBinding {
    DepthLayerImage image;
    Deformable target;
    Node matchedTarget;
    string status;
    bool manual;
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

string ngPsdDepthCompositionModeName(PsdDepthCompositionMode value) {
    final switch (value) {
        case PsdDepthCompositionMode.Unknown: return "Unknown";
        case PsdDepthCompositionMode.OneToOne: return "1:1";
        case PsdDepthCompositionMode.NToOne: return "N:1";
        case PsdDepthCompositionMode.NToN: return "N:N";
        case PsdDepthCompositionMode.UnsupportedOneToN: return "1:N";
    }
}

string ngPsdDepthCompositeSourceKindName(PsdDepthCompositeSourceKind value) {
    final switch (value) {
        case PsdDepthCompositeSourceKind.Unknown: return "Unknown";
        case PsdDepthCompositeSourceKind.ActiveArtTargets: return "ActiveArtTargets";
        case PsdDepthCompositeSourceKind.FlatImage: return "FlatImage";
        case PsdDepthCompositeSourceKind.PsdLayers: return "PsdLayers";
    }
}

PsdDepthCompositionMode ngPsdDepthCompositionModeForCounts(size_t colorLayerCount, size_t depthLayerCount) {
    if (colorLayerCount == 1 && depthLayerCount > 1) return PsdDepthCompositionMode.UnsupportedOneToN;
    if (colorLayerCount == 1 && depthLayerCount == 1) return PsdDepthCompositionMode.OneToOne;
    if (colorLayerCount > 1 && depthLayerCount == 1) return PsdDepthCompositionMode.NToOne;
    if (colorLayerCount > 1 && depthLayerCount > 1) return PsdDepthCompositionMode.NToN;
    return PsdDepthCompositionMode.Unknown;
}

private void setCompositionMode(ref PsdDepthImportResult result, PsdDepthCompositionMode mode) {
    result.compositionMode = mode;
    result.compositionModeName = ngPsdDepthCompositionModeName(mode);
}

string ngPsdDepthNormalizeLayerName(string name) {
    auto normalized = name.baseName.stripExtension.toLower;
    if (normalized.length && normalized[0] == '/') normalized = normalized[1 .. $];
    char[] compact;
    bool previousSeparator;
    foreach (ch; normalized) {
        auto separator = ch == ' ' || ch == '_' || ch == '-';
        if (separator) {
            if (!previousSeparator && compact.length > 0) compact ~= '-';
            previousSeparator = true;
            continue;
        }
        compact ~= ch;
        previousSeparator = false;
    }
    if (compact.length && compact[$ - 1] == '-') compact.length = compact.length - 1;
    return compact.idup;
}

string ngPsdDepthLayerIdentityKey(string name, int left, int top, int width, int height) {
    return "%s:%s:%s:%s:%s".format(ngPsdDepthNormalizeLayerName(name), left, top, width, height);
}

float ngPsdDepthRectOverlapScore(
    float leftA,
    float topA,
    float widthA,
    float heightA,
    float leftB,
    float topB,
    float widthB,
    float heightB
) {
    if (widthA <= 0 || heightA <= 0 || widthB <= 0 || heightB <= 0) return 0.0f;
    auto rightA = leftA + widthA;
    auto bottomA = topA + heightA;
    auto rightB = leftB + widthB;
    auto bottomB = topB + heightB;
    auto overlapWidth = min(rightA, rightB) - max(leftA, leftB);
    auto overlapHeight = min(bottomA, bottomB) - max(topA, topB);
    if (overlapWidth <= 0 || overlapHeight <= 0) return 0.0f;
    auto overlapArea = overlapWidth * overlapHeight;
    auto unionArea = widthA * heightA + widthB * heightB - overlapArea;
    return unionArea > 0 ? overlapArea / unionArea : 0.0f;
}

private bool sameComposedLayerIdentity(PsdDepthComposedLayer layer, PsdDepthComposedLayer previousLayer) {
    if (layer.layerPath.length == 0 || layer.layerPath != previousLayer.layerPath) return false;
    if (layer.targetGridUuid != 0 || previousLayer.targetGridUuid != 0) {
        return layer.targetGridUuid != 0 && layer.targetGridUuid == previousLayer.targetGridUuid;
    }
    return true;
}

private PsdDepthComposedLayer* findPreviousComposedLayerState(
    PsdDepthComposedLayer[] previousLayers,
    PsdDepthComposedLayer layer
) {
    foreach (ref previousLayer; previousLayers) {
        if (sameComposedLayerIdentity(layer, previousLayer)) return &previousLayer;
    }
    return null;
}

private void copyComposedLayerState(ref PsdDepthComposedLayer layer, PsdDepthComposedLayer previousLayer) {
    layer.visible = previousLayer.visible;
    if (layer.targetGridUuid != 0 && layer.targetGridUuid == previousLayer.targetGridUuid) {
        layer.enabled = previousLayer.enabled;
    }
    layer.depthEnabled = previousLayer.depthEnabled;
    layer.invert = previousLayer.invert;
    layer.depthOffset = previousLayer.depthOffset;
    layer.depthScale = previousLayer.depthScale;
    layer.outlierPruneEnabled = previousLayer.outlierPruneEnabled;
    layer.puppetFitEnabled = previousLayer.puppetFitEnabled;
}

void ngPsdDepthApplyPreviousComposedLayerState(
    ref PsdDepthImportResult result,
    ref PsdDepthImportResult previous
) {
    ngPsdDepthApplyPreviousComposedSourceState(result, ngPsdDepthComposedSourceFromImportResult(previous));
}

void ngPsdDepthApplyPreviousComposedSourceState(
    ref PsdDepthImportResult result,
    PsdDepthComposedSource previous
) {
    foreach (ref layer; result.composedLayers) {
        auto previousLayer = findPreviousComposedLayerState(previous.layers, layer);
        if (previousLayer !is null) copyComposedLayerState(layer, *previousLayer);
    }
    ngPsdDepthRefreshDerivedState(result);
}

private void addCompositionDiagnostic(
    ref PsdDepthImportResult result,
    string type,
    string message,
    string layerPath = null,
    string layerName = null,
    size_t count = 0
) {
    PsdDepthCompositionDiagnostic diagnostic;
    diagnostic.type = type;
    diagnostic.message = message;
    diagnostic.layerPath = layerPath;
    diagnostic.layerName = layerName;
    diagnostic.count = count;
    result.compositionDiagnostics ~= diagnostic;
}

private void validateCompositionDimensions(ref PsdDepthImportResult result, string context) {
    enforce(result.compositionWidth > 0 && result.compositionHeight > 0,
        "Composition dimensions must be positive for " ~ context);
    enforce(result.colorSource.width == result.compositionWidth &&
        result.colorSource.height == result.compositionHeight &&
        result.depthSource.width == result.compositionWidth &&
        result.depthSource.height == result.compositionHeight,
        "Color and depth source dimensions must match for " ~ context);
    ngPsdDepthRefreshDerivedState(result);
}

PsdDepthComposedSource ngPsdDepthComposedSourceFromImportResult(ref PsdDepthImportResult result) {
    PsdDepthComposedSource source;
    source.mode = result.compositionMode;
    source.modeName = result.compositionModeName;
    source.width = result.compositionWidth;
    source.height = result.compositionHeight;
    source.globalDepthScale = result.globalDepthScale;
    source.globalDepthCentroid = result.globalDepthCentroid;
    source.colorSource = cloneCompositeSource(result.colorSource);
    source.depthSource = cloneCompositeSource(result.depthSource);
    source.layers.length = result.composedLayers.length;
    foreach (i, layer; result.composedLayers) source.layers[i] = cloneComposedLayer(layer);
    source.diagnostics = result.compositionDiagnostics.dup;
    return source;
}

private float computeGlobalDepthCentroid(PsdDepthComposedLayer[] layers) {
    double sum = 0.0;
    size_t count;
    foreach (layer; layers) {
        if (!layer.visible || !layer.depthEnabled) continue;
        if (layer.depthRgba.length < 4) continue;
        auto pixelCount = layer.depthRgba.length / 4;
        foreach (i; 0 .. pixelCount) {
            auto index = i * 4;
            if (layer.maskRgba.length >= index + 4 && layer.maskRgba[index + 3] == 0) continue;
            if (layer.depthRgba[index + 3] == 0) continue;
            auto depth = layer.depthRgba[index];
            if (depth == 0) continue;
            auto depthScale = layer.depthScale.isFinite ? layer.depthScale : 1.0f;
            auto depthOffset = layer.depthOffset.isFinite ? layer.depthOffset : 0.0f;
            auto scaledDepth = cast(int)round(cast(float)depth * depthScale + depthOffset);
            sum += min(255, max(1, scaledDepth));
            count++;
        }
    }
    return count > 0 ? cast(float)(sum / cast(double)count) : 0.0f;
}

bool ngPsdDepthMappingAccountsForColorLayer(PsdDepthLayerMapping mapping) {
    return mapping.matched || mapping.ignored;
}

bool ngPsdDepthComposedLayerSurfaceCoversDocumentPixel(
    ref PsdDepthComposedLayer layer,
    int documentX,
    int documentY,
    out size_t rgbaIndex
) {
    rgbaIndex = 0;
    auto x = documentX - layer.left;
    auto y = documentY - layer.top;
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return false;
    rgbaIndex = (cast(size_t)y * cast(size_t)layer.width + cast(size_t)x) * 4;
    auto coverage = layer.coverageMaskRgba.length >= rgbaIndex + 4 ?
        layer.coverageMaskRgba : layer.maskRgba;
    if (coverage.length >= rgbaIndex + 4 && coverage[rgbaIndex + 3] == 0) return false;
    if (layer.colorRgba.length >= rgbaIndex + 4 && layer.colorRgba[rgbaIndex + 3] < 3) return false;
    return true;
}

bool ngPsdDepthResolvedPixelAt(
    ref PsdDepthImportResult imported,
    size_t layerIndex,
    int documentX,
    int documentY,
    out ptrdiff_t sourceLayerIndex,
    out ubyte[4] pixel
) {
    sourceLayerIndex = -1;
    pixel[] = 0;
    if (layerIndex >= imported.composedLayers.length) return false;
    auto layer = &imported.composedLayers[layerIndex];
    if (!layer.enabled || !layer.visible) return false;
    if (layer.depthEnabled) {
        size_t rgbaIndex;
        if (!ngPsdDepthComposedLayerSurfaceCoversDocumentPixel(*layer, documentX, documentY, rgbaIndex) ||
            rgbaIndex + 3 >= layer.depthRgba.length || layer.depthRgba[rgbaIndex + 3] == 0) return false;
        sourceLayerIndex = cast(ptrdiff_t)layerIndex;
        pixel[] = layer.depthRgba[rgbaIndex .. rgbaIndex + 4];
        return true;
    }

    // Layers are stored back-to-front. A disabled layer follows the nearest
    // enabled depth below it at the same document coordinate. Skipping another
    // disabled layer implements the recursive attachment without another path.
    foreach_reverse (candidateIndex; 0 .. layerIndex) {
        auto candidate = &imported.composedLayers[candidateIndex];
        if (!candidate.enabled || !candidate.visible || !candidate.depthEnabled) continue;
        size_t rgbaIndex;
        if (!ngPsdDepthComposedLayerSurfaceCoversDocumentPixel(*candidate, documentX, documentY, rgbaIndex)) continue;
        sourceLayerIndex = cast(ptrdiff_t)candidateIndex;
        if (rgbaIndex + 3 < candidate.depthRgba.length && candidate.depthRgba[rgbaIndex + 3] != 0) {
            pixel[] = candidate.depthRgba[rgbaIndex .. rgbaIndex + 4];
            return true;
        }
        sourceLayerIndex = -1;
    }
    return false;
}

ptrdiff_t ngPsdDepthResolvedLayerIndexAt(
    ref PsdDepthImportResult imported,
    size_t layerIndex,
    int documentX,
    int documentY,
    out size_t rgbaIndex
) {
    rgbaIndex = 0;
    ptrdiff_t sourceLayerIndex;
    ubyte[4] pixel;
    return ngPsdDepthResolvedPixelAt(
        imported, layerIndex, documentX, documentY, sourceLayerIndex, pixel)
        ? sourceLayerIndex : -1;
}

void ngPsdDepthRefreshDerivedState(ref PsdDepthImportResult result) {
    result.globalDepthCentroid = computeGlobalDepthCentroid(result.composedLayers);
}

private void applySingleLayerReplacementFallback(ref PsdDepthImportResult result, ref PsdDepthImportResult previous) {
    if (result.composedLayers.length != 1 || previous.composedLayers.length != 1) return;
    copyComposedLayerState(result.composedLayers[0], previous.composedLayers[0]);
    if (result.composedLayers[0].targetGridUuid == 0 && previous.composedLayers[0].targetGridUuid != 0) {
        result.composedLayers[0].targetGridUuid = previous.composedLayers[0].targetGridUuid;
        result.composedLayers[0].targetGridName = previous.composedLayers[0].targetGridName;
        result.composedLayers[0].puppetBindingOverride = previous.composedLayers[0].puppetBindingOverride;
    }
    ngPsdDepthRefreshDerivedState(result);
}

private PsdDepthCompositeSourceLayer makeCompositeSourceLayer(
    string id,
    string name,
    int left,
    int top,
    int width,
    int height,
    bool visible,
    bool hasPixels,
    size_t maskedPixels = 0,
    Deformable target = null
) {
    PsdDepthCompositeSourceLayer layer;
    layer.id = id;
    layer.name = name;
    layer.left = left;
    layer.top = top;
    layer.width = width;
    layer.height = height;
    layer.visible = visible;
    layer.hasPixels = hasPixels;
    layer.maskedPixels = maskedPixels;
    if (target !is null) {
        layer.targetGridUuid = target.uuid;
        layer.targetGridName = target.name;
    }
    return layer;
}

private void setCompositeSourceLayerPixels(
    ref PsdDepthCompositeSourceLayer layer,
    ubyte[] rgba,
    ubyte[] maskRgba = null
) {
    layer.rgba = rgba.dup;
    layer.maskRgba = maskRgba.length ? maskRgba.dup : rgba.dup;
    layer.hasPixels = layer.rgba.length > 0;
}

private size_t countAcceptedAlphaPixels(const(ubyte)[] rgba, float opacity) {
    size_t count;
    if (rgba.length < 4) return 0;
    foreach (i; 0 .. rgba.length / 4) {
        auto index = i * 4;
        if (effectiveAlpha(rgba, index, opacity) > 0.0f) count++;
    }
    return count;
}

private ubyte[] cropRgbaRect(const(ubyte)[] rgba, int sourceWidth, int sourceHeight, int left, int top, int width, int height) {
    ubyte[] result;
    if (width <= 0 || height <= 0) return result;
    result.length = cast(size_t)width * cast(size_t)height * 4;
    foreach (y; 0 .. height) {
        auto sourceY = top + y;
        foreach (x; 0 .. width) {
            auto sourceX = left + x;
            auto outIndex = (cast(size_t)y * cast(size_t)width + cast(size_t)x) * 4;
            if (sourceX < 0 || sourceY < 0 || sourceX >= sourceWidth || sourceY >= sourceHeight) {
                result[outIndex + 3] = 0;
                continue;
            }
            auto sourceIndex = (cast(size_t)sourceY * cast(size_t)sourceWidth + cast(size_t)sourceX) * 4;
            if (sourceIndex + 3 >= rgba.length) {
                result[outIndex + 3] = 0;
                continue;
            }
            result[outIndex .. outIndex + 4] = rgba[sourceIndex .. sourceIndex + 4];
        }
    }
    return result;
}

private ubyte[] normalizeFlatDepthRgba(const(ubyte)[] rgba, int width, int height, int channels = 4) {
    auto expectedLength = cast(size_t)max(0, width * height) * 4;
    ubyte[] result;
    result.length = expectedLength;
    auto sourceChannels = max(1, channels);
    auto pixelCount = cast(size_t)max(0, width * height);
    auto sourceLength = min(rgba.length, pixelCount * cast(size_t)sourceChannels);
    foreach (i; 0 .. pixelCount) {
        auto sourceIndex = i * cast(size_t)sourceChannels;
        auto outIndex = i * 4;
        if (sourceIndex >= sourceLength) {
            result[outIndex + 3] = 0;
            continue;
        }
        switch (sourceChannels) {
            case 1:
                result[outIndex + 0] = rgba[sourceIndex];
                result[outIndex + 1] = rgba[sourceIndex];
                result[outIndex + 2] = rgba[sourceIndex];
                result[outIndex + 3] = 255;
                break;
            case 2:
                result[outIndex + 0] = rgba[sourceIndex];
                result[outIndex + 1] = rgba[sourceIndex];
                result[outIndex + 2] = rgba[sourceIndex];
                result[outIndex + 3] = sourceIndex + 1 < sourceLength ? rgba[sourceIndex + 1] : 255;
                break;
            case 3:
                result[outIndex + 0] = rgba[sourceIndex + 0];
                result[outIndex + 1] = sourceIndex + 1 < sourceLength ? rgba[sourceIndex + 1] : rgba[sourceIndex + 0];
                result[outIndex + 2] = sourceIndex + 2 < sourceLength ? rgba[sourceIndex + 2] : rgba[sourceIndex + 0];
                result[outIndex + 3] = 255;
                break;
            default:
                result[outIndex + 0] = rgba[sourceIndex + 0];
                result[outIndex + 1] = sourceIndex + 1 < sourceLength ? rgba[sourceIndex + 1] : rgba[sourceIndex + 0];
                result[outIndex + 2] = sourceIndex + 2 < sourceLength ? rgba[sourceIndex + 2] : rgba[sourceIndex + 0];
                result[outIndex + 3] = sourceIndex + 3 < sourceLength ? rgba[sourceIndex + 3] : 255;
                break;
        }
    }
    return result;
}

private ubyte[] flipRgbaY(const(ubyte)[] rgba, int width, int height) {
    ubyte[] result;
    auto expectedLength = cast(size_t)max(0, width * height) * 4;
    result.length = expectedLength;
    if (rgba.length < expectedLength || width <= 0 || height <= 0) return result;
    auto rowBytes = cast(size_t)width * 4;
    foreach (y; 0 .. height) {
        auto src = cast(size_t)y * rowBytes;
        auto dst = cast(size_t)(height - 1 - y) * rowBytes;
        result[dst .. dst + rowBytes] = rgba[src .. src + rowBytes];
    }
    return result;
}

private void loadPsdCompositeSourceLayers(
    string sourcePath,
    ref PsdDepthCompositeSource source,
    ref size_t layerCount
) {
    File file = File(sourcePath);
    scope(exit) file.close();
    auto document = parseDocument(file);
    scope(exit) destroy(document);
    source.kind = PsdDepthCompositeSourceKind.PsdLayers;
    source.kindName = ngPsdDepthCompositeSourceKindName(source.kind);
    source.sourcePath = sourcePath;
    source.width = document.width;
    source.height = document.height;

    auto groupStates = ngPsdLayerGroupStates(document.layers);
    size_t[string] layerPathOccurrences;
    PsdClippingBaseState[string] clippingBaseByGroup;
    foreach_reverse (i, layer; document.layers) {
        if (layer.type != LayerType.Any) continue;
        auto groupState = groupStates[i];

        auto layerPath = uniquePsdLayerPath("%s/%s".format(groupState.path, layer.name), layerPathOccurrences);
        layer.extractLayerImage();
        if (layer.data.length == 0) continue;
        auto visible = groupState.visible && psdLayerVisible(layer);
        auto opacity = groupState.opacity * layerOpacity01(layer.opacity);
        DepthLayerImage image;
        image.left = layer.left;
        image.top = layer.top;
        image.width = cast(int)layer.width;
        image.height = cast(int)layer.height;
        image.data = layer.data.dup;
        if (layer.clipping) {
            clippingBaseByGroup[groupState.path] = psdClippingBaseState(image, visible, opacity);
        } else if (auto clippingBase = groupState.path in clippingBaseByGroup) {
            visible = visible && clippingBase.visible;
            auto baseLocalOpacity = groupState.opacity > 0.0f
                ? clippingBase.opacity / groupState.opacity
                : 0.0f;
            opacity *= max(0.0f, min(1.0f, baseLocalOpacity));
            applyPsdClippingBaseAlpha(image, *clippingBase);
        }
        auto sourceLayer = makeCompositeSourceLayer(
            layerPath,
            layer.name,
            layer.left,
            layer.top,
            layer.width,
            layer.height,
            visible,
            true,
            countAcceptedAlphaPixels(image.data, opacity)
        );
        sourceLayer.opacity = opacity;
        setCompositeSourceLayerPixels(sourceLayer, image.data);
        source.layers ~= sourceLayer;
        layerCount++;
        layer.data = null;
    }
}

private float compositeSourceLayerMatchScore(PsdDepthCompositeSourceLayer colorLayer, PsdDepthCompositeSourceLayer depthLayer) {
    if (colorLayer.id.length && colorLayer.id == depthLayer.id) return 4.0f;
    if (colorLayer.name.length && depthLayer.name.length &&
        ngPsdDepthLayerIdentityKey(colorLayer.name, colorLayer.left, colorLayer.top, colorLayer.width, colorLayer.height) ==
        ngPsdDepthLayerIdentityKey(depthLayer.name, depthLayer.left, depthLayer.top, depthLayer.width, depthLayer.height)) {
        return 3.0f;
    }
    if (colorLayer.name.length && depthLayer.name.length &&
        ngPsdDepthNormalizeLayerName(colorLayer.name) == ngPsdDepthNormalizeLayerName(depthLayer.name)) {
        return 2.0f;
    }
    auto overlap = ngPsdDepthRectOverlapScore(
        cast(float)colorLayer.left,
        cast(float)colorLayer.top,
        cast(float)colorLayer.width,
        cast(float)colorLayer.height,
        cast(float)depthLayer.left,
        cast(float)depthLayer.top,
        cast(float)depthLayer.width,
        cast(float)depthLayer.height
    );
    return overlap > 0.0f ? overlap : 0.0f;
}

private ptrdiff_t bestMatchingDepthSourceLayer(PsdDepthCompositeSourceLayer colorLayer, PsdDepthCompositeSourceLayer[] depthLayers) {
    ptrdiff_t bestIndex = -1;
    float bestScore = 0.0f;
    foreach (i, depthLayer; depthLayers) {
        auto score = compositeSourceLayerMatchScore(colorLayer, depthLayer);
        if (score > bestScore) {
            bestScore = score;
            bestIndex = cast(ptrdiff_t)i;
        }
    }
    return bestIndex;
}

private void addComposedLayer(
    ref PsdDepthImportResult result,
    string sourcePath,
    ref DepthLayerImage image,
    ref PsdDepthImportSettings settings,
    bool visible,
    bool enabled,
    Deformable target = null
) {
    PsdDepthComposedLayer layer;
    layer.id = image.layerPath;
    layer.layerPath = image.layerPath;
    layer.layerName = image.layerName;
    layer.colorLayerPath = image.layerPath;
    layer.colorLayerName = image.layerName;
    layer.sourcePath = sourcePath;
    layer.depthLayerName = image.layerName;
    layer.left = image.left;
    layer.top = image.top;
    layer.width = image.width;
    layer.height = image.height;
    layer.visible = visible;
    layer.enabled = enabled;
    layer.invert = false;
    layer.depthOffset = 0.0f;
    layer.depthScale = 1.0f;
    layer.backDepth = settings.backDepth;
    layer.frontDepth = settings.frontDepth;
    layer.sourceDepthScale = settings.depthScale;
    layer.alphaThreshold = settings.alphaThreshold;
    layer.customRadius = settings.customRadius;
    layer.convolution = settings.convolution;
    layer.depthStats = computeLayerDepthStats(image, settings);
    if (target !is null) {
        layer.targetGridUuid = target.uuid;
        layer.targetGridName = target.name;
        layer.puppetBindingOverride = target.uuid.to!string;
    }
    auto surfacePreview = buildColorLayerPreviewRgba(image);
    layer.colorRgba = surfacePreview.rgba;
    layer.depthRgba = buildDepthMaskPreviewFromSurface(image, settings, surfacePreview.rgba);
    layer.maskRgba.length = surfacePreview.rgba.length;
    foreach (i; 0 .. surfacePreview.rgba.length / 4) {
        auto index = i * 4;
        auto alpha = surfacePreview.rgba[index + 3];
        if (alpha == 0) continue;
        layer.maskRgba[index + 0] = 255;
        layer.maskRgba[index + 1] = 255;
        layer.maskRgba[index + 2] = 255;
        layer.maskRgba[index + 3] = alpha;
    }
    layer.coverageMaskRgba = layer.maskRgba.dup;
    result.composedLayers ~= layer;
    result.composedLayerCount = result.composedLayers.length;
}

private void addMissingComposedLayer(
    ref PsdDepthImportResult result,
    string layerPath,
    string layerName,
    int left,
    int top,
    int width,
    int height,
    Deformable target
) {
    if (target is null) return;
    PsdDepthComposedLayer layer;
    layer.id = layerPath;
    layer.layerPath = layer.id;
    layer.layerName = layerName;
    layer.colorLayerPath = layerPath;
    layer.colorLayerName = layerName;
    layer.left = left;
    layer.top = top;
    layer.width = width;
    layer.height = height;
    layer.targetGridUuid = target.uuid;
    layer.targetGridName = target.name;
    layer.puppetBindingOverride = target.uuid.to!string;
    layer.puppetFitEnabled = true;
    layer.enabled = false;
    result.composedLayers ~= layer;
    result.composedLayerCount = result.composedLayers.length;
}

private void addMissingComposedLayer(ref PsdDepthImportResult result, Deformable target) {
    if (target is null) return;
    addMissingComposedLayer(result, "/" ~ target.name, target.name, 0, 0, 0, 0, target);
}

private DepthSampleChannel sampleChannel(PsdDepthChannel channel) {
    final switch (channel) {
        case PsdDepthChannel.AverageRGB:
            return DepthSampleChannel.AverageRGB;
        case PsdDepthChannel.R:
            return DepthSampleChannel.R;
        case PsdDepthChannel.G:
            return DepthSampleChannel.G;
        case PsdDepthChannel.B:
            return DepthSampleChannel.B;
        case PsdDepthChannel.Luminance:
            return DepthSampleChannel.Luminance;
    }
}

private DepthSampleConvolution sampleConvolution(PsdDepthConvolution convolution) {
    final switch (convolution) {
        case PsdDepthConvolution.Nearest:
            return DepthSampleConvolution.Nearest;
        case PsdDepthConvolution.Box3x3:
            return DepthSampleConvolution.Box3x3;
        case PsdDepthConvolution.Box5x5:
            return DepthSampleConvolution.Box5x5;
        case PsdDepthConvolution.Gaussian3x3:
            return DepthSampleConvolution.Gaussian3x3;
        case PsdDepthConvolution.Gaussian5x5:
            return DepthSampleConvolution.Gaussian5x5;
        case PsdDepthConvolution.Median3x3:
            return DepthSampleConvolution.Median3x3;
        case PsdDepthConvolution.Frontmost3x3:
            return DepthSampleConvolution.Frontmost3x3;
        case PsdDepthConvolution.Backmost3x3:
            return DepthSampleConvolution.Backmost3x3;
        case PsdDepthConvolution.BoxCustom:
            return DepthSampleConvolution.BoxCustom;
        case PsdDepthConvolution.GaussianCustom:
            return DepthSampleConvolution.GaussianCustom;
        case PsdDepthConvolution.MedianCustom:
            return DepthSampleConvolution.MedianCustom;
        case PsdDepthConvolution.FrontmostCustom:
            return DepthSampleConvolution.FrontmostCustom;
        case PsdDepthConvolution.BackmostCustom:
            return DepthSampleConvolution.BackmostCustom;
    }
}

private float pixelDepth01(const(ubyte)[] data, size_t index, ref PsdDepthImportSettings settings) {
    if (index + 2 >= data.length) return 0.0f;
    return ngDepthSamplePixelDepth01(data, index, sampleChannel(settings.channel), settings.invert);
}

private float pixelDepth(const(ubyte)[] data, size_t index, ref PsdDepthImportSettings settings) {
    if (index + 2 >= data.length) return settings.backDepth;
    return ngDepthSamplePixelDepth(
        data,
        index,
        sampleChannel(settings.channel),
        settings.invert,
        settings.backDepth,
        settings.frontDepth,
        settings.depthScale
    );
}

private float layerToSourceX(ref DepthLayerImage layer, float documentX) {
    return documentX - cast(float)layer.left;
}

private float layerToSourceY(ref DepthLayerImage layer, float documentY) {
    return documentY - cast(float)layer.top;
}

private float sampledDepth01(float value, ref PsdDepthImportSettings settings) {
    return ngDepthSampleValueToDepth01(value, settings.backDepth, settings.frontDepth, settings.depthScale);
}

private float layerOpacity01(ubyte opacity) {
    return ngDepthSampleOpacity01(opacity);
}

private float effectiveAlpha(const(ubyte)[] rgba, size_t index, float opacity) {
    return ngDepthSampleEffectiveAlpha(rgba, index, opacity);
}

private float coverageAlpha(ref DepthLayerImage layer, int x, int y) {
    auto cached = coverageCacheValue(layer.coverageAlphaCache, layer, x, y);
    if (cached >= 0.0f) return cached;
    if (layer.coverageSources.length > 0) return coverageSourcesAlpha(layer, x, y, true);
    return coveragePixelAlpha(layer, x, y) * layer.coverageOpacity;
}

private bool coverageReliable(ref DepthLayerImage layer, int x, int y) {
    auto cached = coverageCacheValue(layer.coverageGateCache, layer, x, y);
    if (cached >= 0.0f) return cached > 0.5f;
    if (layer.coverageSources.length > 0) return coverageSourcesAlpha(layer, x, y, false) > 0.5f;
    if (layer.coverageData.length == 0) return true;
    if (layer.coverageUsesMesh) return layer.coverageData.length == 0 || coveragePixelAlpha(layer, x, y) > 0.5f;
    return ngDepthImageCoverageReliableAt(
        x,
        y,
        layer.width,
        layer.height,
        layer.coverageData,
        layer.coverageWidth,
        layer.coverageHeight,
        layer.coverageChannels
    );
}

private float coverageCacheValue(ref float[] cache, ref DepthLayerImage layer, int x, int y) {
    if (cache.length != cast(size_t)layer.width * cast(size_t)layer.height) return -1.0f;
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return 0.0f;
    return cache[cast(size_t)y * cast(size_t)layer.width + cast(size_t)x];
}

private float coverageSourcesAlpha(ref DepthLayerImage layer, int x, int y, bool includeOpacity) {
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return 0.0f;

    auto world = vec2(
        cast(float)(layer.left + x) - cast(float)layer.documentWidth / 2.0f,
        cast(float)(layer.top + y) - cast(float)layer.documentHeight / 2.0f
    );
    float combined = 0.0f;
    foreach (ref source; layer.coverageSources) {
        if (source.data.length == 0 || source.width <= 0 || source.height <= 0) continue;
        auto local = (source.worldToLocal * vec4(world, 0, 1)).xy;
        auto alpha = sampleCoverageSourceAlpha(source, local);
        if (alpha <= 0.0f) continue;
        if (includeOpacity) alpha *= source.opacity;
        combined = ngDepthImageCompositeAlpha(combined, alpha);
        if (combined >= 1.0f) return 1.0f;
    }
    return combined;
}

private float coveragePixelAlpha(ref DepthLayerImage layer, int x, int y) {
    if (layer.coverageData.length == 0 || layer.coverageWidth <= 0 || layer.coverageHeight <= 0) {
        return 1.0f;
    }
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return 0.0f;
    if (layer.coverageUsesMesh) {
        auto world = vec2(
            cast(float)(layer.left + x) - cast(float)layer.documentWidth / 2.0f,
            cast(float)(layer.top + y) - cast(float)layer.documentHeight / 2.0f
        );
        auto local = (layer.coverageWorldToLocal * vec4(world, 0, 1)).xy;
        auto uv = vec2(
            local.x / cast(float)layer.coverageWidth + 0.5f,
            local.y / cast(float)layer.coverageHeight + 0.5f
        );
        if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) return 0.0f;
        return sampleCoverageAlphaAtUv(layer, uv);
    }
    return ngDepthImageCoverageAlphaAt(
        x,
        y,
        layer.width,
        layer.height,
        layer.coverageData,
        layer.coverageWidth,
        layer.coverageHeight,
        layer.coverageChannels
    );
}

private bool barycentric(vec2 p, vec2 a, vec2 b, vec2 c, out float w0, out float w1, out float w2) {
    auto denom = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    if (denom == 0.0f) return false;
    w0 = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / denom;
    w1 = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / denom;
    w2 = 1.0f - w0 - w1;
    enum float Epsilon = -0.0001f;
    return w0 >= Epsilon && w1 >= Epsilon && w2 >= Epsilon;
}

private float sampleCoverageSourceAlpha(ref CoverageSource source, vec2 local) {
    if (source.vertices.length == source.uvs.length && source.indices.length >= 3) {
        for (size_t tri = 0; tri + 2 < source.indices.length; tri += 3) {
            auto i0 = cast(size_t)source.indices[tri + 0];
            auto i1 = cast(size_t)source.indices[tri + 1];
            auto i2 = cast(size_t)source.indices[tri + 2];
            if (i0 >= source.vertices.length || i1 >= source.vertices.length || i2 >= source.vertices.length) continue;

            auto a = source.vertices[i0] - source.origin;
            auto b = source.vertices[i1] - source.origin;
            auto c = source.vertices[i2] - source.origin;
            float w0;
            float w1;
            float w2;
            if (!barycentric(local, a, b, c, w0, w1, w2)) continue;

            auto uv = source.uvs[i0] * w0 + source.uvs[i1] * w1 + source.uvs[i2] * w2;
            if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) return 0.0f;
            return sampleCoverageSourceAlphaAtUv(source, uv);
        }
        return 0.0f;
    }

    auto uv = vec2(
        local.x / cast(float)source.width + 0.5f,
        local.y / cast(float)source.height + 0.5f
    );
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) return 0.0f;
    return sampleCoverageSourceAlphaAtUv(source, uv);
}

private float sampleCoverageSourceAlphaAtUv(ref CoverageSource source, vec2 uv) {
    return ngDepthImageCoverageAlphaAtUv(uv.x, uv.y, source.data, source.width, source.height, source.channels);
}

private void sampleCoverageSourceRgbaAtUv(ref CoverageSource source, vec2 uv, ref ubyte[4] rgba) {
    rgba[] = 0;
    if (source.data.length == 0 || source.width <= 0 || source.height <= 0 || source.channels <= 0) return;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) return;

    auto px = min(source.width - 1, max(0, cast(int)floor(uv.x * cast(float)source.width)));
    auto py = min(source.height - 1, max(0, cast(int)floor(uv.y * cast(float)source.height)));
    auto index = (cast(size_t)py * cast(size_t)source.width + cast(size_t)px) * cast(size_t)source.channels;
    if (index >= source.data.length) return;

    rgba[0] = source.data[index];
    rgba[1] = source.channels > 1 && index + 1 < source.data.length ? source.data[index + 1] : rgba[0];
    rgba[2] = source.channels > 2 && index + 2 < source.data.length ? source.data[index + 2] : rgba[0];
    rgba[3] = source.channels > 3 && index + 3 < source.data.length ? source.data[index + 3] : 255;
}

private bool sampleCoverageSourceRgba(ref CoverageSource source, vec2 local, ref ubyte[4] rgba) {
    if (source.vertices.length == source.uvs.length && source.indices.length >= 3) {
        for (size_t tri = 0; tri + 2 < source.indices.length; tri += 3) {
            auto i0 = cast(size_t)source.indices[tri + 0];
            auto i1 = cast(size_t)source.indices[tri + 1];
            auto i2 = cast(size_t)source.indices[tri + 2];
            if (i0 >= source.vertices.length || i1 >= source.vertices.length || i2 >= source.vertices.length) continue;

            auto a = source.vertices[i0] - source.origin;
            auto b = source.vertices[i1] - source.origin;
            auto c = source.vertices[i2] - source.origin;
            float w0;
            float w1;
            float w2;
            if (!barycentric(local, a, b, c, w0, w1, w2)) continue;

            auto uv = source.uvs[i0] * w0 + source.uvs[i1] * w1 + source.uvs[i2] * w2;
            sampleCoverageSourceRgbaAtUv(source, uv, rgba);
            return rgba[3] > 0;
        }
        return false;
    }

    auto uv = vec2(
        local.x / cast(float)source.width + 0.5f,
        local.y / cast(float)source.height + 0.5f
    );
    sampleCoverageSourceRgbaAtUv(source, uv, rgba);
    return rgba[3] > 0;
}

private ColorLayerPreviewRgba buildColorLayerPreviewRgba(ref DepthLayerImage image) {
    ColorLayerPreviewRgba result;
    result.width = image.width;
    result.height = image.height;
    auto expectedLength = cast(size_t)max(0, image.width * image.height) * 4;
    if (expectedLength == 0) return result;

    if (image.coverageSources.length > 0) {
        result.rgba.length = expectedLength;
        bool hasCoverage;

        void compositePixel(int x, int y, ref ubyte[4] rgba, float opacity) {
            auto srcA = clamp01((cast(float)rgba[3] / 255.0f) * opacity);
            if (srcA <= 0.0f) return;
            auto index = (cast(size_t)y * cast(size_t)image.width + cast(size_t)x) * 4;
            auto dstA = cast(float)result.rgba[index + 3] / 255.0f;
            auto outA = srcA + dstA * (1.0f - srcA);
            if (outA <= 0.0f) return;
            auto remaining = 1.0f - srcA;
            auto dstR = (cast(float)result.rgba[index + 0] / 255.0f) * dstA;
            auto dstG = (cast(float)result.rgba[index + 1] / 255.0f) * dstA;
            auto dstB = (cast(float)result.rgba[index + 2] / 255.0f) * dstA;
            auto outR = (cast(float)rgba[0] / 255.0f) * srcA + dstR * remaining;
            auto outG = (cast(float)rgba[1] / 255.0f) * srcA + dstG * remaining;
            auto outB = (cast(float)rgba[2] / 255.0f) * srcA + dstB * remaining;
            result.rgba[index + 0] = cast(ubyte)round(clamp01(outR / outA) * 255.0f);
            result.rgba[index + 1] = cast(ubyte)round(clamp01(outG / outA) * 255.0f);
            result.rgba[index + 2] = cast(ubyte)round(clamp01(outB / outA) * 255.0f);
            result.rgba[index + 3] = cast(ubyte)round(clamp01(outA) * 255.0f);
            hasCoverage = true;
        }

        foreach (ref source; image.coverageSources) {
            if (source.data.length == 0 || source.width <= 0 || source.height <= 0 || source.channels != 4) continue;
            auto useMesh = source.vertices.length == source.uvs.length && source.indices.length >= 3;
            if (!useMesh) {
                auto localToWorld = source.worldToLocal.inverse;
                auto halfWidth = cast(float)source.width / 2.0f;
                auto halfHeight = cast(float)source.height / 2.0f;
                auto corners = [
                    (localToWorld * vec4(vec2(-halfWidth, -halfHeight), 0, 1)).xy,
                    (localToWorld * vec4(vec2( halfWidth, -halfHeight), 0, 1)).xy,
                    (localToWorld * vec4(vec2(-halfWidth,  halfHeight), 0, 1)).xy,
                    (localToWorld * vec4(vec2( halfWidth,  halfHeight), 0, 1)).xy,
                ];
                auto minX = image.width - 1;
                auto minY = image.height - 1;
                auto maxX = 0;
                auto maxY = 0;
                foreach (corner; corners) {
                    auto pixel = worldToLayerPixel(image, corner);
                    minX = min(minX, max(0, cast(int)floor(pixel.x)));
                    minY = min(minY, max(0, cast(int)floor(pixel.y)));
                    maxX = max(maxX, min(image.width - 1, cast(int)ceil(pixel.x)));
                    maxY = max(maxY, min(image.height - 1, cast(int)ceil(pixel.y)));
                }
                if (minX > maxX || minY > maxY) continue;
                foreach (y; minY .. maxY + 1) {
                    foreach (x; minX .. maxX + 1) {
                        auto world = vec2(
                            cast(float)(image.left + x) - cast(float)image.documentWidth / 2.0f,
                            cast(float)(image.top + y) - cast(float)image.documentHeight / 2.0f
                        );
                        auto local = (source.worldToLocal * vec4(world, 0, 1)).xy;
                        ubyte[4] rgba;
                        if (!sampleCoverageSourceRgba(source, local, rgba)) continue;
                        compositePixel(x, y, rgba, source.opacity);
                    }
                }
                continue;
            }

            auto localToWorld = source.worldToLocal.inverse;
            for (size_t tri = 0; tri + 2 < source.indices.length; tri += 3) {
                auto i0 = cast(size_t)source.indices[tri + 0];
                auto i1 = cast(size_t)source.indices[tri + 1];
                auto i2 = cast(size_t)source.indices[tri + 2];
                if (i0 >= source.vertices.length || i1 >= source.vertices.length || i2 >= source.vertices.length) continue;

                auto aLocal = source.vertices[i0] - source.origin;
                auto bLocal = source.vertices[i1] - source.origin;
                auto cLocal = source.vertices[i2] - source.origin;
                auto aPix = worldToLayerPixel(image, (localToWorld * vec4(aLocal, 0, 1)).xy);
                auto bPix = worldToLayerPixel(image, (localToWorld * vec4(bLocal, 0, 1)).xy);
                auto cPix = worldToLayerPixel(image, (localToWorld * vec4(cLocal, 0, 1)).xy);

                auto minX = max(0, cast(int)floor(min(aPix.x, min(bPix.x, cPix.x))));
                auto maxX = min(image.width - 1, cast(int)ceil(max(aPix.x, max(bPix.x, cPix.x))));
                auto minY = max(0, cast(int)floor(min(aPix.y, min(bPix.y, cPix.y))));
                auto maxY = min(image.height - 1, cast(int)ceil(max(aPix.y, max(bPix.y, cPix.y))));
                if (minX > maxX || minY > maxY) continue;

                foreach (y; minY .. maxY + 1) {
                    foreach (x; minX .. maxX + 1) {
                        float w0;
                        float w1;
                        float w2;
                        if (!barycentric(vec2(cast(float)x + 0.5f, cast(float)y + 0.5f),
                            aPix, bPix, cPix, w0, w1, w2)) continue;
                        auto uv = source.uvs[i0] * w0 + source.uvs[i1] * w1 + source.uvs[i2] * w2;
                        ubyte[4] rgba;
                        sampleCoverageSourceRgbaAtUv(source, uv, rgba);
                        compositePixel(x, y, rgba, source.opacity);
                    }
                }
            }
        }
        if (hasCoverage) return result;

        foreach (ref source; image.coverageSources) {
            if (source.data.length == 0 || source.width <= 0 || source.height <= 0 || source.channels != 4) continue;
            auto sourceLength = cast(size_t)source.width * cast(size_t)source.height * 4;
            if (source.data.length < sourceLength) continue;
            result.rgba = source.data[0 .. sourceLength].dup;
            result.width = source.width;
            result.height = source.height;
            return result;
        }
    }

    if (image.coverageData.length >= expectedLength && image.coverageWidth == image.width &&
        image.coverageHeight == image.height && image.coverageChannels == 4) {
        result.rgba = image.coverageData[0 .. expectedLength].dup;
        return result;
    }
    if (image.coverageData.length > 0 && image.coverageWidth == image.documentWidth &&
        image.coverageHeight == image.documentHeight && image.coverageChannels == 4) {
        auto cropped = cropRgbaRect(image.coverageData, image.coverageWidth, image.coverageHeight,
            image.left, image.top, image.width, image.height);
        if (cropped.length == expectedLength) {
            result.rgba = cropped;
            return result;
        }
    }

    result.rgba = image.data.dup;
    return result;
}

private void cropDepthLayerImageToRect(ref DepthLayerImage image, int left, int top, int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (left == image.left && top == image.top && width == image.width && height == image.height) return;
    auto cropped = cropRgbaRect(image.data, image.width, image.height,
        left - image.left, top - image.top, width, height);
    if (cropped.length == 0) return;
    image.left = left;
    image.top = top;
    image.width = width;
    image.height = height;
    image.data = cropped;
    image.coverageGateCache = null;
    image.coverageAlphaCache = null;
}

private bool tightenDepthLayerImageToCoverageBounds(ref DepthLayerImage image) {
    if (image.coverageSources.length == 0 || image.width <= 0 || image.height <= 0) return false;
    bool hasBounds;
    float minX;
    float minY;
    float maxX;
    float maxY;

    void includeWorldPoint(vec2 world) {
        auto documentX = world.x + cast(float)image.documentWidth / 2.0f;
        auto documentY = world.y + cast(float)image.documentHeight / 2.0f;
        if (!hasBounds) {
            minX = maxX = documentX;
            minY = maxY = documentY;
            hasBounds = true;
        } else {
            minX = min(minX, documentX);
            minY = min(minY, documentY);
            maxX = max(maxX, documentX);
            maxY = max(maxY, documentY);
        }
    }

    foreach (ref source; image.coverageSources) {
        if (source.data.length == 0 || source.width <= 0 || source.height <= 0) continue;
        auto localToWorld = source.localToDocumentWorld;
        if (source.vertices.length > 0) {
            foreach (vertex; source.vertices) {
                includeWorldPoint((localToWorld * vec4(vertex - source.origin, 0, 1)).xy);
            }
        } else {
            auto halfWidth = cast(float)source.width / 2.0f;
            auto halfHeight = cast(float)source.height / 2.0f;
            includeWorldPoint((localToWorld * vec4(vec2(-halfWidth, -halfHeight), 0, 1)).xy);
            includeWorldPoint((localToWorld * vec4(vec2( halfWidth, -halfHeight), 0, 1)).xy);
            includeWorldPoint((localToWorld * vec4(vec2(-halfWidth,  halfHeight), 0, 1)).xy);
            includeWorldPoint((localToWorld * vec4(vec2( halfWidth,  halfHeight), 0, 1)).xy);
        }
    }
    if (!hasBounds) return false;

    enum int Padding = 2;
    auto cropLeft = max(image.left, cast(int)floor(minX) - Padding);
    auto cropTop = max(image.top, cast(int)floor(minY) - Padding);
    auto cropRight = min(image.left + image.width, cast(int)ceil(maxX) + Padding + 1);
    auto cropBottom = min(image.top + image.height, cast(int)ceil(maxY) + Padding + 1);
    if (cropRight <= cropLeft || cropBottom <= cropTop) return false;
    cropDepthLayerImageToRect(image, cropLeft, cropTop, cropRight - cropLeft, cropBottom - cropTop);
    return true;
}

private bool setDepthLayerImageToCoverageDocumentBounds(ref DepthLayerImage image) {
    if (image.coverageSources.length == 0 || image.width <= 0 || image.height <= 0) return false;
    bool hasBounds;
    int left;
    int top;
    int right;
    int bottom;

    void includeRect(int rectLeft, int rectTop, int rectWidth, int rectHeight) {
        if (rectWidth <= 0 || rectHeight <= 0) return;
        auto rectRight = rectLeft + rectWidth;
        auto rectBottom = rectTop + rectHeight;
        if (!hasBounds) {
            left = rectLeft;
            top = rectTop;
            right = rectRight;
            bottom = rectBottom;
            hasBounds = true;
        } else {
            left = min(left, rectLeft);
            top = min(top, rectTop);
            right = max(right, rectRight);
            bottom = max(bottom, rectBottom);
        }
    }

    void includeMeshBounds(ref CoverageSource source) {
        if (source.vertices.length == 0) return;
        bool hasMeshBounds;
        float minX;
        float minY;
        float maxX;
        float maxY;
        foreach (vertex; source.vertices) {
            auto world = (source.localToDocumentWorld * vec4(vertex - source.origin, 0, 1)).xy;
            auto documentX = world.x + cast(float)image.documentWidth / 2.0f;
            auto documentY = world.y + cast(float)image.documentHeight / 2.0f;
            if (!hasMeshBounds) {
                minX = maxX = documentX;
                minY = maxY = documentY;
                hasMeshBounds = true;
            } else {
                minX = min(minX, documentX);
                minY = min(minY, documentY);
                maxX = max(maxX, documentX);
                maxY = max(maxY, documentY);
            }
        }
        if (!hasMeshBounds) return;
        includeRect(
            cast(int)floor(minX),
            cast(int)floor(minY),
            max(1, cast(int)ceil(maxX) - cast(int)floor(minX) + 1),
            max(1, cast(int)ceil(maxY) - cast(int)floor(minY) + 1)
        );
    }

    foreach (ref source; image.coverageSources) {
        if (source.vertices.length > 0) {
            includeMeshBounds(source);
        } else if (source.hasDocumentRect) {
            includeRect(source.documentLeft, source.documentTop, source.documentWidth, source.documentHeight);
        }
    }
    if (!hasBounds) return tightenDepthLayerImageToCoverageBounds(image);

    left = max(image.left, left);
    top = max(image.top, top);
    right = min(image.left + image.width, right);
    bottom = min(image.top + image.height, bottom);
    if (right <= left || bottom <= top) return false;
    cropDepthLayerImageToRect(image, left, top, right - left, bottom - top);
    return true;
}

private vec2 worldToLayerPixel(ref DepthLayerImage layer, vec2 world) {
    return vec2(
        world.x + cast(float)layer.documentWidth / 2.0f - cast(float)layer.left,
        world.y + cast(float)layer.documentHeight / 2.0f - cast(float)layer.top
    );
}

private void buildCoverageCache(ref DepthLayerImage layer) {
    if (layer.coverageSources.length == 0 || layer.width <= 0 || layer.height <= 0) return;
    auto pixelCount = cast(size_t)layer.width * cast(size_t)layer.height;
    layer.coverageGateCache.length = pixelCount;
    layer.coverageAlphaCache.length = pixelCount;
    layer.coverageGateCache[] = 0.0f;
    layer.coverageAlphaCache[] = 0.0f;

    foreach (ref source; layer.coverageSources) {
        if (source.data.length == 0 || source.width <= 0 || source.height <= 0) continue;
        auto useMesh = source.vertices.length == source.uvs.length && source.indices.length >= 3;
        if (!useMesh) {
            for (int y = 0; y < layer.height; y++) {
                for (int x = 0; x < layer.width; x++) {
                    auto world = vec2(
                        cast(float)(layer.left + x) - cast(float)layer.documentWidth / 2.0f,
                        cast(float)(layer.top + y) - cast(float)layer.documentHeight / 2.0f
                    );
                    auto local = (source.worldToLocal * vec4(world, 0, 1)).xy;
                    auto alpha = sampleCoverageSourceAlpha(source, local);
                    if (alpha <= 0.0f) continue;
                    auto index = cast(size_t)y * cast(size_t)layer.width + cast(size_t)x;
                    layer.coverageGateCache[index] = ngDepthImageCompositeAlpha(layer.coverageGateCache[index], alpha);
                    layer.coverageAlphaCache[index] = ngDepthImageCompositeAlpha(
                        layer.coverageAlphaCache[index],
                        alpha * source.opacity
                    );
                }
            }
            continue;
        }

        auto localToWorld = source.localToDocumentWorld;
        for (size_t tri = 0; tri + 2 < source.indices.length; tri += 3) {
            auto i0 = cast(size_t)source.indices[tri + 0];
            auto i1 = cast(size_t)source.indices[tri + 1];
            auto i2 = cast(size_t)source.indices[tri + 2];
            if (i0 >= source.vertices.length || i1 >= source.vertices.length || i2 >= source.vertices.length) continue;

            auto aLocal = source.vertices[i0] - source.origin;
            auto bLocal = source.vertices[i1] - source.origin;
            auto cLocal = source.vertices[i2] - source.origin;
            auto aPix = worldToLayerPixel(layer, (localToWorld * vec4(aLocal, 0, 1)).xy);
            auto bPix = worldToLayerPixel(layer, (localToWorld * vec4(bLocal, 0, 1)).xy);
            auto cPix = worldToLayerPixel(layer, (localToWorld * vec4(cLocal, 0, 1)).xy);

            auto minX = max(0, cast(int)floor(min(aPix.x, min(bPix.x, cPix.x))));
            auto maxX = min(layer.width - 1, cast(int)ceil(max(aPix.x, max(bPix.x, cPix.x))));
            auto minY = max(0, cast(int)floor(min(aPix.y, min(bPix.y, cPix.y))));
            auto maxY = min(layer.height - 1, cast(int)ceil(max(aPix.y, max(bPix.y, cPix.y))));
            if (minX > maxX || minY > maxY) continue;

            foreach (y; minY .. maxY + 1) {
                foreach (x; minX .. maxX + 1) {
                    float w0;
                    float w1;
                    float w2;
                    if (!barycentric(vec2(cast(float)x + 0.5f, cast(float)y + 0.5f),
                        aPix, bPix, cPix, w0, w1, w2)) continue;
                    auto uv = source.uvs[i0] * w0 + source.uvs[i1] * w1 + source.uvs[i2] * w2;
                    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) continue;
                    auto alpha = sampleCoverageSourceAlphaAtUv(source, uv);
                    if (alpha <= 0.0f) continue;
                    auto index = cast(size_t)y * cast(size_t)layer.width + cast(size_t)x;
                    layer.coverageGateCache[index] = ngDepthImageCompositeAlpha(layer.coverageGateCache[index], alpha);
                    layer.coverageAlphaCache[index] = ngDepthImageCompositeAlpha(
                        layer.coverageAlphaCache[index],
                        alpha * source.opacity
                    );
                }
            }
        }
    }
}

private void maskDepthToCoverage(ref DepthLayerImage layer, ref PsdDepthImportSettings settings) {
    if (layer.width <= 0 || layer.height <= 0 || layer.data.length == 0) return;
    for (int y = 0; y < layer.height; y++) {
        for (int x = 0; x < layer.width; x++) {
            auto index = (cast(size_t)y * cast(size_t)layer.width + cast(size_t)x) * 4;
            if (index + 3 >= layer.data.length) continue;
            if (ngDepthSampleAcceptsAlpha(coverageAlpha(layer, x, y), settings.alphaThreshold)) continue;
            layer.data[index + 0] = 0;
            layer.data[index + 1] = 0;
            layer.data[index + 2] = 0;
            layer.data[index + 3] = 0;
        }
    }
}

private void applyDepthDrawLayerDepthCleanup(ref DepthLayerImage layer, ref PsdDepthImportSettings settings) {
    if (!settings.repairContourBand) return;
    if (layer.width <= 0 || layer.height <= 0 || layer.data.length != cast(size_t)(layer.width * layer.height * 4)) {
        return;
    }
    auto depth = ngDepthDrawDecodeDepthPixelsFromRgba(layer.data, sampleChannel(settings.channel));
    ubyte[] mask;
    if (layer.coverageAlphaCache.length == depth.length) {
        mask.length = depth.length;
        foreach (i, value; layer.coverageAlphaCache) mask[i] = value > 0.0f ? 1 : 0;
    } else if (layer.coverageData.length == layer.data.length) {
        mask = ngDepthDrawAlphaMaskFromRgba(layer.coverageData);
    } else {
        mask = ngDepthDrawAlphaMaskFromRgba(layer.data);
    }
    if (mask.length != depth.length) return;

    auto contourBand = ngDepthDrawBuildLayerContourBandMask(mask, layer.width, layer.height, 2);
    auto contourSeed = depth.dup;
    size_t remainingSeedPixels;
    foreach (i, value; contourBand) {
        if (value) contourSeed[i] = 0;
        else if (mask[i] && contourSeed[i] > 0) remainingSeedPixels++;
    }
    if (remainingSeedPixels == 0) return;
    auto repaired = ngDepthDrawInpaintMaskedLayerDepth(contourSeed, mask, layer.width, layer.height);
    foreach (i, value; repaired.pixels) {
        if (!repaired.filledMask[i]) continue;
        auto offset = i * 4;
        final switch (settings.channel) {
            case PsdDepthChannel.R:
                layer.data[offset] = value;
                break;
            case PsdDepthChannel.G:
                layer.data[offset + 1] = value;
                break;
            case PsdDepthChannel.B:
                layer.data[offset + 2] = value;
                break;
            case PsdDepthChannel.AverageRGB:
            case PsdDepthChannel.Luminance:
                layer.data[offset] = value;
                layer.data[offset + 1] = value;
                layer.data[offset + 2] = value;
                break;
        }
        layer.data[offset + 3] = mask[i] && value > 0 ? 255 : 0;
    }
}

private DepthDrawSplitLayer depthDrawSplitLayerFromCompositeSource(PsdDepthCompositeSourceLayer sourceLayer) {
    DepthDrawSplitLayer splitLayer;
    splitLayer.left = sourceLayer.left;
    splitLayer.top = sourceLayer.top;
    splitLayer.width = sourceLayer.width;
    splitLayer.height = sourceLayer.height;
    splitLayer.maskPixels = ngDepthDrawAlphaMaskFromRgba(sourceLayer.rgba);
    splitLayer.alphaMask = splitLayer.maskPixels.dup;
    return splitLayer;
}

private ubyte[] depthDrawMaskFromDepthLayerImage(ref DepthLayerImage image) {
    auto pixelCount = cast(size_t)max(0, image.width * image.height);
    ubyte[] mask;
    mask.length = pixelCount;
    if (pixelCount == 0) return mask;

    if (image.coverageAlphaCache.length == pixelCount) {
        foreach (i, value; image.coverageAlphaCache) mask[i] = value > 0.0f ? 1 : 0;
        return mask;
    }
    if (image.coverageData.length >= pixelCount * 4 &&
        image.coverageWidth == image.width &&
        image.coverageHeight == image.height &&
        image.coverageChannels == 4) {
        return ngDepthDrawAlphaMaskFromRgba(image.coverageData[0 .. pixelCount * 4]);
    }

    bool hasDocumentRectSource;
    foreach (ref source; image.coverageSources) {
        if (!source.hasDocumentRect ||
            source.data.length == 0 ||
            source.width <= 0 ||
            source.height <= 0 ||
            source.channels <= 0) {
            continue;
        }
        hasDocumentRectSource = true;
        for (int y = 0; y < image.height; y++) {
            auto sourceY = image.top + y - source.documentTop;
            if (sourceY < 0 || sourceY >= source.height) continue;
            for (int x = 0; x < image.width; x++) {
                auto sourceX = image.left + x - source.documentLeft;
                if (sourceX < 0 || sourceX >= source.width) continue;
                auto sourceIndex = (cast(size_t)sourceY * cast(size_t)source.width + cast(size_t)sourceX) *
                    cast(size_t)source.channels;
                if (sourceIndex >= source.data.length) continue;
                auto alpha = source.channels > 3 ? source.data[sourceIndex + 3] : 255;
                if (alpha == 0) continue;
                mask[cast(size_t)y * cast(size_t)image.width + cast(size_t)x] = 1;
            }
        }
    }
    if (hasDocumentRectSource) return mask;

    if (image.data.length >= pixelCount * 4) {
        return ngDepthDrawAlphaMaskFromRgba(image.data[0 .. pixelCount * 4]);
    }
    return mask;
}

private DepthDrawSplitLayer depthDrawSplitLayerFromDepthLayerImage(ref DepthLayerImage image) {
    DepthDrawSplitLayer splitLayer;
    splitLayer.left = image.left;
    splitLayer.top = image.top;
    splitLayer.width = image.width;
    splitLayer.height = image.height;
    splitLayer.maskPixels = depthDrawMaskFromDepthLayerImage(image);
    splitLayer.alphaMask = splitLayer.maskPixels.dup;
    return splitLayer;
}

private void setLayerDepthFromDepthDrawPixels(ref DepthLayerImage image, const(ubyte)[] depthPixels, const(ubyte)[] alphaMask = null) {
    if (depthPixels.length != cast(size_t)(image.width * image.height)) return;
    if (alphaMask.length && alphaMask.length != depthPixels.length) return;
    image.data.length = cast(size_t)image.width * cast(size_t)image.height * 4;
    foreach (i, value; depthPixels) {
        auto offset = i * 4;
        image.data[offset + 0] = value;
        image.data[offset + 1] = value;
        image.data[offset + 2] = value;
        image.data[offset + 3] = value > 0 && (!alphaMask.length || alphaMask[i]) ? 255 : 0;
    }
}

private bool hasVisibleDepthDrawPixels(const(ubyte)[] depthPixels, const(ubyte)[] alphaMask = null) {
    foreach (i, value; depthPixels) {
        if (value == 0) continue;
        if (alphaMask.length && (i >= alphaMask.length || alphaMask[i] == 0)) continue;
        return true;
    }
    return false;
}

private void seedLayerDepthFromDepthDrawSplit(
    ref DepthLayerImage image,
    DepthDrawSplitLayer splitLayer,
    int layerIndex,
    int documentWidth,
    int documentHeight,
    const(ubyte)[] stableDepthPixels,
    const(int)[] visibleLayerMap,
    const(DepthDrawSplitLayer)[] splitLayers,
    bool repairContourBand
) {
    if (!repairContourBand) {
        ubyte[] sampledDepth;
        sampledDepth.length = cast(size_t)splitLayer.width * cast(size_t)splitLayer.height;
        foreach (y; 0 .. splitLayer.height) {
            foreach (x; 0 .. splitLayer.width) {
                auto localIndex = cast(size_t)y * cast(size_t)splitLayer.width + cast(size_t)x;
                if (!splitLayer.maskPixels[localIndex]) continue;
                auto globalX = splitLayer.left + x;
                auto globalY = splitLayer.top + y;
                if (globalX < 0 || globalX >= documentWidth || globalY < 0 || globalY >= documentHeight) continue;
                sampledDepth[localIndex] = stableDepthPixels[
                    cast(size_t)globalY * cast(size_t)documentWidth + cast(size_t)globalX];
            }
        }
        setLayerDepthFromDepthDrawPixels(image, sampledDepth, splitLayer.maskPixels);
        return;
    }

    ubyte[] contourBand;
    contourBand = ngDepthDrawBuildLayerContourBandMask(
        splitLayer.maskPixels, splitLayer.width, splitLayer.height, 2);
    bool hasInteriorSeed;
    foreach (i, maskPixel; splitLayer.maskPixels) {
        if (maskPixel && !contourBand[i]) {
            hasInteriorSeed = true;
            break;
        }
    }
    if (!hasInteriorSeed) contourBand = null;
    auto seeded = ngDepthDrawSeedLayerDepthPixels(
        splitLayer,
        layerIndex,
        documentWidth,
        documentHeight,
        stableDepthPixels,
        visibleLayerMap,
        splitLayer.maskPixels,
        splitLayers,
        contourBand,
        2
    );
    DepthDrawPruneLayer pruneLayer;
    pruneLayer.left = splitLayer.left;
    pruneLayer.top = splitLayer.top;
    pruneLayer.width = splitLayer.width;
    pruneLayer.height = splitLayer.height;
    auto pruned = ngDepthDrawPruneForeignDepthSeeds(
        seeded,
        pruneLayer,
        layerIndex,
        documentWidth,
        documentHeight,
        stableDepthPixels,
        visibleLayerMap,
        splitLayer.maskPixels,
        24.0
    );
    if (hasVisibleDepthDrawPixels(pruned.pixels, splitLayer.maskPixels)) {
        setLayerDepthFromDepthDrawPixels(image, pruned.pixels, splitLayer.maskPixels);
    } else if (hasVisibleDepthDrawPixels(seeded, splitLayer.maskPixels)) {
        setLayerDepthFromDepthDrawPixels(image, seeded, splitLayer.maskPixels);
    } else {
        // An empty split is authoritative. Retaining the pre-split flat PNG
        // depth here makes fully occluded layers participate in sampling.
        setLayerDepthFromDepthDrawPixels(image, seeded, splitLayer.maskPixels);
    }
}

private float sampleCoverageAlphaAtUv(ref DepthLayerImage layer, vec2 uv) {
    return ngDepthImageCoverageAlphaAtUv(
        uv.x,
        uv.y,
        layer.coverageData,
        layer.coverageWidth,
        layer.coverageHeight,
        layer.coverageChannels
    );
}

private float effectiveAlpha(ref DepthLayerImage layer, size_t index, int x, int y) {
    return effectiveAlpha(layer.data, index, layer.opacity) * coverageAlpha(layer, x, y);
}

private float surfaceAlpha(ref DepthLayerImage layer, size_t index, int x, int y) {
    if (layer.coverageSources.length > 0 || layer.coverageData.length > 0) {
        return coverageAlpha(layer, x, y);
    }
    return effectiveAlpha(layer.data, index, layer.opacity);
}

private ubyte effectiveAlphaByte(ref DepthLayerImage layer, size_t index, int x, int y) {
    auto alpha = surfaceAlpha(layer, index, x, y);
    return ngDepthSampleAlphaByte(alpha);
}

private bool acceptsDepthPixel(ref DepthLayerImage layer, size_t index, int x, int y, ref PsdDepthImportSettings settings) {
    if (index + 3 >= layer.data.length) return false;
    if (!coverageReliable(layer, x, y)) return false;
    if (!ngDepthSampleAcceptsAlpha(surfaceAlpha(layer, index, x, y), settings.alphaThreshold)) return false;
    if (settings.zeroDepthIsMissing) {
        auto sampleSettings = settings;
        if (pixelDepth01(layer.data, index, sampleSettings) <= 0.0f) return false;
    }
    return true;
}

private ptrdiff_t bestMatchingColorSourceLayer(
    PsdDepthCompositeSourceLayer depthLayer,
    PsdDepthCompositeSourceLayer[] colorLayers
) {
    ptrdiff_t bestIndex = -1;
    float bestScore = 0.0f;
    foreach (i, colorLayer; colorLayers) {
        auto score = compositeSourceLayerMatchScore(colorLayer, depthLayer);
        if (score > bestScore) {
            bestScore = score;
            bestIndex = cast(ptrdiff_t)i;
        }
    }
    return bestIndex;
}

private void attachCompositeSourceCoverage(
    ref DepthLayerImage image,
    PsdDepthCompositeSourceLayer colorLayer
) {
    if (image.width <= 0 || image.height <= 0 || colorLayer.width <= 0 || colorLayer.height <= 0) return;
    auto colorLength = cast(size_t)colorLayer.width * cast(size_t)colorLayer.height * 4;
    if (colorLayer.rgba.length < colorLength) return;

    image.coverageWidth = image.width;
    image.coverageHeight = image.height;
    image.coverageChannels = 4;
    image.coverageOpacity = colorLayer.visible ? colorLayer.opacity : 0.0f;
    image.coverageData.length = cast(size_t)image.width * cast(size_t)image.height * 4;
    foreach (y; 0 .. image.height) {
        foreach (x; 0 .. image.width) {
            auto sourceX = image.left + x - colorLayer.left;
            auto sourceY = image.top + y - colorLayer.top;
            if (sourceX < 0 || sourceY < 0 || sourceX >= colorLayer.width || sourceY >= colorLayer.height) continue;
            auto sourceIndex = (cast(size_t)sourceY * cast(size_t)colorLayer.width + cast(size_t)sourceX) * 4;
            auto targetIndex = (cast(size_t)y * cast(size_t)image.width + cast(size_t)x) * 4;
            image.coverageData[targetIndex .. targetIndex + 4] = colorLayer.rgba[sourceIndex .. sourceIndex + 4];
        }
    }
}

version(CommandBrowserDifferential) {
    ubyte[] ngPsdDepthAttachCompositeSourceCoverageForRegression(
        int depthLeft,
        int depthTop,
        int depthWidth,
        int depthHeight,
        PsdDepthCompositeSourceLayer colorLayer
    ) {
        DepthLayerImage image;
        image.left = depthLeft;
        image.top = depthTop;
        image.width = depthWidth;
        image.height = depthHeight;
        attachCompositeSourceCoverage(image, colorLayer);
        return image.coverageData;
    }
}

private struct PsdClippingBaseState {
    int left;
    int top;
    int width;
    int height;
    bool visible;
    float opacity;
    ubyte[] alpha;
}

private PsdClippingBaseState psdClippingBaseState(ref DepthLayerImage image, bool visible, float opacity) {
    PsdClippingBaseState state;
    state.left = image.left;
    state.top = image.top;
    state.width = image.width;
    state.height = image.height;
    state.visible = visible;
    state.opacity = opacity;
    state.alpha.length = cast(size_t)max(0, image.width * image.height);
    foreach (i, ref value; state.alpha) value = image.data[i * 4 + 3];
    return state;
}

private void applyPsdClippingBaseAlpha(ref DepthLayerImage image, ref PsdClippingBaseState base) {
    auto pixelCount = cast(size_t)max(0, image.width * image.height);
    if (image.data.length < pixelCount * 4 || base.alpha.length != cast(size_t)(base.width * base.height)) return;
    ubyte[] alpha;
    alpha.length = pixelCount;
    foreach (i; 0 .. pixelCount) alpha[i] = image.data[i * 4 + 3];
    auto clippedAlpha = ngDepthDrawApplyPsdMaskToAlpha(
        alpha,
        image.width,
        image.height,
        image.left,
        image.top,
        base.alpha,
        base.width,
        base.height,
        base.left,
        base.top,
        false,
        0
    );
    foreach (i, value; clippedAlpha) image.data[i * 4 + 3] = value;
}

private ubyte[] buildDepthMaskPreview(ref DepthLayerImage layer, ref PsdDepthImportSettings settings) {
    ubyte[] result;
    auto pixels = cast(size_t)max(0, layer.width * layer.height);
    result.length = pixels * 4;
    for (int y = 0; y < layer.height; y++) {
        for (int x = 0; x < layer.width; x++) {
            auto index = (cast(size_t)y * cast(size_t)layer.width + cast(size_t)x) * 4;
            if (!acceptsDepthPixel(layer, index, x, y, settings)) {
                result[index + 0] = 0;
                result[index + 1] = 0;
                result[index + 2] = 0;
                result[index + 3] = 0;
                continue;
            }
            auto sampleSettings = settings;
            auto gray = cast(ubyte)round(pixelDepth01(layer.data, index, sampleSettings) * 255.0f);
            result[index + 0] = gray;
            result[index + 1] = gray;
            result[index + 2] = gray;
            result[index + 3] = 255;
        }
    }
    return result;
}

private ubyte[] buildDepthMaskPreviewFromSurface(
    ref DepthLayerImage layer,
    ref PsdDepthImportSettings settings,
    const(ubyte)[] surfaceRgba
) {
    ubyte[] result;
    auto pixels = cast(size_t)max(0, layer.width * layer.height);
    result.length = pixels * 4;
    auto sampleSettings = settings;
    for (int y = 0; y < layer.height; y++) {
        for (int x = 0; x < layer.width; x++) {
            auto index = (cast(size_t)y * cast(size_t)layer.width + cast(size_t)x) * 4;
            if (index + 3 >= layer.data.length || index + 3 >= surfaceRgba.length) continue;
            auto surfaceAlpha01 = cast(float)surfaceRgba[index + 3] / 255.0f;
            auto coverageAlpha01 = coverageAlpha(layer, x, y);
            auto depth01 = pixelDepth01(layer.data, index, sampleSettings);
            if (!ngDepthSampleAcceptsAlpha(surfaceAlpha01, settings.alphaThreshold) ||
                !ngDepthSampleAcceptsAlpha(coverageAlpha01, settings.alphaThreshold) ||
                !coverageReliable(layer, x, y) ||
                depth01 <= 0.0f) {
                continue;
            }
            auto gray = cast(ubyte)round(depth01 * 255.0f);
            result[index + 0] = gray;
            result[index + 1] = gray;
            result[index + 2] = gray;
            result[index + 3] = 255;
        }
    }
    return result;
}

private ubyte[] sampleDepthToSurfaceLayer(
    const(ubyte)[] depthRgba,
    int depthWidth,
    int depthHeight,
    int layerLeft,
    int layerTop,
    int layerWidth,
    int layerHeight
) {
    ubyte[] result;
    if (layerWidth <= 0 || layerHeight <= 0) return result;
    result.length = cast(size_t)layerWidth * cast(size_t)layerHeight * 4;
    foreach (y; 0 .. layerHeight) {
        auto depthY = layerTop + y;
        foreach (x; 0 .. layerWidth) {
            auto depthX = layerLeft + x;
            auto outIndex = (cast(size_t)y * cast(size_t)layerWidth + cast(size_t)x) * 4;
            if (depthX < 0 || depthY < 0 || depthX >= depthWidth || depthY >= depthHeight) continue;
            auto inIndex = (cast(size_t)depthY * cast(size_t)depthWidth + cast(size_t)depthX) * 4;
            if (inIndex + 3 >= depthRgba.length) continue;
            result[outIndex .. outIndex + 4] = depthRgba[inIndex .. inIndex + 4];
        }
    }
    return result;
}

    private ubyte[] sampleDepthToSurfaceLayerFlippedY(
        const(ubyte)[] depthRgba,
        int depthWidth,
        int depthHeight,
        int layerLeft,
    int layerTop,
    int layerWidth,
    int layerHeight
) {
    return sampleDepthToSurfaceLayer(
        depthRgba,
        depthWidth,
        depthHeight,
        layerLeft,
        depthHeight - layerTop - layerHeight,
        layerWidth,
        layerHeight
    );
}

private size_t depthSurfaceOverlap(
    const(ubyte)[] depthRgba,
    const(ubyte)[] surfaceRgba
) {
    size_t result;
    auto pixels = min(depthRgba.length, surfaceRgba.length) / 4;
    foreach (i; 0 .. pixels) {
        auto index = i * 4;
        if (surfaceRgba[index + 3] >= 3 && depthRgba[index] > 0) result++;
    }
    return result;
}

private void compositeFlatDepthByCoverageSources(
    ref DepthLayerImage image,
    const(ubyte)[] depthRgba,
    int depthWidth,
    int depthHeight
) {
    if (image.coverageSources.length == 0 || image.width <= 0 || image.height <= 0) return;
    ubyte[] result;
    result.length = cast(size_t)image.width * cast(size_t)image.height * 4;

    foreach (ref source; image.coverageSources) {
        if (source.data.length == 0 || source.width <= 0 || source.height <= 0 || source.channels <= 0) continue;
        auto sourceLength = cast(size_t)source.width * cast(size_t)source.height * cast(size_t)source.channels;
        if (source.data.length < sourceLength) continue;

        auto localToWorld = source.localToDocumentWorld;
        for (int y = 0; y < source.height; y++) {
            for (int x = 0; x < source.width; x++) {
                auto sourceIndex = (cast(size_t)y * cast(size_t)source.width + cast(size_t)x) *
                    cast(size_t)source.channels;
                auto alpha = source.channels > 3 ? source.data[sourceIndex + 3] : 255;
                if (alpha == 0) continue;

                auto local = vec2(
                    cast(float)x + 0.5f - cast(float)source.width / 2.0f,
                    cast(float)y + 0.5f - cast(float)source.height / 2.0f
                );
                auto world = (localToWorld * vec4(local, 0, 1)).xy;
                auto documentX = world.x + cast(float)image.documentWidth / 2.0f;
                auto documentY = world.y + cast(float)image.documentHeight / 2.0f;
                auto outX = cast(int)floor(documentX) - image.left;
                auto outY = cast(int)floor(documentY) - image.top;
                if (outX < 0 || outY < 0 || outX >= image.width || outY >= image.height) continue;

                auto depthX = cast(int)floor(documentX * cast(float)depthWidth / cast(float)max(1, image.documentWidth));
                auto depthY = cast(int)floor(documentY * cast(float)depthHeight / cast(float)max(1, image.documentHeight));
                if (depthX < 0 || depthY < 0 || depthX >= depthWidth || depthY >= depthHeight) continue;

                auto depthIndex = (cast(size_t)depthY * cast(size_t)depthWidth + cast(size_t)depthX) * 4;
                auto outIndex = (cast(size_t)outY * cast(size_t)image.width + cast(size_t)outX) * 4;
                if (depthIndex + 3 >= depthRgba.length || outIndex + 3 >= result.length) continue;
                result[outIndex + 0] = depthRgba[depthIndex + 0];
                result[outIndex + 1] = depthRgba[depthIndex + 1];
                result[outIndex + 2] = depthRgba[depthIndex + 2];
                result[outIndex + 3] = 255;
            }
        }
    }

    image.data = result;
}

private PsdDepthLayerDepthStats computeLayerDepthStats(ref DepthLayerImage layer, ref PsdDepthImportSettings settings) {
    PsdDepthLayerDepthStats stats;
    bool hasDepth;
    float previousDepth01;
    bool hasPrevious;
    double adjacentSum;
    size_t adjacentCount;

    for (int y = 0; y < layer.height; y++) {
        for (int x = 0; x < layer.width; x++) {
            auto index = (cast(size_t)y * cast(size_t)layer.width + cast(size_t)x) * 4;
            if (!acceptsDepthPixel(layer, index, x, y, settings)) {
                continue;
            }

            auto sampleSettings = settings;
            auto depth01 = pixelDepth01(layer.data, index, sampleSettings);
            stats.maskedPixels++;
            if (depth01 <= 0.0f) stats.zeroPixels++;
            if (!hasDepth) {
                stats.minDepth01 = depth01;
                stats.maxDepth01 = depth01;
                hasDepth = true;
            } else {
                if (depth01 < stats.minDepth01) stats.minDepth01 = depth01;
                if (depth01 > stats.maxDepth01) stats.maxDepth01 = depth01;
            }
            if (hasPrevious) {
                adjacentSum += abs(depth01 - previousDepth01);
                adjacentCount++;
            }
            previousDepth01 = depth01;
            hasPrevious = true;
        }
        hasPrevious = false;
    }

    stats.hasDepth = hasDepth;
    if (hasDepth) stats.rangeDepth01 = stats.maxDepth01 - stats.minDepth01;
    if (adjacentCount > 0) stats.adjacentDelta01 = cast(float)(adjacentSum / cast(double)adjacentCount);
    return stats;
}

private DepthSample samplePixel(ref DepthLayerImage layer, int x, int y, ref PsdDepthImportSettings settings) {
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return DepthSample(false, 0, 0);
    auto index = (cast(size_t)y * cast(size_t)layer.width + cast(size_t)x) * 4;
    if (!acceptsDepthPixel(layer, index, x, y, settings)) return DepthSample(false, 0, 0);
    auto alpha = surfaceAlpha(layer, index, x, y);
    auto sampleSettings = settings;
    return DepthSample(true, pixelDepth(layer.data, index, sampleSettings), alpha);
}

private DepthSample aggregateToDepthSample(DepthSampleAggregate aggregate) {
    return DepthSample(aggregate.valid, aggregate.value, aggregate.weight);
}

private DepthSample sampleLayer(ref DepthLayerImage layer, float x, float y, ref PsdDepthImportSettings settings) {
    auto cx = cast(int)round(x);
    auto cy = cast(int)round(y);
    auto convolution = sampleConvolution(settings.convolution);

    DepthSamplePoint sampleAt(int sampleX, int sampleY) {
        auto sample = samplePixel(layer, sampleX, sampleY, settings);
        if (!sample.valid) return ngDepthSampleMissingPoint();
        return DepthSamplePoint(sample.valid, sample.value, sample.weight);
    }

    return aggregateToDepthSample(ngDepthSampleConvolve!sampleAt(convolution, settings.customRadius, cx, cy));
}

private Deformable containingDepthTarget(Node node) {
    auto cursor = node;
    while (cursor !is null) {
        if (auto grid = cast(GridDeformer)cursor) return grid;
        if (auto path = cast(PathDeformer)cursor) return path;
        cursor = cursor.parent;
    }
    return null;
}

GridDeformer ngPsdDepthContainingGrid(Node node) {
    return cast(GridDeformer)containingDepthTarget(node);
}

Deformable ngPsdDepthContainingTarget(Node node) {
    return containingDepthTarget(node);
}

private float partLayerOverlapScore(
    Part part,
    int layerLeft,
    int layerTop,
    int layerWidth,
    int layerHeight,
    int documentWidth,
    int documentHeight
) {
    if (part is null || layerWidth <= 0 || layerHeight <= 0 || documentWidth <= 0 || documentHeight <= 0) {
        return 0.0f;
    }
    auto bounds = part.bounds;
    auto partLeft = bounds.x + cast(float)documentWidth / 2.0f;
    auto partTop = bounds.y + cast(float)documentHeight / 2.0f;
    auto partWidth = bounds.z - bounds.x;
    auto partHeight = bounds.w - bounds.y;
    return ngPsdDepthRectOverlapScore(
        cast(float)layerLeft,
        cast(float)layerTop,
        cast(float)layerWidth,
        cast(float)layerHeight,
        partLeft,
        partTop,
        partWidth,
        partHeight
    );
}

private Candidate[] matchCandidates(
    Puppet puppet,
    string layerPath,
    string layerName,
    ref PsdDepthImportSettings settings,
    int layerLeft = 0,
    int layerTop = 0,
    int layerWidth = 0,
    int layerHeight = 0,
    int documentWidth = 0,
    int documentHeight = 0,
    bool useBoundsFallback = false
) {
    Candidate[] candidates;

    auto parts = puppet.findNodesType!Part(puppet.root);
    foreach (part; parts) {
        auto path = activeArtLayerPath(part);
        if (path.length == 0) continue;
        if (path == layerPath) {
            candidates ~= Candidate(part, 0, 1.0f);
        } else if (baseName(path) == layerName) {
            candidates ~= Candidate(part, 1, 1.0f);
        } else if (part.name == layerName) {
            candidates ~= Candidate(part, 2, 1.0f);
        } else if (ngPsdDepthNormalizeLayerName(path) == ngPsdDepthNormalizeLayerName(layerName) ||
            ngPsdDepthNormalizeLayerName(part.name) == ngPsdDepthNormalizeLayerName(layerName)) {
            candidates ~= Candidate(part, 3, 1.0f);
        } else if (useBoundsFallback) {
            auto overlap = partLayerOverlapScore(part, layerLeft, layerTop, layerWidth, layerHeight,
                documentWidth, documentHeight);
            if (overlap > 0.0f) candidates ~= Candidate(part, 4, overlap);
        }
    }

    if (settings.matchDirectGridName) {
        auto grids = puppet.findNodesType!GridDeformer(puppet.root);
        foreach (grid; grids) {
            if (grid.name == layerName) {
                candidates ~= Candidate(grid, 5, 1.0f);
            }
        }
        auto paths = puppet.findNodesType!PathDeformer(puppet.root);
        foreach (path; paths) {
            if (path.name == layerName) {
                candidates ~= Candidate(path, 5, 1.0f);
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
    if (best.length > 1) {
        float bestOverlap = best[0].overlapScore;
        foreach (candidate; best) {
            if (candidate.overlapScore > bestOverlap) bestOverlap = candidate.overlapScore;
        }
        Candidate[] overlapBest;
        foreach (candidate; best) {
            if (candidate.overlapScore == bestOverlap) overlapBest ~= candidate;
        }
        best = overlapBest;
    }
    return best;
}

private bool targetHasArtCoverage(Puppet puppet, Deformable target) {
    if (puppet is null || puppet.root is null || target is null) return false;
    foreach (part; puppet.findNodesType!Part(puppet.root)) {
        if (!isActiveArtPart(part)) continue;
        if (part.textures.length == 0 || part.textures[0] is null) continue;
        if (containingDepthTarget(part) is target || nodeCapturedByGrid(target, part)) {
            return true;
        }
    }
    return false;
}

private bool isActiveArtPart(Part part) {
    return part !is null && part.renderEnabled() && (cast(DynamicComposite)part) is null;
}

private string activeArtLayerName(Part part) {
    if (!isActiveArtPart(part)) return null;
    if (part.name.length) return part.name;
    return part.uuid.to!string;
}

private string activeArtLayerPath(Part part) {
    if (part is null) return null;
    if (auto expart = cast(ExPart)part) {
        if (expart.layerPath.length) return expart.layerPath;
    }
    auto name = activeArtLayerName(part);
    return name.length ? "/" ~ name : null;
}

private string uniqueActiveArtLayerPath(Part part, ref bool[string] usedLayerPaths) {
    auto path = activeArtLayerPath(part);
    if (path.length == 0) return null;
    if (path !in usedLayerPaths) {
        usedLayerPaths[path] = true;
        return path;
    }
    auto uniquePath = "%s#%s".format(path, part.uuid);
    usedLayerPaths[uniquePath] = true;
    return uniquePath;
}

private Deformable activeArtDepthTarget(Puppet puppet, Part part) {
    if (puppet is null || puppet.root is null || !isActiveArtPart(part)) return null;
    if (auto target = containingDepthTarget(part)) return target;
    foreach (grid; puppet.findNodesType!GridDeformer(puppet.root)) {
        if (nodeCapturedByGrid(grid, part)) return grid;
    }
    foreach (path; puppet.findNodesType!PathDeformer(puppet.root)) {
        if (nodeCapturedByGrid(path, part)) return path;
    }
    return null;
}

private size_t findAccum(ref GridAccum[] accums, Deformable grid) {
    foreach (i, ref accum; accums) {
        if (accum.grid is grid) return i;
    }
    auto vertices = grid.vertices;
    GridAccum accum;
    accum.grid = grid;
    accum.best.length = vertices.length;
    accum.baseBest.length = vertices.length;
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

private float clamp01(float value) {
    return max(0.0f, min(1.0f, value));
}

private void attachPartCoverage(ref DepthLayerImage image, Part part) {
    bool[ulong] seen;
    attachPartCoverage(image, part, seen);
}

private void attachPartCoverage(ref DepthLayerImage image, Part part, ref bool[ulong] seen) {
    if (part is null) return;
    if (part.uuid in seen) return;
    seen[part.uuid] = true;
    if (part.textures.length == 0) return;
    auto texture = part.textures[0];
    if (texture is null || texture.width <= 0 || texture.height <= 0) return;

    auto channels = texture.channels;
    if (channels <= 0) return;
    auto data = texture.getTextureData(true);
    auto expected = cast(size_t)texture.width * cast(size_t)texture.height * cast(size_t)channels;
    if (data.length < expected) return;

    CoverageSource source;
    source.width = texture.width;
    source.height = texture.height;
    source.channels = channels;
    source.opacity = clamp01(part.opacity);
    source.data = data;
    source.worldToLocal = part.transform.matrix.inverse;
    source.localToDocumentWorld = part.transform.matrix;
    auto mesh = part.getMesh();
    source.vertices = mesh.vertices;
    source.uvs = mesh.uvs;
    source.indices = mesh.indices.dup;
    source.origin = mesh.origin;
    auto center = (source.localToDocumentWorld * vec4(vec2(0, 0), 0, 1)).xy;
    auto documentLeft = cast(int)round(center.x + cast(float)image.documentWidth / 2.0f -
        cast(float)source.width / 2.0f);
    auto documentTop = cast(int)round(center.y + cast(float)image.documentHeight / 2.0f -
        cast(float)source.height / 2.0f);
    source.hasDocumentRect = true;
    source.documentLeft = documentLeft;
    source.documentTop = documentTop;
    source.documentWidth = source.width;
    source.documentHeight = source.height;
    image.coverageSources ~= source;
}

private bool nodeTreeContains(Node root, Node target) {
    if (root is null || target is null) return false;
    if (root is target) return true;
    foreach (child; root.children) {
        if (nodeTreeContains(child, target)) return true;
    }
    return false;
}

private bool nodeCapturedByGrid(Deformable grid, Node target) {
    if (grid is null || target is null) return false;
    foreach (child; grid.children) {
        if (nodeTreeContains(child, target)) return true;
    }
    return false;
}

private void attachNodeCoverage(ref DepthLayerImage image, Node node, ref bool[ulong] seen) {
    if (node is null) return;
    if (auto part = cast(Part)node) {
        if (isActiveArtPart(part)) {
            attachPartCoverage(image, part, seen);
            return;
        }
    }
    foreach (child; node.children) {
        attachNodeCoverage(image, child, seen);
    }
}

private void attachGridCoverage(ref DepthLayerImage image, Puppet puppet, Deformable grid, ref bool[ulong] seen) {
    if (puppet is null || puppet.root is null || grid is null) return;
    foreach (part; puppet.findNodesType!Part(puppet.root)) {
        if (!isActiveArtPart(part)) continue;
        if (containingDepthTarget(part) is grid || nodeCapturedByGrid(grid, part)) {
            attachPartCoverage(image, part, seen);
        }
    }
}

private void attachMatchedCoverage(ref DepthLayerImage image, Puppet puppet, Node matchedNode, Deformable grid) {
    bool[ulong] seen;
    if (auto part = cast(Part)matchedNode) {
        if (isActiveArtPart(part)) {
            attachPartCoverage(image, part, seen);
            return;
        }
    }
    attachGridCoverage(image, puppet, grid, seen);
    if (image.coverageSources.length == 0) {
        attachNodeCoverage(image, matchedNode, seen);
    }
}

string ngPsdDepthGridLayerKey(ulong gridUuid, string layerPath) {
    return "%s\n%s".format(gridUuid, layerPath);
}

bool ngPsdDepthGridEnabled(ref PsdDepthImportSettings settings, ulong gridUuid) {
    auto key = gridUuid.to!string;
    if (auto disabled = key in settings.disabledGridUuids) return !*disabled;
    return true;
}

bool ngPsdDepthGridLayerEnabled(ref PsdDepthImportSettings settings, ulong gridUuid, string layerPath) {
    auto key = ngPsdDepthGridLayerKey(gridUuid, layerPath);
    if (auto disabled = key in settings.disabledGridLayerKeys) return !*disabled;
    return true;
}

private Deformable findGridByUuid(Puppet puppet, string uuid) {
    auto grids = puppet.findNodesType!GridDeformer(puppet.root);
    foreach (grid; grids) {
        if (grid.uuid.to!string == uuid) return grid;
    }
    auto paths = puppet.findNodesType!PathDeformer(puppet.root);
    foreach (path; paths) {
        if (path.uuid.to!string == uuid) return path;
    }
    return null;
}

private vec2 gridVertexDocumentPosition(Deformable grid, vec2 vertex, int documentWidth, int documentHeight) {
    auto world = grid.transform.matrix * vec4(vertex, 0, 1);
    return vec2(
        world.x + cast(float)documentWidth / 2.0f,
        world.y + cast(float)documentHeight / 2.0f
    );
}

vec2 ngPsdDepthGridVertexDocumentPosition(GridDeformer grid, vec2 vertex, int documentWidth, int documentHeight) {
    return gridVertexDocumentPosition(grid, vertex, documentWidth, documentHeight);
}

vec2 ngPsdDepthTargetPointDocumentPosition(Deformable target, vec2 vertex, int documentWidth, int documentHeight) {
    return gridVertexDocumentPosition(target, vertex, documentWidth, documentHeight);
}

PsdDepthSampleResult ngPsdDepthSamplePixels(
    const(ubyte)[] rgba,
    int width,
    int height,
    float x,
    float y,
    PsdDepthImportSettings settings
) {
    return ngPsdDepthSamplePixelsWithOpacity(rgba, width, height, x, y, 1.0f, settings);
}

PsdDepthSampleResult ngPsdDepthSamplePixelsWithOpacity(
    const(ubyte)[] rgba,
    int width,
    int height,
    float x,
    float y,
    float opacity,
    PsdDepthImportSettings settings
) {
    enforce(width >= 0 && height >= 0, "Image dimensions must be non-negative");
    enforce(rgba.length >= cast(size_t)max(0, width * height) * 4, "RGBA buffer is smaller than dimensions");
    DepthLayerImage layer;
    layer.width = width;
    layer.height = height;
    layer.opacity = clamp01(opacity);
    layer.data = rgba.dup;
    auto sample = sampleLayer(layer, x, y, settings);
    return PsdDepthSampleResult(sample.valid, sample.value);
}

PsdDepthSampleResult ngPsdDepthSamplePixelsWithCoverage(
    const(ubyte)[] rgba,
    int width,
    int height,
    const(ubyte)[] coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    PsdDepthImportSettings settings
) {
    enforce(width >= 0 && height >= 0, "Image dimensions must be non-negative");
    enforce(coverageWidth >= 0 && coverageHeight >= 0, "Coverage dimensions must be non-negative");
    enforce(rgba.length >= cast(size_t)max(0, width * height) * 4, "RGBA buffer is smaller than dimensions");
    enforce(coverageRgba.length >= cast(size_t)max(0, coverageWidth * coverageHeight) * 4,
        "Coverage RGBA buffer is smaller than dimensions");
    DepthLayerImage layer;
    layer.width = width;
    layer.height = height;
    layer.data = rgba.dup;
    layer.coverageWidth = coverageWidth;
    layer.coverageHeight = coverageHeight;
    layer.coverageChannels = 4;
    layer.coverageOpacity = clamp01(coverageOpacity);
    layer.coverageData = coverageRgba.dup;
    auto sample = sampleLayer(layer, x, y, settings);
    return PsdDepthSampleResult(sample.valid, sample.value);
}

PsdDepthSampleResult ngPsdDepthFrontmost(PsdDepthSampleResult[] samples) {
    PsdDepthSampleResult best;
    foreach (sample; samples) {
        if (!sample.valid || !sample.value.isFinite) continue;
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
    result.baseDepths.length = accum.best.length;
    result.winnerLayerPaths.length = accum.best.length;
    result.missingVertexMask.length = accum.best.length;
    result.layerMasks = accum.layerMasks.dup;
    if (!ngPsdDepthGridEnabled(settings, accum.grid.uuid)) {
        result.depths = existing;
        result.missingVertexMask[] = true;
        result.skipped = true;
        return;
    }
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
            result.depths[i] = ngFiniteDepthOrZero(accum.best[i]);
            result.baseDepths[i] = ngFiniteDepthOrZero(accum.baseBest[i]);
            result.winnerLayerPaths[i] = accum.winnerLayerPaths[i];
            result.missingVertexMask[i] = false;
            result.sampledVertices++;
            if (!hasMinMax || result.depths[i] < result.minDepth) result.minDepth = result.depths[i];
            if (!hasMinMax || result.depths[i] > result.maxDepth) result.maxDepth = result.depths[i];
            hasMinMax = true;
        } else {
            result.missingVertexMask[i] = true;
            result.missingVertices++;
            final switch (settings.missingPolicy) {
                case PsdDepthMissingPolicy.KeepExisting:
                    result.depths[i] = existing[i];
                    break;
                case PsdDepthMissingPolicy.SetZero:
                    result.depths[i] = 0.0f;
                    break;
                case PsdDepthMissingPolicy.SetBack:
                    result.depths[i] = ngFiniteDepthOrZero(settings.backDepth * settings.depthScale);
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
    ngNormalizeDepths(result.depths);
    ngNormalizeDepths(result.baseDepths);
}

private void buildCompositePreview(
    ref PsdDepthGridResult result,
    DepthLayerImage[] layers,
    Deformable grid,
    ref PsdDepthImportSettings settings
) {
    enum int MaxPreviewSize = 192;

    bool hasBounds;
    int left;
    int top;
    int right;
    int bottom;
    foreach (ref layer; layers) {
        if (layer.grid !is grid || layer.width <= 0 || layer.height <= 0) continue;
        auto layerRight = layer.left + layer.width;
        auto layerBottom = layer.top + layer.height;
        if (!hasBounds) {
            left = layer.left;
            top = layer.top;
            right = layerRight;
            bottom = layerBottom;
            hasBounds = true;
        } else {
            left = min(left, layer.left);
            top = min(top, layer.top);
            right = max(right, layerRight);
            bottom = max(bottom, layerBottom);
        }
    }
    if (!hasBounds || right <= left || bottom <= top) return;

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
    result.previewLeft = left;
    result.previewTop = top;
    result.previewWidth = width;
    result.previewHeight = height;
    result.rawCompositePreviewRgba.length = cast(size_t)width * cast(size_t)height * 4;
    result.compositePreviewRgba.length = cast(size_t)width * cast(size_t)height * 4;

    foreach (py; 0 .. height) {
        foreach (px; 0 .. width) {
            auto documentX = cast(float)left + (cast(float)px + 0.5f) / scale;
            auto documentY = cast(float)top + (cast(float)py + 0.5f) / scale;
            bool hasRawSample;
            float rawBestDepth = 0.0f;
            float rawBestGray = 0.0f;
            ubyte rawBestAlpha = 0;
            bool hasSample;
            float bestDepth = 0.0f;
            float bestGray = 0.0f;
            ubyte bestAlpha = 0;

            foreach (ref layer; layers) {
                if (layer.grid !is grid) continue;
                if (!ngPsdDepthGridLayerEnabled(settings, grid.uuid, layer.layerPath)) continue;
                auto layerX = cast(int)round(layerToSourceX(layer, documentX));
                auto layerY = cast(int)round(layerToSourceY(layer, documentY));
                if (layerX < 0 || layerY < 0 || layerX >= layer.width || layerY >= layer.height) continue;
                auto index = (cast(size_t)layerY * cast(size_t)layer.width + cast(size_t)layerX) * 4;
                auto rawAlpha = surfaceAlpha(layer, index, layerX, layerY);
                if (ngDepthSampleAcceptsAlpha(rawAlpha, settings.alphaThreshold)) {
                    auto sampleSettings = settings;
                    auto rawDepth = pixelDepth(layer.data, index, sampleSettings);
                    if (!hasRawSample || rawDepth > rawBestDepth) {
                        hasRawSample = true;
                        rawBestDepth = rawDepth;
                        rawBestGray = pixelDepth01(layer.data, index, sampleSettings);
                        rawBestAlpha = ngDepthSampleAlphaByte(rawAlpha);
                    }
                }
                if (!coverageReliable(layer, layerX, layerY)) continue;
                auto alpha = surfaceAlpha(layer, index, layerX, layerY);
                if (!ngDepthSampleAcceptsAlpha(alpha, settings.alphaThreshold)) continue;
                auto sampleSettings = settings;
                auto depth = pixelDepth(layer.data, index, sampleSettings);
                if (!hasSample || depth > bestDepth) {
                    hasSample = true;
                    bestDepth = depth;
                    bestGray = pixelDepth01(layer.data, index, sampleSettings);
                    bestAlpha = ngDepthSampleAlphaByte(alpha);
                }
            }

            auto outIndex = (cast(size_t)py * cast(size_t)width + cast(size_t)px) * 4;
            if (!hasRawSample) {
                result.rawCompositePreviewRgba[outIndex + 0] = 0;
                result.rawCompositePreviewRgba[outIndex + 1] = 0;
                result.rawCompositePreviewRgba[outIndex + 2] = 0;
                result.rawCompositePreviewRgba[outIndex + 3] = 0;
            } else {
                auto gray = cast(ubyte)round(rawBestGray * 255.0f);
                result.rawCompositePreviewRgba[outIndex + 0] = gray;
                result.rawCompositePreviewRgba[outIndex + 1] = gray;
                result.rawCompositePreviewRgba[outIndex + 2] = gray;
                result.rawCompositePreviewRgba[outIndex + 3] = rawBestAlpha;
            }
            if (!hasSample) {
                result.compositePreviewRgba[outIndex + 0] = 0;
                result.compositePreviewRgba[outIndex + 1] = 0;
                result.compositePreviewRgba[outIndex + 2] = 0;
                result.compositePreviewRgba[outIndex + 3] = 0;
            } else {
                auto gray = cast(ubyte)round(bestGray * 255.0f);
                result.compositePreviewRgba[outIndex + 0] = gray;
                result.compositePreviewRgba[outIndex + 1] = gray;
                result.compositePreviewRgba[outIndex + 2] = gray;
                result.compositePreviewRgba[outIndex + 3] = bestAlpha;
            }
        }
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
    settings.zeroDepthIsMissing = true;

    File file = File(path);
    scope(exit) file.close();
    auto document = parseDocument(file);
    scope(exit) destroy(document);
    bool hasExplicitColorSource = settings.colorSourcePath.length > 0;
    bool hasFlatColorSource = hasExplicitColorSource && settings.colorSourcePath.extension.toLower == ".png";
    bool hasPsdColorSource = hasExplicitColorSource && settings.colorSourcePath.extension.toLower == ".psd";
    ShallowTexture flatColorTexture;
    string flatColorLayerName;
    string flatColorLayerPath;
    if (hasFlatColorSource) {
        flatColorTexture = ShallowTexture(settings.colorSourcePath, 4);
        enforce(flatColorTexture.width == document.width && flatColorTexture.height == document.height,
            "Color and depth source dimensions must match for PSD depth composition");
        flatColorLayerName = settings.colorSourcePath.baseName.stripExtension;
        flatColorLayerPath = "/" ~ flatColorLayerName;
    } else if (hasExplicitColorSource && !hasPsdColorSource) {
        enforce(false, "Unsupported color source format for PSD depth composition");
    }

    PsdDepthImportResult result;
    result.gpuCompositionRequested = settings.useGpuComposition;
    result.smoothWavySurface = settings.smoothWavySurface;
    setCompositionMode(result, PsdDepthCompositionMode.NToN);
    result.compositionWidth = document.width;
    result.compositionHeight = document.height;
    result.globalDepthScale = settings.depthScale;
    result.missingPolicy = settings.missingPolicy;
    result.missingBackDepth = settings.backDepth;
    result.depthSource.kind = PsdDepthCompositeSourceKind.PsdLayers;
    result.depthSource.kindName = ngPsdDepthCompositeSourceKindName(result.depthSource.kind);
    result.depthSource.sourcePath = path;
    result.depthSource.width = document.width;
    result.depthSource.height = document.height;
    result.colorSource.kind = hasPsdColorSource ? PsdDepthCompositeSourceKind.PsdLayers :
        (hasFlatColorSource ? PsdDepthCompositeSourceKind.FlatImage : PsdDepthCompositeSourceKind.ActiveArtTargets);
    result.colorSource.kindName = ngPsdDepthCompositeSourceKindName(result.colorSource.kind);
    result.colorSource.sourcePath = hasExplicitColorSource ? settings.colorSourcePath : null;
    result.colorSource.width = document.width;
    result.colorSource.height = document.height;
    if (hasFlatColorSource) {
        result.colorLayerCount = 1;
        auto colorSourceLayer = makeCompositeSourceLayer(
            flatColorLayerPath,
            flatColorLayerName,
            0,
            0,
            flatColorTexture.width,
            flatColorTexture.height,
            true,
            true,
            0
        );
        setCompositeSourceLayerPixels(colorSourceLayer, flatColorTexture.data);
        result.colorSource.layers ~= colorSourceLayer;
    } else if (hasPsdColorSource) {
        loadPsdCompositeSourceLayers(settings.colorSourcePath, result.colorSource, result.colorLayerCount);
        enforce(result.colorSource.width == document.width && result.colorSource.height == document.height,
            "Color and depth source dimensions must match for PSD depth composition");
    }
    DepthLayerImage[] layers;

    auto groupStates = ngPsdLayerGroupStates(document.layers);
    size_t[string] layerPathOccurrences;
    PsdClippingBaseState[string] clippingBaseByGroup;
    foreach_reverse (i, layer; document.layers) {
        if (layer.type != LayerType.Any) continue;
        auto groupState = groupStates[i];

        result.sourceDepthLayerCount++;

        auto layerPath = uniquePsdLayerPath("%s/%s".format(groupState.path, layer.name), layerPathOccurrences);
        auto layerVisible = groupState.visible && psdLayerVisible(layer);
        auto effectiveLayerOpacity = groupState.opacity * layerOpacity01(layer.opacity);
        PsdDepthLayerMapping mapping;
        mapping.layerPath = layerPath;
        mapping.layerName = layer.name;

        layer.extractLayerImage();
        bool hasLayerImage = layer.data.length > 0;
        DepthLayerImage image;
        if (hasLayerImage) {
            image.layerPath = layerPath;
            image.layerName = layer.name;
            image.left = layer.left;
            image.top = layer.top;
            image.width = layer.width;
            image.height = layer.height;
            image.documentWidth = document.width;
            image.documentHeight = document.height;
            image.opacity = effectiveLayerOpacity;
            image.data = layer.data.dup;
            if (layer.clipping) {
                clippingBaseByGroup[groupState.path] = psdClippingBaseState(
                    image, layerVisible, effectiveLayerOpacity);
            } else if (auto clippingBase = groupState.path in clippingBaseByGroup) {
                layerVisible = layerVisible && clippingBase.visible;
                auto baseLocalOpacity = groupState.opacity > 0.0f
                    ? clippingBase.opacity / groupState.opacity
                    : 0.0f;
                effectiveLayerOpacity *= max(0.0f, min(1.0f, baseLocalOpacity));
                image.opacity = effectiveLayerOpacity;
                applyPsdClippingBaseAlpha(image, *clippingBase);
            }
            if (hasPsdColorSource) {
                image.coverageWidth = image.width;
                image.coverageHeight = image.height;
                image.coverageChannels = 4;
                image.coverageOpacity = 1.0f;
                image.coverageData.length = cast(size_t)image.width * cast(size_t)image.height * 4;
                auto depthCandidate = makeCompositeSourceLayer(
                    layerPath,
                    layer.name,
                    layer.left,
                    layer.top,
                    layer.width,
                    layer.height,
                    layerVisible,
                    true,
                    0
                );
                auto colorIndex = bestMatchingColorSourceLayer(depthCandidate, result.colorSource.layers);
                if (colorIndex >= 0) {
                    attachCompositeSourceCoverage(image, result.colorSource.layers[cast(size_t)colorIndex]);
                }
            } else if (hasFlatColorSource) {
                image.coverageWidth = flatColorTexture.width;
                image.coverageHeight = flatColorTexture.height;
                image.coverageChannels = flatColorTexture.channels;
                image.coverageOpacity = 1.0f;
                image.coverageData = flatColorTexture.data.dup;
            }
            auto depthSourceLayer = makeCompositeSourceLayer(
                layerPath,
                layer.name,
                layer.left,
                layer.top,
                layer.width,
                layer.height,
                layerVisible,
                true,
                computeLayerDepthStats(image, settings).maskedPixels
            );
            depthSourceLayer.opacity = effectiveLayerOpacity;
            setCompositeSourceLayerPixels(depthSourceLayer, image.data, buildDepthMaskPreview(image, settings));
            result.depthSource.layers ~= depthSourceLayer;
        }

        Deformable grid;
        Node matchedNode;
        if (auto ignored = layerPath in settings.ignoredLayerPaths) {
            if (*ignored) {
                mapping.ignored = true;
                mapping.manual = true;
                mapping.status = "Ignored";
                result.mappings ~= mapping;
                if (hasLayerImage) {
                    addComposedLayer(result, path, image, settings, layerVisible, false);
                }
                layer.data = null;
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
                if (hasLayerImage) {
                    addComposedLayer(result, path, image, settings, layerVisible, true);
                }
                addCompositionDiagnostic(result, "missing-manual-target",
                    "Manual target binding points to a missing GridDeformer or PathDeformer.", layerPath, layer.name);
                layer.data = null;
                continue;
            }
            matchedNode = grid;
            mapping.matched = true;
            mapping.matchedNodeName = grid.name;
            mapping.matchedNodeUuid = grid.uuid;
            mapping.targetGridName = grid.name;
            mapping.targetGridUuid = grid.uuid;
            mapping.status = "Manual";
        } else {
            auto candidates = matchCandidates(puppet, layerPath, layer.name, settings,
                layer.left, layer.top, layer.width, layer.height, document.width, document.height, true);
            if (candidates.length == 0) {
                mapping.status = "Unmatched";
                result.unmatchedLayers++;
                result.mappings ~= mapping;
                if (hasLayerImage) {
                    addComposedLayer(result, path, image, settings, layerVisible, true);
                }
                addCompositionDiagnostic(result, "unused-depth-layer",
                    "Depth layer did not match any color/target layer.", layerPath, layer.name);
                layer.data = null;
                continue;
            }

            auto candidate = candidates[0];
            matchedNode = candidate.node;
            grid = containingDepthTarget(candidate.node);
            if (grid is null) {
                if (auto part = cast(Part)candidate.node) grid = activeArtDepthTarget(puppet, part);
            }
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
                if (hasLayerImage) {
                    addComposedLayer(result, path, image, settings, layerVisible, true);
                }
                addCompositionDiagnostic(result, "unused-depth-layer",
                    "Depth layer matched a node that is not inside a depth target.", layerPath, layer.name);
                layer.data = null;
                continue;
            }

            mapping.matched = true;
            mapping.targetGridName = grid.name;
            mapping.targetGridUuid = grid.uuid;
        }

        result.matchedLayers++;
        result.mappings ~= mapping;

        if (!hasLayerImage) continue;
        image.grid = grid;
        if (!hasExplicitColorSource) attachMatchedCoverage(image, puppet, matchedNode, grid);
        buildCoverageCache(image);
        addComposedLayer(result, path, image, settings, layerVisible,
            ngPsdDepthGridLayerEnabled(settings, grid.uuid, layerPath), grid);

        if (layerVisible) layers ~= image;
        layer.data = null;
    }

    GridAccum[] accums;
    foreach (ref layer; layers) {
        auto accumIndex = findAccum(accums, layer.grid);
        auto layerMaskIndex = findLayerMask(accums[accumIndex], layer.layerPath, layer.layerName);
        if (!ngPsdDepthGridEnabled(settings, layer.grid.uuid)) continue;
        if (!ngPsdDepthGridLayerEnabled(settings, layer.grid.uuid, layer.layerPath)) continue;
        auto vertices = layer.grid.vertices;
        foreach (i; 0 .. vertices.length) {
            auto documentPoint = gridVertexDocumentPosition(layer.grid, vertices[i], document.width, document.height);
            auto layerX = layerToSourceX(layer, documentPoint.x);
            auto layerY = layerToSourceY(layer, documentPoint.y);
            auto sample = sampleLayer(layer, layerX, layerY, settings);
            if (!sample.valid) continue;
            accums[accumIndex].layerMasks[layerMaskIndex].sampledVertices++;
            if (!accums[accumIndex].has[i] || sample.value > accums[accumIndex].best[i]) {
                accums[accumIndex].has[i] = true;
                accums[accumIndex].best[i] = sample.value;
                accums[accumIndex].baseBest[i] = sample.value;
                accums[accumIndex].winnerLayerPaths[i] = layer.layerPath;
            }
        }
    }

    foreach (ref accum; accums) {
        PsdDepthGridResult gridResult;
        gridResult.documentWidth = document.width;
        gridResult.documentHeight = document.height;
        finalizeGridResult(gridResult, accum, settings);
        foreach (ref layer; layers) {
            if (layer.grid is accum.grid) gridResult.coverageSources += layer.coverageSources.length;
        }
        buildCompositePreview(gridResult, layers, accum.grid, settings);
        if (gridResult.skipped) result.skippedGrids++;
        result.grids ~= gridResult;
    }

    if (hasPsdColorSource) {
        bool[] matchedDepthSourceLayers;
        matchedDepthSourceLayers.length = result.depthSource.layers.length;
        foreach (colorLayer; result.colorSource.layers) {
            auto depthIndex = bestMatchingDepthSourceLayer(colorLayer, result.depthSource.layers);
            if (depthIndex < 0) {
                PsdDepthComposedLayer missingLayer;
                missingLayer.id = colorLayer.id;
                missingLayer.layerPath = colorLayer.id;
                missingLayer.layerName = colorLayer.name;
                missingLayer.colorLayerPath = colorLayer.id;
                missingLayer.colorLayerName = colorLayer.name;
                missingLayer.left = colorLayer.left;
                missingLayer.top = colorLayer.top;
                missingLayer.width = colorLayer.width;
                missingLayer.height = colorLayer.height;
                missingLayer.visible = colorLayer.visible;
                missingLayer.enabled = false;
                missingLayer.puppetFitEnabled = true;
                result.composedLayers ~= missingLayer;
                result.composedLayerCount = result.composedLayers.length;
                addCompositionDiagnostic(result, "missing-depth-layer",
                    "PSD color layer has no matched PSD depth layer.", colorLayer.id, colorLayer.name);
                continue;
            }

            matchedDepthSourceLayers[cast(size_t)depthIndex] = true;
            auto depthLayer = result.depthSource.layers[cast(size_t)depthIndex];
            foreach (ref composedLayer; result.composedLayers) {
                if (composedLayer.layerPath == depthLayer.id) {
                    composedLayer.colorLayerPath = colorLayer.id;
                    composedLayer.colorLayerName = colorLayer.name;
                    break;
                }
            }
        }
        foreach (i, matched; matchedDepthSourceLayers) {
            if (matched) continue;
            auto depthLayer = result.depthSource.layers[i];
            addCompositionDiagnostic(result, "unused-depth-layer",
                "PSD depth layer did not match any PSD color layer.", depthLayer.id, depthLayer.name);
        }
    }

    bool[string] accountedLayerPaths;
    foreach (mapping; result.mappings) {
        if (mapping.layerPath.length && ngPsdDepthMappingAccountsForColorLayer(mapping)) {
            accountedLayerPaths[mapping.layerPath] = true;
        }
    }
    if (!hasExplicitColorSource) {
        bool[string] usedLayerPaths;
        foreach (part; puppet.findNodesType!Part(puppet.root)) {
            if (part.textures.length == 0 || part.textures[0] is null) continue;
            auto target = activeArtDepthTarget(puppet, part);
            if (target is null) continue;
            auto layerName = activeArtLayerName(part);
            auto layerPath = uniqueActiveArtLayerPath(part, usedLayerPaths);
            if (layerPath.length == 0) continue;
            DepthLayerImage image;
            image.layerPath = layerPath;
            image.layerName = layerName;
            image.left = 0;
            image.top = 0;
            image.width = result.compositionWidth;
            image.height = result.compositionHeight;
            image.documentWidth = result.compositionWidth;
            image.documentHeight = result.compositionHeight;
            attachPartCoverage(image, part);
            tightenDepthLayerImageToCoverageBounds(image);
            result.colorLayerCount++;
            result.colorSource.layers ~= makeCompositeSourceLayer(
                layerPath,
                layerName,
                image.left,
                image.top,
                image.width,
                image.height,
                true,
                true,
                0,
                target
            );
            if (layerPath in accountedLayerPaths) continue;
            addMissingComposedLayer(result, layerPath, layerName, image.left, image.top, image.width, image.height, target);
            addCompositionDiagnostic(result, "missing-depth-layer",
                "Color/art layer has no matched depth layer.", layerPath, layerName);
        }
    }
    auto explicitColorLayerCount = hasExplicitColorSource ? result.colorLayerCount : settings.explicitColorLayerCount;
    if (explicitColorLayerCount > 0) {
        result.colorLayerCount = explicitColorLayerCount;
        auto explicitMode = ngPsdDepthCompositionModeForCounts(explicitColorLayerCount, result.sourceDepthLayerCount);
        if (explicitMode == PsdDepthCompositionMode.UnsupportedOneToN) {
            setCompositionMode(result, explicitMode);
            result.grids = null;
            addCompositionDiagnostic(result, "unsupported-1:N",
                "A single color layer cannot be composed with multiple depth layers without a split policy.");
        } else if (hasFlatColorSource) {
            setCompositionMode(result, explicitMode);
        }
    }

    validateCompositionDimensions(result, "PSD depth composition");
    return result;
}

PsdDepthImportResult ngBuildPsdDepthsFromImage(Puppet puppet, string path, PsdDepthImportSettings settings) {
    enforce(puppet !is null && puppet.root !is null, "No active puppet");
    enforce(path.length > 0, "Path not provided");
    enforce(settings.backDepth.isFinite && settings.frontDepth.isFinite, "Depth range must be finite");
    enforce(settings.depthScale.isFinite, "Depth scale must be finite");
    enforce(settings.depthScale >= 0.0f, "Depth scale must be non-negative");
    enforce(settings.alphaThreshold >= 0.0f && settings.alphaThreshold <= 1.0f, "Alpha threshold must be in [0, 1]");
    enforce(settings.customRadius >= 1 && settings.customRadius <= 64, "Custom radius must be in [1, 64]");
    settings.zeroDepthIsMissing = true;

    auto texture = ShallowTexture(path, 4);
    auto layerName = path.baseName.stripExtension;
    auto layerPath = "/" ~ layerName;
    bool hasExplicitColorSource = settings.colorSourcePath.length > 0;
    bool hasFlatColorSource = hasExplicitColorSource && settings.colorSourcePath.extension.toLower == ".png";
    bool hasPsdColorSource = hasExplicitColorSource && settings.colorSourcePath.extension.toLower == ".psd";
    ShallowTexture flatColorTexture;
    string flatColorLayerName;
    string flatColorLayerPath;
    if (hasFlatColorSource) {
        flatColorTexture = ShallowTexture(settings.colorSourcePath, 4);
        enforce(flatColorTexture.width == texture.width && flatColorTexture.height == texture.height,
            "Color and depth source dimensions must match for 1:1 composition");
        flatColorLayerName = settings.colorSourcePath.baseName.stripExtension;
        flatColorLayerPath = "/" ~ flatColorLayerName;
    } else if (hasExplicitColorSource && !hasPsdColorSource) {
        enforce(false, "Unsupported color source format for flat depth composition");
    }

    PsdDepthImportResult result;
    result.gpuCompositionRequested = settings.useGpuComposition;
    result.smoothWavySurface = settings.smoothWavySurface;
    setCompositionMode(result, PsdDepthCompositionMode.Unknown);
    result.compositionWidth = texture.width;
    result.compositionHeight = texture.height;
    result.globalDepthScale = settings.depthScale;
    result.missingPolicy = settings.missingPolicy;
    result.missingBackDepth = settings.backDepth;
    result.sourceDepthLayerCount = 1;
    result.depthSource.kind = PsdDepthCompositeSourceKind.FlatImage;
    result.depthSource.kindName = ngPsdDepthCompositeSourceKindName(result.depthSource.kind);
    result.depthSource.sourcePath = path;
    result.depthSource.width = texture.width;
    result.depthSource.height = texture.height;
    auto pngDepthSourceLayer = makeCompositeSourceLayer(
        layerPath,
        layerName,
        0,
        0,
        texture.width,
        texture.height,
        true,
        true,
        0
    );
    auto normalizedFlatDepthRgba = normalizeFlatDepthRgba(texture.data, texture.width, texture.height, texture.channels);
    setCompositeSourceLayerPixels(pngDepthSourceLayer, normalizedFlatDepthRgba);
    result.depthSource.layers ~= pngDepthSourceLayer;
    result.colorSource.kind = hasPsdColorSource ? PsdDepthCompositeSourceKind.PsdLayers :
        (hasFlatColorSource ? PsdDepthCompositeSourceKind.FlatImage : PsdDepthCompositeSourceKind.ActiveArtTargets);
    result.colorSource.kindName = ngPsdDepthCompositeSourceKindName(result.colorSource.kind);
    result.colorSource.sourcePath = hasExplicitColorSource ? settings.colorSourcePath : null;
    result.colorSource.width = texture.width;
    result.colorSource.height = texture.height;

    PsdDepthImportResult finishResult() {
        validateCompositionDimensions(result, hasFlatColorSource ? "1:1 composition" :
            (hasPsdColorSource ? "N:1 composition" : "active color/depth composition"));
        return result;
    }

    if (hasFlatColorSource) {
        result.colorLayerCount = 1;
        setCompositionMode(result, PsdDepthCompositionMode.OneToOne);
        auto colorSourceLayer = makeCompositeSourceLayer(
            flatColorLayerPath,
            flatColorLayerName,
            0,
            0,
            flatColorTexture.width,
            flatColorTexture.height,
            true,
            true,
            0
        );
        setCompositeSourceLayerPixels(colorSourceLayer, flatColorTexture.data);
        result.colorSource.layers ~= colorSourceLayer;
    } else if (hasPsdColorSource) {
        loadPsdCompositeSourceLayers(settings.colorSourcePath, result.colorSource, result.colorLayerCount);
        enforce(result.colorSource.width == texture.width && result.colorSource.height == texture.height,
            "Color and depth source dimensions must match for N:1 composition");
        setCompositionMode(result, ngPsdDepthCompositionModeForCounts(result.colorLayerCount, result.sourceDepthLayerCount));
    }
    PsdDepthLayerMapping mapping;
    mapping.layerPath = layerPath;
    mapping.layerName = layerName;

    DepthLayerImage basePngImage(string imageLayerPath = null, string imageLayerName = null) {
        DepthLayerImage image;
        image.layerPath = imageLayerPath.length ? imageLayerPath : (hasFlatColorSource ? flatColorLayerPath : layerPath);
        image.layerName = imageLayerName.length ? imageLayerName : (hasFlatColorSource ? flatColorLayerName : layerName);
        image.left = 0;
        image.top = 0;
        image.width = texture.width;
        image.height = texture.height;
        image.documentWidth = texture.width;
        image.documentHeight = texture.height;
        image.opacity = 1.0f;
        image.data = normalizedFlatDepthRgba.dup;
        if (hasFlatColorSource) {
            image.coverageWidth = flatColorTexture.width;
            image.coverageHeight = flatColorTexture.height;
            image.coverageChannels = flatColorTexture.channels;
            image.coverageOpacity = 1.0f;
            image.coverageData = flatColorTexture.data.dup;
        }
        return image;
    }

    Deformable grid;
    Node matchedNode;
    if (auto ignored = layerPath in settings.ignoredLayerPaths) {
        if (*ignored) {
            mapping.ignored = true;
            mapping.manual = true;
            mapping.status = "Ignored";
            result.mappings ~= mapping;
            auto image = basePngImage();
            addComposedLayer(result, path, image, settings, true, false);
            return finishResult();
        }
    }

    void addMappedPngLayer(ref DepthLayerImage image, Deformable target, Node matchedTarget,
        string status, bool manual, ref DepthLayerImage[] layers) {
        PsdDepthLayerMapping targetMapping;
        targetMapping.layerPath = image.layerPath;
        targetMapping.layerName = image.layerName;
        targetMapping.manual = manual;
        targetMapping.matched = true;
        targetMapping.matchedNodeName = matchedTarget.name;
        targetMapping.matchedNodeUuid = matchedTarget.uuid;
        targetMapping.targetGridName = target.name;
        targetMapping.targetGridUuid = target.uuid;
        targetMapping.status = status;
        result.matchedLayers++;
        result.mappings ~= targetMapping;

        image.grid = target;
        if (!hasPsdColorSource && (status == "ComposedN1" || status == "Manual")) {
            bool existsColorLayer;
            foreach (sourceLayer; result.colorSource.layers) {
                if (sourceLayer.targetGridUuid == target.uuid && sourceLayer.id == image.layerPath) {
                    existsColorLayer = true;
                    break;
                }
            }
            if (!existsColorLayer) {
                auto colorSourceLayer = makeCompositeSourceLayer(
                    image.layerPath,
                    image.layerName,
                    image.left,
                    image.top,
                    image.width,
                    image.height,
                    true,
                    image.coverageSources.length > 0 || image.coverageData.length > 0,
                    0,
                    target
                );
                auto colorMask = image.coverageData.length ? image.coverageData : image.data;
                setCompositeSourceLayerPixels(colorSourceLayer, colorMask);
                result.colorSource.layers ~= colorSourceLayer;
            }
        }
        addComposedLayer(result, path, image, settings, true,
            ngPsdDepthGridLayerEnabled(settings, target.uuid, image.layerPath), target);

        layers ~= image;
    }

    void applyDepthDrawSplitToPngBindings(ref PngComposedBinding[] bindings) {
        sort!((a, b) {
            auto drawableA = cast(Drawable)a.matchedTarget;
            auto drawableB = cast(Drawable)b.matchedTarget;
            auto zA = drawableA is null ? (a.target is null ? 0.0f : a.target.zSort) : drawableA.zSort;
            auto zB = drawableB is null ? (b.target is null ? 0.0f : b.target.zSort) : drawableB.zSort;
            if (zA == zB) return a.image.layerPath < b.image.layerPath;
            // depth-draw consumes layers back-to-front: larger indexes are
            // upper layers. nijigenerate's PSD/import ordering uses smaller
            // zSort values for the front, so pass larger values first.
            return zA > zB;
        })(bindings);

        DepthDrawSplitLayer[] splitLayers;
        foreach (ref binding; bindings) {
            if (binding.image.width <= 0 || binding.image.height <= 0) continue;
            splitLayers ~= depthDrawSplitLayerFromDepthLayerImage(binding.image);
        }
        if (splitLayers.length == 0) return;

        auto yFlippedFlatDepthRgba = flipRgbaY(normalizedFlatDepthRgba, texture.width, texture.height);
        size_t normalCropOverlap;
        size_t flippedCropOverlap;
        foreach (ref binding; bindings) {
            if (binding.image.width <= 0 || binding.image.height <= 0) continue;
            auto surfacePreview = buildColorLayerPreviewRgba(binding.image);
            auto normalCrop = sampleDepthToSurfaceLayer(
                normalizedFlatDepthRgba,
                texture.width,
                texture.height,
                binding.image.left,
                binding.image.top,
                binding.image.width,
                binding.image.height
            );
            auto flippedCrop = sampleDepthToSurfaceLayer(
                yFlippedFlatDepthRgba,
                texture.width,
                texture.height,
                binding.image.left,
                binding.image.top,
                binding.image.width,
                binding.image.height
            );
            normalCropOverlap += depthSurfaceOverlap(normalCrop, surfacePreview.rgba);
            flippedCropOverlap += depthSurfaceOverlap(flippedCrop, surfacePreview.rgba);
        }
        auto splitDepthRgba = flippedCropOverlap > normalCropOverlap ? yFlippedFlatDepthRgba : normalizedFlatDepthRgba;
        auto stableDepthPixels = ngDepthDrawDecodeDepthPixelsFromRgba(
            splitDepthRgba, sampleChannel(settings.channel));
        auto visibleLayerMap = ngDepthDrawBuildVisibleLayerMap(texture.width, texture.height, splitLayers);
        int splitIndex;
        foreach (ref binding; bindings) {
            if (binding.image.width <= 0 || binding.image.height <= 0) continue;
            auto splitLayer = depthDrawSplitLayerFromDepthLayerImage(binding.image);
            seedLayerDepthFromDepthDrawSplit(
                binding.image,
                splitLayer,
                splitIndex,
                texture.width,
                texture.height,
                stableDepthPixels,
                visibleLayerMap,
                splitLayers,
                settings.repairContourBand
            );
            splitIndex++;
            buildCoverageCache(binding.image);
            maskDepthToCoverage(binding.image, settings);
            applyDepthDrawLayerDepthCleanup(binding.image, settings);
        }
    }

    void addMappedPngGridResults(ref DepthLayerImage[] layers) {
        GridAccum[] accums;
        foreach (ref layer; layers) {
            if (layer.grid is null) continue;
            auto accumIndex = findAccum(accums, layer.grid);
            auto layerMaskIndex = findLayerMask(accums[accumIndex], layer.layerPath, layer.layerName);
            if (!ngPsdDepthGridEnabled(settings, layer.grid.uuid)) continue;
            if (!ngPsdDepthGridLayerEnabled(settings, layer.grid.uuid, layer.layerPath)) continue;
            auto vertices = layer.grid.vertices;
            foreach (i; 0 .. vertices.length) {
                auto documentPoint = gridVertexDocumentPosition(layer.grid, vertices[i], texture.width, texture.height);
                auto layerX = layerToSourceX(layer, documentPoint.x);
                auto layerY = layerToSourceY(layer, documentPoint.y);
                auto sample = sampleLayer(layer, layerX, layerY, settings);
                if (!sample.valid) continue;
                accums[accumIndex].layerMasks[layerMaskIndex].sampledVertices++;
                if (!accums[accumIndex].has[i] || sample.value > accums[accumIndex].best[i]) {
                    accums[accumIndex].has[i] = true;
                    accums[accumIndex].best[i] = sample.value;
                    accums[accumIndex].baseBest[i] = sample.value;
                    accums[accumIndex].winnerLayerPaths[i] = layer.layerPath;
                }
            }
        }

        foreach (ref accum; accums) {
            PsdDepthGridResult gridResult;
            gridResult.documentWidth = texture.width;
            gridResult.documentHeight = texture.height;
            finalizeGridResult(gridResult, accum, settings);
            foreach (ref layer; layers) {
                if (layer.grid is accum.grid) gridResult.coverageSources += layer.coverageSources.length;
            }
            buildCompositePreview(gridResult, layers, accum.grid, settings);
            if (gridResult.skipped) result.skippedGrids++;
            result.grids ~= gridResult;
        }
    }

    void addMappedPngTarget(ref DepthLayerImage image, Deformable target, Node matchedTarget, string status, bool manual) {
        DepthLayerImage[] layers;
        addMappedPngLayer(image, target, matchedTarget, status, manual, layers);
        addMappedPngGridResults(layers);
    }

    bool addDirectMatchedPngTarget(Deformable target, Node matchedTarget, string status, bool manual) {
        setCompositionMode(result, PsdDepthCompositionMode.OneToOne);
        result.colorLayerCount = max(result.colorLayerCount, 1);
        auto image = basePngImage();
        image.grid = target;
        attachMatchedCoverage(image, puppet, matchedTarget, target);
        if (!hasFlatColorSource) {
            setDepthLayerImageToCoverageDocumentBounds(image);
            compositeFlatDepthByCoverageSources(image, normalizedFlatDepthRgba, texture.width, texture.height);
        }
        buildCoverageCache(image);
        maskDepthToCoverage(image, settings);
        applyDepthDrawLayerDepthCleanup(image, settings);
        addMappedPngTarget(image, target, matchedTarget, status, manual);
        return true;
    }

    bool collectComposedTargetFromFlatDepth(ref PngComposedBinding[] bindings, Deformable target) {
        if (target is null) return false;
        if (!targetHasArtCoverage(puppet, target)) return false;
        auto composedLayerName = target.name.length ? target.name : layerName;
        auto composedLayerPath = "/" ~ composedLayerName;
        if (auto overrideUuid = composedLayerPath in settings.layerTargetGridUuidOverrides) {
            auto manualGrid = findGridByUuid(puppet, *overrideUuid);
            if (manualGrid is null) return false;
            result.colorLayerCount++;
            setCompositionMode(result, PsdDepthCompositionMode.NToOne);
            auto image = basePngImage(composedLayerPath, composedLayerName);
            image.grid = manualGrid;
            attachMatchedCoverage(image, puppet, target, target);
            setDepthLayerImageToCoverageDocumentBounds(image);
            compositeFlatDepthByCoverageSources(image, normalizedFlatDepthRgba, texture.width, texture.height);
            buildCoverageCache(image);
            maskDepthToCoverage(image, settings);
            applyDepthDrawLayerDepthCleanup(image, settings);
            bindings ~= PngComposedBinding(image, manualGrid, manualGrid, "Manual", true);
            return true;
        }
        auto image = basePngImage(composedLayerPath, composedLayerName);
        image.grid = target;
        attachMatchedCoverage(image, puppet, target, target);
        if (image.coverageSources.length == 0 && image.coverageData.length == 0) return false;
        result.colorLayerCount++;
        setCompositionMode(result, PsdDepthCompositionMode.NToOne);
        setDepthLayerImageToCoverageDocumentBounds(image);
        compositeFlatDepthByCoverageSources(image, normalizedFlatDepthRgba, texture.width, texture.height);
        buildCoverageCache(image);
        maskDepthToCoverage(image, settings);
        applyDepthDrawLayerDepthCleanup(image, settings);
        bindings ~= PngComposedBinding(image, target, target, "ComposedN1", false);
        return true;
    }

    bool addComposedTargetsFromFlatDepth() {
        PngComposedBinding[] bindings;
        bool[string] usedLayerPaths;
        foreach (part; puppet.findNodesType!Part(puppet.root)) {
            if (part.textures.length == 0 || part.textures[0] is null) continue;
            auto target = activeArtDepthTarget(puppet, part);
            if (target is null) continue;
            auto composedLayerName = activeArtLayerName(part);
            auto composedLayerPath = uniqueActiveArtLayerPath(part, usedLayerPaths);
            if (composedLayerPath.length == 0) continue;

            bool manual;
            Node matchedTarget = part;
            if (auto overrideUuid = composedLayerPath in settings.layerTargetGridUuidOverrides) {
                auto manualGrid = findGridByUuid(puppet, *overrideUuid);
                if (manualGrid is null) continue;
                target = manualGrid;
                matchedTarget = manualGrid;
                manual = true;
            }

            auto image = basePngImage(composedLayerPath, composedLayerName);
            image.grid = target;
            attachPartCoverage(image, part);
            if (image.coverageSources.length == 0 && image.coverageData.length == 0) continue;
            result.colorLayerCount++;
            setCompositionMode(result, PsdDepthCompositionMode.NToOne);
            setDepthLayerImageToCoverageDocumentBounds(image);
            compositeFlatDepthByCoverageSources(image, normalizedFlatDepthRgba, texture.width, texture.height);
            buildCoverageCache(image);
            maskDepthToCoverage(image, settings);
            applyDepthDrawLayerDepthCleanup(image, settings);
            bindings ~= PngComposedBinding(image, target, matchedTarget, manual ? "Manual" : "ComposedN1", manual);
        }

        if (bindings.length == 0) {
            foreach (target; puppet.findNodesType!GridDeformer(puppet.root)) {
                collectComposedTargetFromFlatDepth(bindings, target);
            }
            foreach (target; puppet.findNodesType!PathDeformer(puppet.root)) {
                collectComposedTargetFromFlatDepth(bindings, target);
            }
        }
        if (bindings.length == 0) return false;

        applyDepthDrawSplitToPngBindings(bindings);

        DepthLayerImage[] composedImages;
        foreach (binding; bindings) composedImages ~= binding.image;
        DepthLayerImage[] layers;
        foreach (i, ref binding; bindings) {
            binding.image = composedImages[i];
            addMappedPngLayer(binding.image, binding.target, binding.matchedTarget, binding.status, binding.manual, layers);
        }
        addMappedPngGridResults(layers);
        return true;
    }

    DepthLayerImage imageFromColorSourceLayer(PsdDepthCompositeSourceLayer sourceLayer) {
        DepthLayerImage image;
        image.layerPath = sourceLayer.id;
        image.layerName = sourceLayer.name;
        image.left = sourceLayer.left;
        image.top = sourceLayer.top;
        image.width = sourceLayer.width;
        image.height = sourceLayer.height;
        image.documentWidth = texture.width;
        image.documentHeight = texture.height;
        image.opacity = 1.0f;
        image.data = normalizeFlatDepthRgba(
            cropRgbaRect(texture.data, texture.width, texture.height,
                sourceLayer.left, sourceLayer.top, sourceLayer.width, sourceLayer.height),
            sourceLayer.width,
            sourceLayer.height
        );
        image.coverageWidth = sourceLayer.width;
        image.coverageHeight = sourceLayer.height;
        image.coverageChannels = 4;
        image.coverageOpacity = sourceLayer.opacity;
        image.coverageData = sourceLayer.rgba.dup;
        return image;
    }

    bool addPsdColorComposedTargetsFromFlatDepth() {
        if (!hasPsdColorSource) return false;
        if (result.colorSource.layers.length == 0) {
            addCompositionDiagnostic(result, "missing-color-source",
                "PSD color source has no pixel layers.", settings.colorSourcePath, null);
            return false;
        }

        DepthLayerImage[] composedImages;
        DepthDrawSplitLayer[] splitLayers;
        foreach (sourceLayer; result.colorSource.layers) {
            if (!sourceLayer.visible || sourceLayer.width <= 0 || sourceLayer.height <= 0) continue;
            splitLayers ~= depthDrawSplitLayerFromCompositeSource(sourceLayer);
        }
        auto stableDepthPixels = ngDepthDrawDecodeDepthPixelsFromRgba(
            normalizedFlatDepthRgba, sampleChannel(settings.channel));
        auto visibleLayerMap = ngDepthDrawBuildVisibleLayerMap(texture.width, texture.height, splitLayers);
        int splitIndex;
        foreach (sourceLayer; result.colorSource.layers) {
            if (!sourceLayer.visible || sourceLayer.width <= 0 || sourceLayer.height <= 0) continue;
            auto image = imageFromColorSourceLayer(sourceLayer);
            auto splitLayer = depthDrawSplitLayerFromCompositeSource(sourceLayer);
            seedLayerDepthFromDepthDrawSplit(
                image,
                splitLayer,
                splitIndex,
                texture.width,
                texture.height,
                stableDepthPixels,
                visibleLayerMap,
                splitLayers,
                settings.repairContourBand
            );
            splitIndex++;
            buildCoverageCache(image);
            maskDepthToCoverage(image, settings);
            applyDepthDrawLayerDepthCleanup(image, settings);
            composedImages ~= image;
        }
        if (composedImages.length == 0) {
            addCompositionDiagnostic(result, "missing-color-source",
                "PSD color source has no visible pixel layers.", settings.colorSourcePath, null);
            return false;
        }
        DepthLayerImage[] layers;
        foreach (ref image; composedImages) {
            Deformable target;
            Node matchedTarget;
            bool manual;
            string status = "ComposedN1";
            if (auto overrideUuid = image.layerPath in settings.layerTargetGridUuidOverrides) {
                target = findGridByUuid(puppet, *overrideUuid);
                if (target !is null) {
                    matchedTarget = target;
                    manual = true;
                    status = "Manual";
                }
            }
            if (target is null) {
                auto candidates = matchCandidates(puppet, image.layerPath, image.layerName, settings,
                    image.left, image.top, image.width, image.height, texture.width, texture.height, true);
                if (candidates.length > 0) {
                    matchedTarget = candidates[0].node;
                    target = containingDepthTarget(matchedTarget);
                    if (target is null) {
                        if (auto part = cast(Part)matchedTarget) target = activeArtDepthTarget(puppet, part);
                    }
                }
            }
            if (target !is null && matchedTarget !is null) {
                addMappedPngLayer(image, target, matchedTarget, status, manual, layers);
            } else {
                PsdDepthLayerMapping colorMapping;
                colorMapping.layerPath = image.layerPath;
                colorMapping.layerName = image.layerName;
                colorMapping.status = "Unmatched";
                result.unmatchedLayers++;
                result.mappings ~= colorMapping;
                addCompositionDiagnostic(result, "unbound-composed-layer",
                    "PSD color/depth N:1 composed layer has no matching target binding.",
                    image.layerPath, image.layerName);
                addComposedLayer(result, path, image, settings, true, true);
            }
        }
        addMappedPngGridResults(layers);
        return true;
    }

    if (hasPsdColorSource) {
        addPsdColorComposedTargetsFromFlatDepth();
        return finishResult();
    }

    if (auto overrideUuid = layerPath in settings.layerTargetGridUuidOverrides) {
        grid = findGridByUuid(puppet, *overrideUuid);
        mapping.manual = true;
        if (grid is null) {
            mapping.status = "UnmatchedManualGrid";
            result.unmatchedLayers++;
            result.mappings ~= mapping;
            addCompositionDiagnostic(result, "missing-manual-target",
                "Manual target binding points to a missing GridDeformer or PathDeformer.", layerPath, layerName);
            auto image = basePngImage();
            addComposedLayer(result, path, image, settings, true, true);
            return finishResult();
        }
        matchedNode = grid;
        mapping.matched = true;
        mapping.matchedNodeName = grid.name;
        mapping.matchedNodeUuid = grid.uuid;
        mapping.targetGridName = grid.name;
        mapping.targetGridUuid = grid.uuid;
        mapping.status = "Manual";
        addDirectMatchedPngTarget(grid, matchedNode, "Manual", true);
        return finishResult();
    } else {
        auto candidates = matchCandidates(puppet, layerPath, layerName, settings);
        if (candidates.length > 0) {
            auto candidate = candidates[0];
            matchedNode = candidate.node;
            grid = containingDepthTarget(candidate.node);
            if (grid is null) {
                if (auto part = cast(Part)candidate.node) grid = activeArtDepthTarget(puppet, part);
            }
            mapping.matchedNodeName = candidate.node.name;
            mapping.matchedNodeUuid = candidate.node.uuid;
            mapping.ambiguous = candidates.length > 1;
            if (grid is null) {
                mapping.status = mapping.ambiguous ? "AmbiguousWithoutGrid" : "UnmatchedWithoutGrid";
                result.unmatchedLayers++;
                result.mappings ~= mapping;
                addCompositionDiagnostic(result, "missing-target",
                    "Depth source matched a node that is not inside a depth target.", layerPath, layerName);
                auto image = basePngImage();
                addComposedLayer(result, path, image, settings, true, true);
                return finishResult();
            }
            addDirectMatchedPngTarget(grid, matchedNode, mapping.ambiguous ? "Ambiguous" : "Matched", false);
            if (mapping.ambiguous) result.ambiguousLayers++;
            return finishResult();
        }

        if (addComposedTargetsFromFlatDepth()) {
            return finishResult();
        }

        {
            mapping.status = "Unmatched";
            result.unmatchedLayers++;
            result.mappings ~= mapping;
            if (hasFlatColorSource) {
                addCompositionDiagnostic(result, "unbound-composed-layer",
                    "Flat color/depth 1:1 composed layer has no matching target binding.",
                    flatColorLayerPath, flatColorLayerName);
            } else {
                addCompositionDiagnostic(result, "missing-color-source",
                    "Flat depth source has no matching or color/art-backed target layer.", layerPath, layerName);
            }
            auto image = basePngImage();
            addComposedLayer(result, path, image, settings, true, true);
        }
        return finishResult();
    }
}

PsdDepthImportResult ngBuildPsdDepthsFromSource(Puppet puppet, string path, PsdDepthImportSettings settings) {
    auto ext = path.extension.toLower;
    if (ext == ".png") return ngBuildPsdDepthsFromImage(puppet, path, settings);
    return ngBuildPsdDepthsFromPSD(puppet, path, settings);
}

PsdDepthImportResult ngPsdDepthReplaceDepthSource(
    Puppet puppet,
    ref PsdDepthImportResult previous,
    string depthSourcePath,
    PsdDepthImportSettings settings
) {
    enforce(depthSourcePath.length > 0, "Replacement depth source path is required");
    auto previousColorPath = previous.colorSource.sourcePath;
    if (settings.colorSourcePath.length == 0 && previousColorPath.length > 0) {
        settings.colorSourcePath = previousColorPath;
    }
    if (previous.composedLayers.length == 1 && previous.composedLayers[0].targetGridUuid != 0) {
        auto replacementLayerPath = "/" ~ depthSourcePath.baseName.stripExtension;
        if (!(replacementLayerPath in settings.layerTargetGridUuidOverrides)) {
            settings.layerTargetGridUuidOverrides[replacementLayerPath] =
                previous.composedLayers[0].targetGridUuid.to!string;
        }
    }
    auto result = ngBuildPsdDepthsFromSource(puppet, depthSourcePath, settings);
    ngPsdDepthApplyPreviousComposedLayerState(result, previous);
    applySingleLayerReplacementFallback(result, previous);
    return result;
}

PsdDepthImportResult ngPsdDepthReplaceColorSource(
    Puppet puppet,
    ref PsdDepthImportResult previous,
    string colorSourcePath,
    PsdDepthImportSettings settings
) {
    enforce(colorSourcePath.length > 0, "Replacement color source path is required");
    auto previousDepthPath = previous.depthSource.sourcePath;
    enforce(previousDepthPath.length > 0, "Previous depth source path is required");
    settings.colorSourcePath = colorSourcePath;
    auto result = ngBuildPsdDepthsFromSource(puppet, previousDepthPath, settings);
    ngPsdDepthApplyPreviousComposedLayerState(result, previous);
    applySingleLayerReplacementFallback(result, previous);
    return result;
}
