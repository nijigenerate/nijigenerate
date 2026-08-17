module nijigenerate.io.depthsample;

import std.algorithm : sort;
import std.algorithm.comparison : max, min;
import std.math : exp, round;

enum DepthSampleChannel {
    AverageRGB,
    R,
    G,
    B,
    Luminance,
}

enum DepthSampleConvolution {
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

struct DepthSampleAggregate {
    bool valid;
    float value;
    float weight;
}

struct DepthSampleResult {
    bool valid;
    float value;
}

struct DepthSamplePoint {
    bool valid;
    float value;
    float weight;
}

DepthSampleResult ngDepthSampleFrontmost(const(DepthSampleResult)[] samples) {
    DepthSampleResult best;
    foreach (sample; samples) {
        if (!sample.valid) continue;
        if (!best.valid || sample.value > best.value) best = sample;
    }
    return best;
}

struct DepthSampleWeightedAccumulator {
    float total = 0.0f;
    float weightTotal = 0.0f;

    void add(float value, float weight) {
        if (weight <= 0.0f) return;
        total += value * weight;
        weightTotal += weight;
    }

    DepthSampleAggregate result() const {
        if (weightTotal <= 0.0f) return DepthSampleAggregate(false, 0.0f, 0.0f);
        return DepthSampleAggregate(true, total / weightTotal, weightTotal);
    }
}

struct DepthSampleExtremeAccumulator {
    bool valid;
    float value;
    bool frontmost;

    void add(float nextValue) {
        if (!valid || (frontmost ? nextValue > value : nextValue < value)) {
            valid = true;
            value = nextValue;
        }
    }

    DepthSampleAggregate result() const {
        if (!valid) return DepthSampleAggregate(false, 0.0f, 0.0f);
        return DepthSampleAggregate(true, value, 1.0f);
    }
}

float ngDepthSampleLerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float ngDepthSamplePixelDepth01(const(ubyte)[] data, size_t index, DepthSampleChannel channel, bool invert) {
    auto r = cast(float)data[index + 0];
    auto g = cast(float)data[index + 1];
    auto b = cast(float)data[index + 2];
    float depth01 = 0.0f;
    final switch (channel) {
        case DepthSampleChannel.AverageRGB:
            depth01 = (r + g + b) / (255.0f * 3.0f);
            break;
        case DepthSampleChannel.R:
            depth01 = r / 255.0f;
            break;
        case DepthSampleChannel.G:
            depth01 = g / 255.0f;
            break;
        case DepthSampleChannel.B:
            depth01 = b / 255.0f;
            break;
        case DepthSampleChannel.Luminance:
            depth01 = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
            break;
    }
    return invert ? 1.0f - depth01 : depth01;
}

float ngDepthSamplePixelDepth(
    const(ubyte)[] data,
    size_t index,
    DepthSampleChannel channel,
    bool invert,
    float backDepth,
    float frontDepth,
    float depthScale
) {
    return ngDepthSampleLerp(backDepth, frontDepth,
        ngDepthSamplePixelDepth01(data, index, channel, invert)) * depthScale;
}

float ngDepthSampleValueToDepth01(float value, float backDepth, float frontDepth, float depthScale) {
    if (depthScale == 0.0f || frontDepth == backDepth) return 0.0f;
    auto unscaled = value / depthScale;
    return max(0.0f, min(1.0f, (unscaled - backDepth) / (frontDepth - backDepth)));
}

float ngDepthSampleOpacity01(ubyte opacity) {
    return cast(float)opacity / 255.0f;
}

float ngDepthSampleEffectiveAlpha(const(ubyte)[] rgba, size_t index, float opacity) {
    if (index + 3 >= rgba.length) return 0.0f;
    return (cast(float)rgba[index + 3] / 255.0f) * opacity;
}

bool ngDepthSampleAcceptsAlpha(float alpha, float threshold) {
    return alpha > threshold;
}

DepthSamplePoint ngDepthSampleMissingPoint() {
    return DepthSamplePoint(false, 0.0f, 0.0f);
}

ubyte ngDepthSampleAlphaByte(float alpha) {
    return cast(ubyte)round(max(0.0f, min(1.0f, alpha)) * 255.0f);
}

int ngDepthSampleConvolutionRadius(DepthSampleConvolution convolution, int customRadius) {
    final switch (convolution) {
        case DepthSampleConvolution.Nearest:
            return 0;
        case DepthSampleConvolution.Box3x3:
        case DepthSampleConvolution.Gaussian3x3:
        case DepthSampleConvolution.Median3x3:
        case DepthSampleConvolution.Frontmost3x3:
        case DepthSampleConvolution.Backmost3x3:
            return 1;
        case DepthSampleConvolution.Box5x5:
        case DepthSampleConvolution.Gaussian5x5:
            return 2;
        case DepthSampleConvolution.BoxCustom:
        case DepthSampleConvolution.GaussianCustom:
        case DepthSampleConvolution.MedianCustom:
        case DepthSampleConvolution.FrontmostCustom:
        case DepthSampleConvolution.BackmostCustom:
            return max(1, customRadius);
    }
}

float ngDepthSampleKernelWeight(DepthSampleConvolution convolution, int customRadius, int dx, int dy) {
    final switch (convolution) {
        case DepthSampleConvolution.Box3x3:
        case DepthSampleConvolution.Box5x5:
        case DepthSampleConvolution.BoxCustom:
            return 1.0f;
        case DepthSampleConvolution.Gaussian3x3:
            return cast(float)((2 - iabs(dx)) * (2 - iabs(dy)));
        case DepthSampleConvolution.Gaussian5x5:
            return cast(float)(pascal5(dx) * pascal5(dy));
        case DepthSampleConvolution.GaussianCustom:
            auto radius = customRadius > 0 ? customRadius : 1;
            auto sigma = max(1.0f, cast(float)radius / 2.0f);
            auto distance2 = cast(float)(dx * dx + dy * dy);
            return cast(float)exp(-distance2 / (2.0f * sigma * sigma));
        case DepthSampleConvolution.Nearest:
        case DepthSampleConvolution.Median3x3:
        case DepthSampleConvolution.Frontmost3x3:
        case DepthSampleConvolution.Backmost3x3:
        case DepthSampleConvolution.MedianCustom:
        case DepthSampleConvolution.FrontmostCustom:
        case DepthSampleConvolution.BackmostCustom:
            return 1.0f;
    }
}

bool ngDepthSampleConvolutionUsesWeightedAverage(DepthSampleConvolution convolution) {
    final switch (convolution) {
        case DepthSampleConvolution.Box3x3:
        case DepthSampleConvolution.Box5x5:
        case DepthSampleConvolution.Gaussian3x3:
        case DepthSampleConvolution.Gaussian5x5:
        case DepthSampleConvolution.BoxCustom:
        case DepthSampleConvolution.GaussianCustom:
            return true;
        case DepthSampleConvolution.Nearest:
        case DepthSampleConvolution.Median3x3:
        case DepthSampleConvolution.Frontmost3x3:
        case DepthSampleConvolution.Backmost3x3:
        case DepthSampleConvolution.MedianCustom:
        case DepthSampleConvolution.FrontmostCustom:
        case DepthSampleConvolution.BackmostCustom:
            return false;
    }
}

bool ngDepthSampleConvolutionUsesMedian(DepthSampleConvolution convolution) {
    final switch (convolution) {
        case DepthSampleConvolution.Median3x3:
        case DepthSampleConvolution.MedianCustom:
            return true;
        case DepthSampleConvolution.Nearest:
        case DepthSampleConvolution.Box3x3:
        case DepthSampleConvolution.Box5x5:
        case DepthSampleConvolution.Gaussian3x3:
        case DepthSampleConvolution.Gaussian5x5:
        case DepthSampleConvolution.Frontmost3x3:
        case DepthSampleConvolution.Backmost3x3:
        case DepthSampleConvolution.BoxCustom:
        case DepthSampleConvolution.GaussianCustom:
        case DepthSampleConvolution.FrontmostCustom:
        case DepthSampleConvolution.BackmostCustom:
            return false;
    }
}

bool ngDepthSampleConvolutionUsesFrontmost(DepthSampleConvolution convolution) {
    final switch (convolution) {
        case DepthSampleConvolution.Frontmost3x3:
        case DepthSampleConvolution.FrontmostCustom:
            return true;
        case DepthSampleConvolution.Nearest:
        case DepthSampleConvolution.Box3x3:
        case DepthSampleConvolution.Box5x5:
        case DepthSampleConvolution.Gaussian3x3:
        case DepthSampleConvolution.Gaussian5x5:
        case DepthSampleConvolution.Median3x3:
        case DepthSampleConvolution.Backmost3x3:
        case DepthSampleConvolution.BoxCustom:
        case DepthSampleConvolution.GaussianCustom:
        case DepthSampleConvolution.MedianCustom:
        case DepthSampleConvolution.BackmostCustom:
            return false;
    }
}

bool ngDepthSampleConvolutionUsesBackmost(DepthSampleConvolution convolution) {
    final switch (convolution) {
        case DepthSampleConvolution.Backmost3x3:
        case DepthSampleConvolution.BackmostCustom:
            return true;
        case DepthSampleConvolution.Nearest:
        case DepthSampleConvolution.Box3x3:
        case DepthSampleConvolution.Box5x5:
        case DepthSampleConvolution.Gaussian3x3:
        case DepthSampleConvolution.Gaussian5x5:
        case DepthSampleConvolution.Median3x3:
        case DepthSampleConvolution.Frontmost3x3:
        case DepthSampleConvolution.BoxCustom:
        case DepthSampleConvolution.GaussianCustom:
        case DepthSampleConvolution.MedianCustom:
        case DepthSampleConvolution.FrontmostCustom:
            return false;
    }
}

DepthSampleAggregate ngDepthSampleWeightedAverage(const(float)[] values, const(float)[] weights) {
    auto length = min(values.length, weights.length);
    DepthSampleWeightedAccumulator accumulator;
    foreach (i; 0 .. length) {
        accumulator.add(values[i], weights[i]);
    }
    return accumulator.result();
}

DepthSampleAggregate ngDepthSampleMedian(const(float)[] values) {
    if (values.length == 0) return DepthSampleAggregate(false, 0.0f, 0.0f);
    auto sorted = values.dup;
    sorted.sort();
    return DepthSampleAggregate(true, sorted[sorted.length / 2], 1.0f);
}

DepthSampleAggregate ngDepthSampleExtreme(const(float)[] values, bool frontmost) {
    DepthSampleExtremeAccumulator accumulator;
    accumulator.frontmost = frontmost;
    foreach (value; values) accumulator.add(value);
    return accumulator.result();
}

DepthSampleAggregate ngDepthSampleConvolve(alias sampleAt)(
    DepthSampleConvolution convolution,
    int customRadius,
    int centerX,
    int centerY
) {
    auto radius = ngDepthSampleConvolutionRadius(convolution, customRadius);
    if (convolution == DepthSampleConvolution.Nearest) {
        auto sample = sampleAt(centerX, centerY);
        return sample.valid
            ? DepthSampleAggregate(true, sample.value, sample.weight)
            : DepthSampleAggregate(false, 0.0f, 0.0f);
    }

    if (ngDepthSampleConvolutionUsesWeightedAverage(convolution)) {
        DepthSampleWeightedAccumulator accumulator;
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                auto sample = sampleAt(centerX + dx, centerY + dy);
                if (!sample.valid) continue;
                auto weight = ngDepthSampleKernelWeight(convolution, customRadius, dx, dy) * sample.weight;
                accumulator.add(sample.value, weight);
            }
        }
        return accumulator.result();
    }

    if (ngDepthSampleConvolutionUsesMedian(convolution)) {
        float[] values;
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                auto sample = sampleAt(centerX + dx, centerY + dy);
                if (sample.valid) values ~= sample.value;
            }
        }
        return ngDepthSampleMedian(values);
    }

    if (ngDepthSampleConvolutionUsesFrontmost(convolution) || ngDepthSampleConvolutionUsesBackmost(convolution)) {
        DepthSampleExtremeAccumulator accumulator;
        accumulator.frontmost = ngDepthSampleConvolutionUsesFrontmost(convolution);
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                auto sample = sampleAt(centerX + dx, centerY + dy);
                if (sample.valid) accumulator.add(sample.value);
            }
        }
        return accumulator.result();
    }

    return DepthSampleAggregate(false, 0.0f, 0.0f);
}

private int iabs(int value) {
    return value < 0 ? -value : value;
}

private int pascal5(int d) {
    final switch (iabs(d)) {
        case 0: return 6;
        case 1: return 4;
        case 2: return 1;
    }
    return 0;
}
