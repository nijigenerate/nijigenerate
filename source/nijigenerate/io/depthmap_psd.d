module nijigenerate.io.depthmap_psd;

import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.ext.nodes.expart;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import psd;
import std.algorithm : sort;
import std.algorithm.comparison : max, min;
import std.array : array;
import std.conv : to;
import std.exception : enforce;
import std.math : ceil, exp, floor, isFinite, round;
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
    GridDeformer grid;
    float[] depths;
    PsdDepthGridLayerMask[] layerMasks;
    size_t coverageSources;
    int previewLeft;
    int previewTop;
    int previewWidth;
    int previewHeight;
    ubyte[] rawCompositePreviewRgba;
    ubyte[] compositePreviewRgba;
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

private struct CoverageSource {
    int width;
    int height;
    int channels;
    float opacity = 1.0f;
    ubyte[] data;
    mat4 worldToLocal;
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
    GridDeformer grid;
}

private struct Candidate {
    Node node;
    int priority;
}

private struct DepthSample {
    bool valid;
    float value;
    float weight;
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

private float sampledDepth01(float value, ref PsdDepthImportSettings settings) {
    if (settings.depthScale == 0.0f || settings.frontDepth == settings.backDepth) return 0.0f;
    auto unscaled = value / settings.depthScale;
    return max(0.0f, min(1.0f, (unscaled - settings.backDepth) / (settings.frontDepth - settings.backDepth)));
}

private float layerOpacity01(ubyte opacity) {
    return cast(float)opacity / 255.0f;
}

private float effectiveAlpha(const(ubyte)[] rgba, size_t index, float opacity) {
    if (index + 3 >= rgba.length) return 0.0f;
    return (cast(float)rgba[index + 3] / 255.0f) * opacity;
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
    return layer.coverageData.length == 0 || coveragePixelAlpha(layer, x, y) > 0.5f;
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
        combined = 1.0f - ((1.0f - combined) * (1.0f - alpha));
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
    auto coverageX = cast(int)round(
        ((cast(float)x + 0.5f) * cast(float)layer.coverageWidth / cast(float)layer.width) - 0.5f);
    auto coverageY = cast(int)round(
        ((cast(float)y + 0.5f) * cast(float)layer.coverageHeight / cast(float)layer.height) - 0.5f);
    coverageX = max(0, min(layer.coverageWidth - 1, coverageX));
    coverageY = max(0, min(layer.coverageHeight - 1, coverageY));
    auto channels = max(1, layer.coverageChannels);
    auto index = (cast(size_t)coverageY * cast(size_t)layer.coverageWidth + cast(size_t)coverageX) * cast(size_t)channels;
    if (channels < 4 || index + 3 >= layer.coverageData.length) return 1.0f;
    return cast(float)layer.coverageData[index + 3] / 255.0f;
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
    if (source.channels < 4) return 1.0f;
    auto u = max(0.0f, min(1.0f, uv.x));
    auto v = max(0.0f, min(1.0f, uv.y));
    auto x = u * cast(float)max(0, source.width - 1);
    auto y = v * cast(float)max(0, source.height - 1);
    auto x0 = max(0, min(source.width - 1, cast(int)x));
    auto y0 = max(0, min(source.height - 1, cast(int)y));
    auto x1 = max(0, min(source.width - 1, x0 + 1));
    auto y1 = max(0, min(source.height - 1, y0 + 1));
    auto tx = x - cast(float)x0;
    auto ty = y - cast(float)y0;
    float a00 = coverageSourceAlphaAtPixel(source, x0, y0);
    float a10 = coverageSourceAlphaAtPixel(source, x1, y0);
    float a01 = coverageSourceAlphaAtPixel(source, x0, y1);
    float a11 = coverageSourceAlphaAtPixel(source, x1, y1);
    return lerp(lerp(a00, a10, tx), lerp(a01, a11, tx), ty);
}

private float coverageSourceAlphaAtPixel(ref CoverageSource source, int x, int y) {
    auto channels = max(1, source.channels);
    auto index = (cast(size_t)y * cast(size_t)source.width + cast(size_t)x) * cast(size_t)channels;
    if (channels < 4 || index + 3 >= source.data.length) return 1.0f;
    return cast(float)source.data[index + 3] / 255.0f;
}

private void compositeCoverage(ref float dst, float alpha) {
    alpha = clamp01(alpha);
    dst = 1.0f - ((1.0f - dst) * (1.0f - alpha));
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
                    compositeCoverage(layer.coverageGateCache[index], alpha);
                    compositeCoverage(layer.coverageAlphaCache[index], alpha * source.opacity);
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
                    compositeCoverage(layer.coverageGateCache[index], alpha);
                    compositeCoverage(layer.coverageAlphaCache[index], alpha * source.opacity);
                }
            }
        }
    }
}

private float sampleCoverageAlphaAtUv(ref DepthLayerImage layer, vec2 uv) {
    if (layer.coverageChannels < 4) return 1.0f;
    auto u = max(0.0f, min(1.0f, uv.x));
    auto v = max(0.0f, min(1.0f, uv.y));
    auto x = u * cast(float)max(0, layer.coverageWidth - 1);
    auto y = v * cast(float)max(0, layer.coverageHeight - 1);
    auto x0 = max(0, min(layer.coverageWidth - 1, cast(int)x));
    auto y0 = max(0, min(layer.coverageHeight - 1, cast(int)y));
    auto x1 = max(0, min(layer.coverageWidth - 1, x0 + 1));
    auto y1 = max(0, min(layer.coverageHeight - 1, y0 + 1));
    auto tx = x - cast(float)x0;
    auto ty = y - cast(float)y0;
    float a00 = coverageAlphaAtPixel(layer, x0, y0);
    float a10 = coverageAlphaAtPixel(layer, x1, y0);
    float a01 = coverageAlphaAtPixel(layer, x0, y1);
    float a11 = coverageAlphaAtPixel(layer, x1, y1);
    return lerp(lerp(a00, a10, tx), lerp(a01, a11, tx), ty);
}

private float coverageAlphaAtPixel(ref DepthLayerImage layer, int x, int y) {
    auto channels = max(1, layer.coverageChannels);
    auto index = (cast(size_t)y * cast(size_t)layer.coverageWidth + cast(size_t)x) * cast(size_t)channels;
    if (channels < 4 || index + 3 >= layer.coverageData.length) return 1.0f;
    return cast(float)layer.coverageData[index + 3] / 255.0f;
}

private float effectiveAlpha(ref DepthLayerImage layer, size_t index, int x, int y) {
    return effectiveAlpha(layer.data, index, layer.opacity) * coverageAlpha(layer, x, y);
}

private ubyte effectiveAlphaByte(ref DepthLayerImage layer, size_t index, int x, int y) {
    auto alpha = effectiveAlpha(layer, index, x, y);
    return cast(ubyte)round(max(0.0f, min(1.0f, alpha)) * 255.0f);
}

private ubyte[] buildDepthMaskPreview(ref DepthLayerImage layer, ref PsdDepthImportSettings settings) {
    ubyte[] result;
    auto pixels = cast(size_t)max(0, layer.width * layer.height);
    result.length = pixels * 4;
    for (int y = 0; y < layer.height; y++) {
        for (int x = 0; x < layer.width; x++) {
            auto index = (cast(size_t)y * cast(size_t)layer.width + cast(size_t)x) * 4;
            if (!coverageReliable(layer, x, y) || effectiveAlpha(layer, index, x, y) <= settings.alphaThreshold) {
                result[index + 0] = 0;
                result[index + 1] = 0;
                result[index + 2] = 0;
                result[index + 3] = 0;
                continue;
            }
            auto gray = cast(ubyte)round(pixelDepth01(layer.data, index, settings) * 255.0f);
            result[index + 0] = gray;
            result[index + 1] = gray;
            result[index + 2] = gray;
            result[index + 3] = 255;
        }
    }
    return result;
}

private DepthSample samplePixel(ref DepthLayerImage layer, int x, int y, ref PsdDepthImportSettings settings) {
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return DepthSample(false, 0, 0);
    auto index = (cast(size_t)y * cast(size_t)layer.width + cast(size_t)x) * 4;
    if (!coverageReliable(layer, x, y)) return DepthSample(false, 0, 0);
    auto alpha = effectiveAlpha(layer, index, x, y);
    if (alpha <= settings.alphaThreshold) return DepthSample(false, 0, 0);
    return DepthSample(true, pixelDepth(layer.data, index, settings), alpha);
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
            auto weight = kernelWeight(settings, dx, dy) * sample.weight;
            total += sample.value * weight;
            weightTotal += weight;
        }
    }
    if (weightTotal <= 0) return DepthSample(false, 0, 0);
    return DepthSample(true, total / weightTotal, weightTotal);
}

private DepthSample medianSample(ref DepthLayerImage layer, int cx, int cy, int radius, ref PsdDepthImportSettings settings) {
    float[] values;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            auto sample = samplePixel(layer, cx + dx, cy + dy, settings);
            if (sample.valid) values ~= sample.value;
        }
    }
    if (values.length == 0) return DepthSample(false, 0, 0);
    values.sort();
    return DepthSample(true, values[values.length / 2], 1.0f);
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
    return DepthSample(hasValue, hasValue ? best : 0, hasValue ? 1.0f : 0.0f);
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
    source.worldToLocal = part.getDynamicMatrix().inverse;
    auto mesh = part.getMesh();
    source.vertices = mesh.vertices.dup;
    source.uvs = mesh.uvs.dup;
    source.indices = mesh.indices.dup;
    source.origin = mesh.origin;
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

private bool nodeCapturedByGrid(GridDeformer grid, Node target) {
    if (grid is null || target is null) return false;
    foreach (child; grid.children) {
        if (nodeTreeContains(child, target)) return true;
    }
    return false;
}

private void attachNodeCoverage(ref DepthLayerImage image, Node node, ref bool[ulong] seen) {
    if (node is null) return;
    if (auto part = cast(Part)node) {
        attachPartCoverage(image, part, seen);
        return;
    }
    foreach (child; node.children) {
        attachNodeCoverage(image, child, seen);
    }
}

private void attachGridCoverage(ref DepthLayerImage image, Puppet puppet, GridDeformer grid, ref bool[ulong] seen) {
    if (puppet is null || puppet.root is null || grid is null) return;
    foreach (part; puppet.findNodesType!Part(puppet.root)) {
        if (containingGrid(part) is grid || nodeCapturedByGrid(grid, part)) {
            attachPartCoverage(image, part, seen);
        }
    }
}

private void attachMatchedCoverage(ref DepthLayerImage image, Puppet puppet, Node matchedNode, GridDeformer grid) {
    bool[ulong] seen;
    if (auto part = cast(Part)matchedNode) {
        attachPartCoverage(image, part, seen);
        return;
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
    if (!ngPsdDepthGridEnabled(settings, accum.grid.uuid)) {
        result.depths = existing;
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

private void buildCompositePreview(
    ref PsdDepthGridResult result,
    DepthLayerImage[] layers,
    GridDeformer grid,
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
                auto layerX = cast(int)round(documentX - cast(float)layer.left);
                auto layerY = cast(int)round(documentY - cast(float)layer.top);
                if (layerX < 0 || layerY < 0 || layerX >= layer.width || layerY >= layer.height) continue;
                auto index = (cast(size_t)layerY * cast(size_t)layer.width + cast(size_t)layerX) * 4;
                auto rawAlpha = effectiveAlpha(layer.data, index, layer.opacity);
                if (rawAlpha > settings.alphaThreshold) {
                    auto rawDepth = pixelDepth(layer.data, index, settings);
                    if (!hasRawSample || rawDepth > rawBestDepth) {
                        hasRawSample = true;
                        rawBestDepth = rawDepth;
                        rawBestGray = pixelDepth01(layer.data, index, settings);
                        rawBestAlpha = cast(ubyte)round(max(0.0f, min(1.0f, rawAlpha)) * 255.0f);
                    }
                }
                if (!coverageReliable(layer, layerX, layerY)) continue;
                auto alpha = effectiveAlpha(layer, index, layerX, layerY);
                if (alpha <= settings.alphaThreshold) continue;
                auto depth = pixelDepth(layer.data, index, settings);
                if (!hasSample || depth > bestDepth) {
                    hasSample = true;
                    bestDepth = depth;
                    bestGray = pixelDepth01(layer.data, index, settings);
                    bestAlpha = cast(ubyte)round(max(0.0f, min(1.0f, alpha)) * 255.0f);
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
        Node matchedNode;
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
            matchedNode = grid;
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
            matchedNode = candidate.node;
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

        auto opacity = layerOpacity01(layer.opacity);
        DepthLayerImage image;
        image.layerPath = layerPath;
        image.layerName = layer.name;
        image.left = layer.left;
        image.top = layer.top;
        image.width = layer.width;
        image.height = layer.height;
        image.documentWidth = document.width;
        image.documentHeight = document.height;
        image.opacity = opacity;
        image.data = layer.data.dup;
        image.grid = grid;
        attachMatchedCoverage(image, puppet, matchedNode, grid);
        buildCoverageCache(image);

        PsdDepthLayerPreview layerPreview;
        layerPreview.layerPath = layerPath;
        layerPreview.layerName = layer.name;
        layerPreview.left = layer.left;
        layerPreview.top = layer.top;
        layerPreview.width = layer.width;
        layerPreview.height = layer.height;
        layerPreview.originalRgba = layer.data.dup;
        layerPreview.depthMaskRgba = buildDepthMaskPreview(image, settings);
        result.layerPreviews ~= layerPreview;

        layers ~= image;
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
        foreach (ref layer; layers) {
            if (layer.grid is accum.grid) gridResult.coverageSources += layer.coverageSources.length;
        }
        buildCompositePreview(gridResult, layers, accum.grid, settings);
        if (gridResult.skipped) result.skippedGrids++;
        result.grids ~= gridResult;
    }

    return result;
}
