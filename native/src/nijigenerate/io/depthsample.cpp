#include "depthsample.hpp"
#include <nijigenerate/compat/phobos_float_sort.hpp>
#include <nijigenerate/compat/phobos_exp_float.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <algorithm>
#include <cmath>
#include <new>
#include <stdexcept>
#include <vector>

namespace nijigenerate::io::depthsample {
namespace {
int iabs(int value) { return value < 0 ? detail::sub32(0, value) : value; }
int pascal5(int d) {
    switch (iabs(d)) {
        case 0: return 6;
        case 1: return 4;
        case 2: return 1;
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthsample.d"), 369);
}
[[noreturn]] void invalidConvolution(std::size_t sourceLine) {
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthsample.d"), sourceLine);
}
}
DepthSampleResult ngDepthSampleFrontmost(std::span<const DepthSampleResult> samples) {
    DepthSampleResult best;
    for (auto sample : samples) {
        if (!sample.valid) continue;
        if (!best.valid || sample.value > best.value) best = sample;
    }
    return best;
}
void DepthSampleWeightedAccumulator::add(float value, float weight) {
    if (weight <= 0.0f) return;
    total += value * weight;
    weightTotal += weight;
}
DepthSampleAggregate DepthSampleWeightedAccumulator::result() const {
    if (weightTotal <= 0.0f) return {false, 0.0f, 0.0f};
    return {true, total / weightTotal, weightTotal};
}
void DepthSampleExtremeAccumulator::add(float nextValue) {
    if (!valid || (frontmost ? nextValue > value : nextValue < value)) {
        valid = true;
        value = nextValue;
    }
}
DepthSampleAggregate DepthSampleExtremeAccumulator::result() const {
    if (!valid) return {false, 0.0f, 0.0f};
    return {true, value, 1.0f};
}
float ngDepthSampleLerp(float a, float b, float t) { return a + (b - a) * t; }
float ngDepthSamplePixelDepth01(std::span<const std::uint8_t> data, std::size_t index,
    DepthSampleChannel channel, bool invert) {
    // Keep D bounds failures instead of unchecked C++ span indexing.
    auto byte = [&](std::size_t i, std::size_t sourceLine) {
        if (i >= data.size()) throw compat::ngStaticError<compat::ArrayIndexError>(i, data.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthsample.d"), sourceLine);
        return static_cast<float>(data[i]);
    };
    auto r = byte(index + 0, 96);
    auto g = byte(index + 1, 97);
    auto b = byte(index + 2, 98);
    float depth01 = 0.0f;
    switch (channel) {
        case DepthSampleChannel::AverageRGB: depth01 = (r + g + b) / (255.0f * 3.0f); break;
        case DepthSampleChannel::R: depth01 = r / 255.0f; break;
        case DepthSampleChannel::G: depth01 = g / 255.0f; break;
        case DepthSampleChannel::B: depth01 = b / 255.0f; break;
        case DepthSampleChannel::Luminance: depth01 = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f; break;
        default: throw compat::ngStaticError<compat::SwitchError>(
            compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthsample.d"), 100);
    }
    return invert ? 1.0f - depth01 : depth01;
}
float ngDepthSamplePixelDepth(std::span<const std::uint8_t> data, std::size_t index,
    DepthSampleChannel channel, bool invert, float backDepth, float frontDepth, float depthScale) {
    return ngDepthSampleLerp(backDepth, frontDepth, ngDepthSamplePixelDepth01(data, index, channel, invert)) * depthScale;
}
float ngDepthSampleValueToDepth01(float value, float backDepth, float frontDepth, float depthScale) {
    if (depthScale == 0.0f || frontDepth == backDepth) return 0.0f;
    auto unscaled = value / depthScale;
    return std::max(0.0f, std::min(1.0f, (unscaled - backDepth) / (frontDepth - backDepth)));
}
float ngDepthSampleOpacity01(std::uint8_t opacity) { return static_cast<float>(opacity) / 255.0f; }
float ngDepthSampleEffectiveAlpha(std::span<const std::uint8_t> rgba, std::size_t index, float opacity) {
    if (index + 3 >= rgba.size()) return 0.0f;
    return (static_cast<float>(rgba[index + 3]) / 255.0f) * opacity;
}
bool ngDepthSampleAcceptsAlpha(float alpha, float threshold) { return alpha > threshold; }
DepthSamplePoint ngDepthSampleMissingPoint() { return {false, 0.0f, 0.0f}; }
std::uint8_t ngDepthSampleAlphaByte(float alpha) {
    return static_cast<std::uint8_t>(std::round(std::max(0.0f, std::min(1.0f, alpha)) * 255.0f));
}
int ngDepthSampleConvolutionRadius(DepthSampleConvolution convolution, int customRadius) {
    switch (convolution) {
        case DepthSampleConvolution::Nearest: return 0;
        case DepthSampleConvolution::Box3x3:
        case DepthSampleConvolution::Gaussian3x3:
        case DepthSampleConvolution::Median3x3:
        case DepthSampleConvolution::Frontmost3x3:
        case DepthSampleConvolution::Backmost3x3: return 1;
        case DepthSampleConvolution::Box5x5:
        case DepthSampleConvolution::Gaussian5x5: return 2;
        case DepthSampleConvolution::BoxCustom:
        case DepthSampleConvolution::GaussianCustom:
        case DepthSampleConvolution::MedianCustom:
        case DepthSampleConvolution::FrontmostCustom:
        case DepthSampleConvolution::BackmostCustom: return std::max(1, customRadius);
    }
    invalidConvolution(161);
}
float ngDepthSampleKernelWeight(DepthSampleConvolution convolution, int customRadius, int dx, int dy) {
    switch (convolution) {
        case DepthSampleConvolution::Box3x3:
        case DepthSampleConvolution::Box5x5:
        case DepthSampleConvolution::BoxCustom: return 1.0f;
        case DepthSampleConvolution::Gaussian3x3:
            return static_cast<float>(detail::mul32(detail::sub32(2, iabs(dx)), detail::sub32(2, iabs(dy))));
        case DepthSampleConvolution::Gaussian5x5: return static_cast<float>(pascal5(dx) * pascal5(dy));
        case DepthSampleConvolution::GaussianCustom: {
            auto radius = customRadius > 0 ? customRadius : 1;
            auto sigma = std::max(1.0f, static_cast<float>(radius) / 2.0f);
            auto distance2 = static_cast<float>(detail::add32(detail::mul32(dx, dx), detail::mul32(dy, dy)));
            return compat::phobos_exp_float::exp(-distance2 / (2.0f * sigma * sigma));
        }
        case DepthSampleConvolution::Nearest:
        case DepthSampleConvolution::Median3x3:
        case DepthSampleConvolution::Frontmost3x3:
        case DepthSampleConvolution::Backmost3x3:
        case DepthSampleConvolution::MedianCustom:
        case DepthSampleConvolution::FrontmostCustom:
        case DepthSampleConvolution::BackmostCustom: return 1.0f;
    }
    invalidConvolution(183);
}
bool ngDepthSampleConvolutionUsesWeightedAverage(DepthSampleConvolution convolution) {
    switch (convolution) {
        case DepthSampleConvolution::Box3x3:
        case DepthSampleConvolution::Box5x5:
        case DepthSampleConvolution::Gaussian3x3:
        case DepthSampleConvolution::Gaussian5x5:
        case DepthSampleConvolution::BoxCustom:
        case DepthSampleConvolution::GaussianCustom: return true;
        case DepthSampleConvolution::Nearest:
        case DepthSampleConvolution::Median3x3:
        case DepthSampleConvolution::Frontmost3x3:
        case DepthSampleConvolution::Backmost3x3:
        case DepthSampleConvolution::MedianCustom:
        case DepthSampleConvolution::FrontmostCustom:
        case DepthSampleConvolution::BackmostCustom: return false;
    }
    invalidConvolution(209);
}
bool ngDepthSampleConvolutionUsesMedian(DepthSampleConvolution convolution) {
    switch (convolution) {
        case DepthSampleConvolution::Median3x3:
        case DepthSampleConvolution::MedianCustom: return true;
        case DepthSampleConvolution::Nearest:
        case DepthSampleConvolution::Box3x3:
        case DepthSampleConvolution::Box5x5:
        case DepthSampleConvolution::Gaussian3x3:
        case DepthSampleConvolution::Gaussian5x5:
        case DepthSampleConvolution::Frontmost3x3:
        case DepthSampleConvolution::Backmost3x3:
        case DepthSampleConvolution::BoxCustom:
        case DepthSampleConvolution::GaussianCustom:
        case DepthSampleConvolution::FrontmostCustom:
        case DepthSampleConvolution::BackmostCustom: return false;
    }
    invalidConvolution(229);
}
bool ngDepthSampleConvolutionUsesFrontmost(DepthSampleConvolution convolution) {
    switch (convolution) {
        case DepthSampleConvolution::Frontmost3x3:
        case DepthSampleConvolution::FrontmostCustom: return true;
        case DepthSampleConvolution::Nearest:
        case DepthSampleConvolution::Box3x3:
        case DepthSampleConvolution::Box5x5:
        case DepthSampleConvolution::Gaussian3x3:
        case DepthSampleConvolution::Gaussian5x5:
        case DepthSampleConvolution::Median3x3:
        case DepthSampleConvolution::Backmost3x3:
        case DepthSampleConvolution::BoxCustom:
        case DepthSampleConvolution::GaussianCustom:
        case DepthSampleConvolution::MedianCustom:
        case DepthSampleConvolution::BackmostCustom: return false;
    }
    invalidConvolution(249);
}
bool ngDepthSampleConvolutionUsesBackmost(DepthSampleConvolution convolution) {
    switch (convolution) {
        case DepthSampleConvolution::Backmost3x3:
        case DepthSampleConvolution::BackmostCustom: return true;
        case DepthSampleConvolution::Nearest:
        case DepthSampleConvolution::Box3x3:
        case DepthSampleConvolution::Box5x5:
        case DepthSampleConvolution::Gaussian3x3:
        case DepthSampleConvolution::Gaussian5x5:
        case DepthSampleConvolution::Median3x3:
        case DepthSampleConvolution::Frontmost3x3:
        case DepthSampleConvolution::BoxCustom:
        case DepthSampleConvolution::GaussianCustom:
        case DepthSampleConvolution::MedianCustom:
        case DepthSampleConvolution::FrontmostCustom: return false;
    }
    invalidConvolution(269);
}
DepthSampleAggregate ngDepthSampleWeightedAverage(std::span<const float> values, std::span<const float> weights) {
    auto length = std::min(values.size(), weights.size());
    DepthSampleWeightedAccumulator accumulator;
    for (std::size_t i = 0; i < length; ++i) accumulator.add(values[i], weights[i]);
    return accumulator.result();
}
DepthSampleAggregate ngDepthSampleMedian(std::span<const float> values) {
    if (values.empty()) return {false, 0.0f, 0.0f};
    const auto duplicate = [&] {
        try {
            return std::vector<float>(values.begin(), values.end());
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    };
    auto sorted = duplicate();
    compat::phobos_float_sort::sort(sorted);
    return {true, sorted[sorted.size() / 2], 1.0f};
}
DepthSampleAggregate ngDepthSampleExtreme(std::span<const float> values, bool frontmost) {
    DepthSampleExtremeAccumulator accumulator;
    accumulator.frontmost = frontmost;
    for (auto value : values) accumulator.add(value);
    return accumulator.result();
}
}
