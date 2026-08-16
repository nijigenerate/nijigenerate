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

enum int DepthDrawMaxContourThickness = 64;
enum int DepthDrawMaxFocusedRuleRadius = 64;

int ngNormalizeDepthDrawContourThickness(int thickness) {
    if (thickness < 1) return 1;
    if (thickness > DepthDrawMaxContourThickness) return DepthDrawMaxContourThickness;
    return thickness;
}

DepthDrawAlphaDepthFocusedRule ngNormalizeDepthDrawFocusedRule(
    DepthDrawAlphaDepthFocusedRule rule,
    int width,
    int height,
) {
    width = width < 0 ? 0 : width;
    height = height < 0 ? 0 : height;
    if (rule.x < 0) rule.x = 0;
    if (rule.x > width) rule.x = width;
    if (rule.y < 0) rule.y = 0;
    if (rule.y > height) rule.y = height;
    if (rule.w < 0) rule.w = 0;
    if (rule.w > width - rule.x) rule.w = width - rule.x;
    if (rule.h < 0) rule.h = 0;
    if (rule.h > height - rule.y) rule.h = height - rule.y;
    if (rule.lift < 0) rule.lift = 0;
    if (rule.lift > 255) rule.lift = 255;
    if (rule.radius < 0) rule.radius = 0;
    if (rule.radius > DepthDrawMaxFocusedRuleRadius) rule.radius = DepthDrawMaxFocusedRuleRadius;
    return rule;
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
