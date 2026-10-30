#pragma once
namespace nijigenerate::viewport::depth::draw::gpu_detail {
inline constexpr char layerSampleVertexSource[] = R"NGGLSL(#version 330

#define LAYER_WIDTH 0u
#define LAYER_HEIGHT 1u
#define LAYER_FLAGS 4u
#define LAYER_BOUNDS_LEFT 5u
#define LAYER_BOUNDS_TOP 6u
#define LAYER_OPACITY 9u
#define LAYER_XY_OFFSET_X 10u
#define LAYER_XY_OFFSET_Y 11u
#define LAYER_XY_SCALE_X 12u
#define LAYER_XY_SCALE_Y 13u
#define LAYER_Z_OFFSET 14u
#define LAYER_Z_SCALE 15u
#define LAYER_BACK_DEPTH 16u
#define LAYER_FRONT_DEPTH 17u
#define LAYER_ALPHA_THRESHOLD 18u
#define LAYER_CHANNEL 19u
#define LAYER_CONVOLUTION 20u
#define LAYER_CUSTOM_RADIUS 21u
#define LAYER_SAMPLE_DEPTH_SCALE 22u

#define BINDING_FLAGS 2u
#define BINDING_COVERAGE_THRESHOLD 3u

layout(location = 0) in float inDocumentX;
layout(location = 1) in float inDocumentY;

out float outValid;
out float outDepth;

uniform samplerBuffer layerData;
uniform samplerBuffer bindingData;
uniform usamplerBuffer depthPixels;
uniform usamplerBuffer normalCoveragePixels;

float layerValue(uint index) {
    return texelFetch(layerData, int(index)).r;
}

float bindingValue(uint index) {
    return texelFetch(bindingData, int(index)).r;
}

float depthPixelValue(uint index) {
    return float(texelFetch(depthPixels, int(index)).r);
}

float coveragePixelValue(uint index) {
    return float(texelFetch(normalCoveragePixels, int(index)).r);
}

float depth01(uint base, uint channel, bool invert) {
    float r = depthPixelValue(base + 0u);
    float g = depthPixelValue(base + 1u);
    float b = depthPixelValue(base + 2u);
    float value = 0.0;
    if (channel == 1u) {
        value = r / 255.0;
    } else if (channel == 2u) {
        value = g / 255.0;
    } else if (channel == 3u) {
        value = b / 255.0;
    } else if (channel == 4u) {
        value = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0;
    } else {
        value = (r + g + b) / (255.0 * 3.0);
    }
    return invert ? 1.0 - value : value;
}

int convolutionRadius(uint convolution, int customRadius) {
    if (convolution == 0u) return 0;
    if (convolution == 2u || convolution == 4u) return 2;
    if (convolution >= 8u) return max(1, customRadius);
    return 1;
}

float gaussianCustomWeight(int radius, int dx, int dy) {
    float sigma = max(1.0, float(radius) / 2.0);
    float distance2 = float(dx * dx + dy * dy);
    return exp(-distance2 / (2.0 * sigma * sigma));
}

float kernelWeight(uint convolution, int customRadius, int dx, int dy) {
    if (convolution == 3u) {
        return float((2 - abs(dx)) * (2 - abs(dy)));
    }
    if (convolution == 4u) {
        int wx = abs(dx) == 0 ? 6 : (abs(dx) == 1 ? 4 : 1);
        int wy = abs(dy) == 0 ? 6 : (abs(dy) == 1 ? 4 : 1);
        return float(wx * wy);
    }
    if (convolution == 9u) {
        return gaussianCustomWeight(max(1, customRadius), dx, dy);
    }
    return 1.0;
}

bool convolutionUsesWeighted(uint convolution) {
    return convolution == 1u || convolution == 2u || convolution == 3u ||
        convolution == 4u || convolution == 8u || convolution == 9u;
}

bool convolutionUsesFrontmost(uint convolution) {
    return convolution == 6u || convolution == 11u;
}

bool convolutionUsesBackmost(uint convolution) {
    return convolution == 7u || convolution == 12u;
}

bool convolutionUsesMedian(uint convolution) {
    return convolution == 5u;
}

bool samplePixel(
    int layerX,
    int layerY,
    uint width,
    uint height,
    uint channel,
    bool invert,
    bool hasCoverage,
    bool useCoverage,
    float threshold,
    out float sampledDepth,
    out float sampledWeight
) {
    sampledDepth = 0.0;
    sampledWeight = 0.0;
    if (layerX < 0 || layerY < 0 || layerX >= int(width) || layerY >= int(height)) return false;
    uint pixel = uint(layerY) * width + uint(layerX);
    uint base = pixel * 4u;
    float alpha = (depthPixelValue(base + 3u) / 255.0) * clamp(layerValue(LAYER_OPACITY), 0.0, 1.0);
    if (useCoverage && hasCoverage) {
        float coverageAlpha = coveragePixelValue(base + 3u) / 255.0;
        if (coverageAlpha <= 0.5) return false;
        alpha *= coverageAlpha;
    }
    if (!(alpha > threshold)) return false;
    float rawDepth = mix(layerValue(LAYER_BACK_DEPTH), layerValue(LAYER_FRONT_DEPTH), depth01(base, channel, invert));
    rawDepth *= layerValue(LAYER_SAMPLE_DEPTH_SCALE);
    sampledDepth = rawDepth;
    sampledWeight = alpha;
    return true;
}

float applyZTransform(float sampledDepth) {
    return sampledDepth * layerValue(LAYER_Z_SCALE) + layerValue(LAYER_Z_OFFSET);
}

void main() {
    uint flags = uint(round(layerValue(LAYER_FLAGS)));
    bool invert = (flags & 1u) != 0u;
    bool hasCoverage = (flags & 2u) != 0u;
    uint bindingFlags = uint(round(bindingValue(BINDING_FLAGS)));
    bool useCoverage = (bindingFlags & 1u) != 0u;

    uint width = uint(round(layerValue(LAYER_WIDTH)));
    uint height = uint(round(layerValue(LAYER_HEIGHT)));
    float scaleX = layerValue(LAYER_XY_SCALE_X);
    float scaleY = layerValue(LAYER_XY_SCALE_Y);
    if (scaleX == 0.0) scaleX = 1.0;
    if (scaleY == 0.0) scaleY = 1.0;

    float layerXf = (inDocumentX - layerValue(LAYER_BOUNDS_LEFT) - layerValue(LAYER_XY_OFFSET_X)) / scaleX;
    float layerYf = (inDocumentY - layerValue(LAYER_BOUNDS_TOP) - layerValue(LAYER_XY_OFFSET_Y)) / scaleY;
    int layerX = int(round(layerXf));
    int layerY = int(round(layerYf));
    float threshold = max(layerValue(LAYER_ALPHA_THRESHOLD), bindingValue(BINDING_COVERAGE_THRESHOLD));
    uint channel = uint(round(layerValue(LAYER_CHANNEL)));
    uint convolution = uint(round(layerValue(LAYER_CONVOLUTION)));
    int customRadius = int(round(layerValue(LAYER_CUSTOM_RADIUS)));
    int radius = convolutionRadius(convolution, customRadius);

    float sampleDepth = 0.0;
    float sampleWeight = 0.0;
    if (convolution == 0u) {
        if (!samplePixel(layerX, layerY, width, height, channel, invert, hasCoverage, useCoverage,
            threshold, sampleDepth, sampleWeight)) {
            outValid = 0.0;
            outDepth = 0.0;
            return;
        }
        outValid = 1.0;
        outDepth = applyZTransform(sampleDepth);
        return;
    }

    bool any = false;
    float weightedTotal = 0.0;
    float weightTotal = 0.0;
    float extremeDepth = 0.0;
    float medianValues[9];
    int medianCount = 0;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (!samplePixel(layerX + dx, layerY + dy, width, height, channel, invert, hasCoverage,
                useCoverage, threshold, sampleDepth, sampleWeight)) {
                continue;
            }
            if (convolutionUsesWeighted(convolution)) {
                float weight = sampleWeight * kernelWeight(convolution, customRadius, dx, dy);
                if (weight <= 0.0) continue;
                weightedTotal += sampleDepth * weight;
                weightTotal += weight;
                any = true;
            } else if (convolutionUsesFrontmost(convolution) || convolutionUsesBackmost(convolution)) {
                if (!any || (convolutionUsesFrontmost(convolution) ? sampleDepth > extremeDepth : sampleDepth < extremeDepth)) {
                    extremeDepth = sampleDepth;
                }
                any = true;
            } else if (convolutionUsesMedian(convolution)) {
                if (medianCount < 9) {
                    medianValues[medianCount] = sampleDepth;
                    medianCount += 1;
                    any = true;
                }
            }
        }
    }

    if (!any) {
        outValid = 0.0;
        outDepth = 0.0;
        return;
    }
    if (convolutionUsesMedian(convolution)) {
        for (int a = 0; a < 9; ++a) {
            if (a >= medianCount) break;
            for (int b = a + 1; b < 9; ++b) {
                if (b >= medianCount) break;
                if (medianValues[b] < medianValues[a]) {
                    float swapValue = medianValues[a];
                    medianValues[a] = medianValues[b];
                    medianValues[b] = swapValue;
                }
            }
        }
        outValid = 1.0;
        outDepth = applyZTransform(medianValues[medianCount / 2]);
        return;
    }
    outValid = 1.0;
    outDepth = applyZTransform(
        convolutionUsesWeighted(convolution) ? (weightedTotal / weightTotal) : extremeDepth);
}
)NGGLSL";
}
