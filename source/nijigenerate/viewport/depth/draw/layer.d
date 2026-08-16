module nijigenerate.viewport.depth.draw.layer;

import nijigenerate.io.depthimage : DepthDrawAlphaDepthFocusedRule, DepthImageChannel, DepthImageConvolution,
    DepthImageSampleSettings, ngNormalizeDepthImageCustomRadius;
import nijilive.math : vec2;

struct DepthDrawRect {
    int left;
    int top;
    int width;
    int height;
}

enum DepthDrawLayerCleanupKind {
    AlphaDepthGapFill,
    ContourRepair,
}

struct DepthDrawLayerCleanupOperation {
    DepthDrawLayerCleanupKind kind;
    int contourThickness = 2;
    DepthDrawAlphaDepthFocusedRule[] focusedRules;
}

struct DepthDrawLayer {
    string id;
    string sourcePath;
    string layerPath;
    string displayName;

    int width;
    int height;
    DepthDrawRect bounds;

    ubyte[] rgba;
    ubyte[] depthPixels;
    ubyte[] alphaMask;
    ubyte[] normalCoverage;
    DepthDrawLayerCleanupOperation[] cleanupOperations;

    float opacity = 1.0f;
    bool visible = true;
    bool enabled = true;

    vec2 xyOffset = vec2(0, 0);
    vec2 xyScale = vec2(1, 1);

    float zOffset = 0.0f;
    float zScale = 1.0f;
    float backDepth = -1.0f;
    float frontDepth = 1.0f;
    float sampleDepthScale = 1.0f;
    bool invert = false;

    DepthImageChannel channel = DepthImageChannel.AverageRGB;
    DepthImageConvolution convolution = DepthImageConvolution.Gaussian3x3;
    int customRadius = 3;
    float alphaThreshold = 0.01f;

    bool hasDepthPixels() const {
        return width > 0 && height > 0 && depthPixels.length >= cast(size_t)width * cast(size_t)height * 4;
    }

    bool hasNormalCoverage() const {
        return normalCoverage.length >= cast(size_t)width * cast(size_t)height * 4;
    }

    DepthImageSampleSettings sampleSettings() const {
        DepthImageSampleSettings settings;
        settings.invert = invert;
        settings.backDepth = backDepth;
        settings.frontDepth = frontDepth;
        settings.depthScale = sampleDepthScale;
        settings.alphaThreshold = alphaThreshold;
        settings.customRadius = ngNormalizeDepthImageCustomRadius(customRadius);
        settings.convolution = convolution;
        settings.channel = channel;
        return settings;
    }

    float applyZTransform(float rawDepth) const {
        return rawDepth * zScale + zOffset;
    }
}
