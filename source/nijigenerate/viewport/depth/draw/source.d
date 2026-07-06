module nijigenerate.viewport.depth.draw.source;

import nijigenerate.io.depthimage : ngDepthDrawAlphaMaskFromRgba, ngDepthDrawApplyPsdMaskToAlpha;
import nijigenerate.viewport.depth.draw.layer;
import nijigenerate.viewport.depth.draw.session;
import nijilive : ShallowTexture;
import psd;
import std.array : join;
import std.algorithm : max, min;
import std.math : lround;
import std.path : baseName, stripExtension;
import std.string : format;

struct DepthDrawPsdLoadResult {
    int documentWidth;
    int documentHeight;
    DepthDrawLayer[] layers;
    DepthDrawSession session;
}

struct DepthDrawLayerPairingResult {
    size_t matchedDepthLayers;
    size_t missingNormalCoverage;
    string[] matchedDepthLayerIds;
    string[] missingNormalLayerIds;
}

DepthDrawLayer ngLoadDepthDrawPngLayer(string path, string id = null) {
    auto tex = ShallowTexture(path, 4);

    DepthDrawLayer layer;
    layer.id = id.length ? id : path.baseName.stripExtension;
    layer.sourcePath = path;
    layer.layerPath = "/" ~ layer.id;
    layer.displayName = layer.id;
    layer.width = tex.width;
    layer.height = tex.height;
    layer.bounds.left = 0;
    layer.bounds.top = 0;
    layer.bounds.width = tex.width;
    layer.bounds.height = tex.height;
    layer.rgba = tex.data.dup;
    layer.depthPixels = tex.data.dup;
    return layer;
}

DepthDrawPsdLoadResult ngLoadDepthDrawPsd(string path) {
    auto document = parseDocument(path);
    scope(exit) destroy(document);

    DepthDrawPsdLoadResult result;
    result.documentWidth = document.width;
    result.documentHeight = document.height;
    result.session = new DepthDrawSession();

    string[] layerPathSegments;
    string groupPath;
    size_t layerIndex;
    foreach_reverse (layer; document.layers) {
        if (layer.type != LayerType.Any) {
            if (layer.name != "</Layer set>" && layer.name != "</Layer group>") {
                layerPathSegments ~= layer.name;
            } else if (layerPathSegments.length > 0) {
                layerPathSegments.length--;
            }
            groupPath = layerPathSegments.length > 0 ? "/" ~ layerPathSegments.join("/") : "";
            continue;
        }

        layer.extractLayerImage();

        DepthDrawLayer drawLayer;
        drawLayer.id = "psd:%s".format(layerIndex);
        drawLayer.sourcePath = path;
        drawLayer.layerPath = "%s/%s".format(groupPath, layer.name);
        drawLayer.displayName = layer.name;
        drawLayer.width = cast(int)layer.width;
        drawLayer.height = cast(int)layer.height;
        drawLayer.bounds.left = layer.left;
        drawLayer.bounds.top = layer.top;
        drawLayer.bounds.width = cast(int)layer.width;
        drawLayer.bounds.height = cast(int)layer.height;
        drawLayer.opacity = cast(float)layer.opacity / 255.0f;
        drawLayer.visible = (layer.flags & LayerFlags.Visible) == 0;
        drawLayer.enabled = drawLayer.visible;
        drawLayer.rgba = layer.data.dup;
        drawLayer.depthPixels = layer.data.dup;
        applyPsdLayerMaskIfAvailable(drawLayer, layer);
        drawLayer.alphaMask = ngDepthDrawAlphaMaskFromRgba(drawLayer.rgba);

        result.layers ~= drawLayer;
        result.session.layers ~= drawLayer;
        layer.data = null;
        layerIndex++;
    }

    return result;
}

void ngDepthDrawApplyMaskToLayerAlpha(
    ref DepthDrawLayer layer,
    const(ubyte)[] mask,
    int maskWidth,
    int maskHeight,
    int maskLeft,
    int maskTop,
    bool positionRelativeToLayer,
    ubyte defaultMask = 255
) {
    if (layer.width <= 0 || layer.height <= 0 || maskWidth <= 0 || maskHeight <= 0) return;
    auto pixelCount = cast(size_t)layer.width * cast(size_t)layer.height;
    if (layer.rgba.length < pixelCount * 4 || mask.length < cast(size_t)maskWidth * cast(size_t)maskHeight) return;

    ubyte[] alpha;
    alpha.length = pixelCount;
    foreach (i; 0 .. pixelCount) alpha[i] = layer.rgba[i * 4 + 3];

    auto maskedAlpha = ngDepthDrawApplyPsdMaskToAlpha(
        alpha,
        layer.width,
        layer.height,
        layer.bounds.left,
        layer.bounds.top,
        mask,
        maskWidth,
        maskHeight,
        maskLeft,
        maskTop,
        positionRelativeToLayer,
        defaultMask
    );

    foreach (i, value; maskedAlpha) {
        layer.rgba[i * 4 + 3] = value;
        if (layer.depthPixels.length >= pixelCount * 4) layer.depthPixels[i * 4 + 3] = value;
    }
    layer.alphaMask = ngDepthDrawAlphaMaskFromRgba(layer.rgba);
}

private void applyPsdLayerMaskIfAvailable(ref DepthDrawLayer drawLayer, ref Layer psdLayer) {
    if (psdLayer.layerMask.length == 0) return;
    auto mask = psdLayer.layerMask[0];
    auto maskWidth = mask.right - mask.left;
    auto maskHeight = mask.bottom - mask.top;
    if (maskWidth <= 0 || maskHeight <= 0) return;

    auto maskBytes = psdLayerMaskChannelBytes(psdLayer, maskWidth, maskHeight);
    if (maskBytes.length < cast(size_t)maskWidth * cast(size_t)maskHeight) return;
    ngDepthDrawApplyMaskToLayerAlpha(
        drawLayer,
        maskBytes,
        maskWidth,
        maskHeight,
        mask.left,
        mask.top,
        false,
        mask.defaultColor
    );
}

private ubyte[] psdLayerMaskChannelBytes(ref Layer psdLayer, int maskWidth, int maskHeight) {
    foreach (ref channel; psdLayer.channels) {
        if (channel.type != ChannelType.LAYER_MASK && channel.type != ChannelType.LAYER_OR_VECTOR_MASK) continue;
        if (channel.data.length >= cast(size_t)maskWidth * cast(size_t)maskHeight) {
            return channel.data[0 .. cast(size_t)maskWidth * cast(size_t)maskHeight].dup;
        }
    }
    return null;
}

DepthDrawLayerPairingResult ngDepthDrawAttachNormalCoverage(
    DepthDrawSession depthSession,
    const(DepthDrawLayer)[] normalLayers
) {
    DepthDrawLayerPairingResult result;
    if (depthSession is null) return result;

    bool[] usedNormalLayers;
    usedNormalLayers.length = normalLayers.length;
    foreach (depthIndex; 0 .. depthSession.layers.length) {
        if (!isLayerUsableForDepthDrawComposition(depthSession.layers[depthIndex])) continue;
        auto normalIndex = takeMatchedNormalLayer(depthSession.layers[depthIndex], depthIndex, normalLayers, usedNormalLayers);
        if (normalIndex < 0) {
            result.missingNormalCoverage++;
            result.missingNormalLayerIds ~= depthSession.layers[depthIndex].id;
            continue;
        }

        depthSession.layers[depthIndex].normalCoverage = buildNormalCoverageForDepthLayer(
            depthSession.layers[depthIndex],
            normalLayers[cast(size_t)normalIndex]
        );
        usedNormalLayers[cast(size_t)normalIndex] = true;
        result.matchedDepthLayers++;
        result.matchedDepthLayerIds ~= depthSession.layers[depthIndex].id;
    }
    return result;
}

private ptrdiff_t takeMatchedNormalLayer(
    ref DepthDrawLayer depthLayer,
    size_t fallbackDepthIndex,
    const(DepthDrawLayer)[] normalLayers,
    const(bool)[] usedNormalLayers
) {
    foreach (i, ref normalLayer; normalLayers) {
        if (usedNormalLayers[i]) continue;
        if (!isLayerUsableForDepthDrawComposition(normalLayer)) continue;
        if (layerMatchKey(normalLayer) == layerMatchKey(depthLayer)) return cast(ptrdiff_t)i;
    }

    auto depthName = normalizeLayerName(depthLayer.displayName);
    ptrdiff_t bestNamedIndex = -1;
    long bestNamedScore = -1;
    if (depthName.length) {
        foreach (i, ref normalLayer; normalLayers) {
            if (usedNormalLayers[i]) continue;
            if (!isLayerUsableForDepthDrawComposition(normalLayer)) continue;
            if (normalizeLayerName(normalLayer.displayName) != depthName) continue;
            auto score = scoreLayerMatch(normalLayer, depthLayer, i, fallbackDepthIndex);
            if (score > bestNamedScore) {
                bestNamedScore = score;
                bestNamedIndex = cast(ptrdiff_t)i;
            }
        }
    }
    if (bestNamedIndex >= 0) return bestNamedIndex;

    ptrdiff_t bestOverlapIndex = -1;
    long bestOverlapScore = -1;
    int bestOverlap = 0;
    foreach (i, ref normalLayer; normalLayers) {
        if (usedNormalLayers[i]) continue;
        if (!isLayerUsableForDepthDrawComposition(normalLayer)) continue;
        auto overlap = estimateLayerRectOverlap(normalLayer, depthLayer);
        if (overlap <= 0) continue;
        auto score = scoreLayerMatch(normalLayer, depthLayer, i, fallbackDepthIndex);
        if (score > bestOverlapScore) {
            bestOverlapScore = score;
            bestOverlap = overlap;
            bestOverlapIndex = cast(ptrdiff_t)i;
        }
    }
    if (bestOverlapIndex >= 0 && bestOverlap > 0) return bestOverlapIndex;

    foreach (i, ref normalLayer; normalLayers) {
        if (usedNormalLayers[i]) continue;
        if (!isLayerUsableForDepthDrawComposition(normalLayer)) continue;
        if (isEmptyFallbackLayer(normalLayer)) return cast(ptrdiff_t)i;
    }
    return -1;
}

private bool isLayerUsableForDepthDrawComposition(ref const(DepthDrawLayer) layer) {
    // Matches depth-draw src/composite/colorSources.js:flattenCompositeLayers hidden-layer exclusion.
    return layer.enabled && layer.visible;
}

private bool isEmptyFallbackLayer(ref const(DepthDrawLayer) layer) {
    return normalizeLayerName(layer.displayName).length == 0 &&
        layer.bounds.left == 0 &&
        layer.bounds.top == 0 &&
        layer.width == 0 &&
        layer.height == 0;
}

private string layerMatchKey(ref const(DepthDrawLayer) layer) {
    return "%s\0%s\0%s\0%s\0%s".format(
        normalizeLayerName(layer.displayName),
        layer.bounds.left,
        layer.bounds.top,
        layer.width,
        layer.height
    );
}

private string normalizeLayerName(string name) {
    string result;
    bool lastWasSpace = true;
    foreach (dchar ch; name) {
        auto isSpace = ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
        if (isSpace) {
            if (!lastWasSpace && result.length) result ~= ' ';
            lastWasSpace = true;
            continue;
        }
        if (ch >= 'A' && ch <= 'Z') {
            result ~= cast(char)(ch - 'A' + 'a');
        } else if (ch <= 0x7f) {
            result ~= cast(char)ch;
        } else {
            result ~= ch;
        }
        lastWasSpace = false;
    }
    if (result.length && result[$ - 1] == ' ') result.length--;
    return result;
}

private int estimateLayerRectOverlap(ref const(DepthDrawLayer) layerA, ref const(DepthDrawLayer) layerB) {
    auto left = max(layerA.bounds.left, layerB.bounds.left);
    auto top = max(layerA.bounds.top, layerB.bounds.top);
    auto right = min(layerA.bounds.left + layerA.width, layerB.bounds.left + layerB.width);
    auto bottom = min(layerA.bounds.top + layerA.height, layerB.bounds.top + layerB.height);
    return max(0, right - left) * max(0, bottom - top);
}

private long scoreLayerMatch(
    ref const(DepthDrawLayer) normalLayer,
    ref const(DepthDrawLayer) depthLayer,
    size_t fallbackNormalIndex,
    size_t fallbackDepthIndex
) {
    auto normalName = normalizeLayerName(normalLayer.displayName);
    auto depthName = normalizeLayerName(depthLayer.displayName);
    auto score = cast(long)estimateLayerRectOverlap(normalLayer, depthLayer);
    if (normalName.length && normalName == depthName) score += 100000000;
    if (normalLayer.bounds.left == depthLayer.bounds.left && normalLayer.bounds.top == depthLayer.bounds.top) {
        score += 1000000;
    }
    if (normalLayer.width == depthLayer.width && normalLayer.height == depthLayer.height) {
        score += 100000;
    }
    if (fallbackNormalIndex == fallbackDepthIndex) score += 1000;
    return score;
}

private ubyte[] buildNormalCoverageForDepthLayer(ref DepthDrawLayer depthLayer, ref const(DepthDrawLayer) normalLayer) {
    ubyte[] result;
    result.length = cast(size_t)max(0, depthLayer.width * depthLayer.height) * 4;
    if (normalLayer.rgba.length < cast(size_t)max(0, normalLayer.width * normalLayer.height) * 4) return result;

    foreach (y; 0 .. depthLayer.height) {
        auto documentY = depthLayer.bounds.top + y;
        auto normalY = documentY - normalLayer.bounds.top;
        if (normalY < 0 || normalY >= normalLayer.height) continue;
        foreach (x; 0 .. depthLayer.width) {
            auto documentX = depthLayer.bounds.left + x;
            auto normalX = documentX - normalLayer.bounds.left;
            if (normalX < 0 || normalX >= normalLayer.width) continue;
            auto outIndex = (cast(size_t)y * cast(size_t)depthLayer.width + cast(size_t)x) * 4;
            auto normalIndex = (cast(size_t)normalY * cast(size_t)normalLayer.width + cast(size_t)normalX) * 4;
            auto alpha = cast(ubyte)min(255, max(0,
                cast(int)lround((cast(float)normalLayer.rgba[normalIndex + 3] * normalLayer.opacity))));
            result[outIndex + 0] = 255;
            result[outIndex + 1] = 255;
            result[outIndex + 2] = 255;
            result[outIndex + 3] = alpha;
        }
    }
    return result;
}
