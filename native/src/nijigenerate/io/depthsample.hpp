#pragma once
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <tuple>

namespace nijigenerate::io::depthsample {
namespace detail {
static_assert(sizeof(int) == sizeof(std::uint32_t));
inline int add32(int a, int b) {
    return std::bit_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
inline int sub32(int a, int b) {
    return std::bit_cast<int>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
inline int mul32(int a, int b) {
    return std::bit_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
}
enum class DepthSampleChannel : std::int32_t { AverageRGB, R, G, B, Luminance };
enum class DepthSampleConvolution : std::int32_t {
    Nearest, Box3x3, Box5x5, Gaussian3x3, Gaussian5x5, Median3x3, Frontmost3x3,
    Backmost3x3, BoxCustom, GaussianCustom, MedianCustom, FrontmostCustom, BackmostCustom
};
struct DepthSampleAggregate {
    bool valid = false;
    float value = std::numeric_limits<float>::quiet_NaN();
    float weight = std::numeric_limits<float>::quiet_NaN();
};
struct DepthSampleResult {
    bool valid = false;
    float value = std::numeric_limits<float>::quiet_NaN();
};
struct DepthSamplePoint {
    bool valid = false;
    float value = std::numeric_limits<float>::quiet_NaN();
    float weight = std::numeric_limits<float>::quiet_NaN();
};
DepthSampleResult ngDepthSampleFrontmost(std::span<const DepthSampleResult> samples);
struct DepthSampleWeightedAccumulator {
    float total = 0.0f;
    float weightTotal = 0.0f;
    void add(float value, float weight);
    DepthSampleAggregate result() const;
};
struct DepthSampleExtremeAccumulator {
    bool valid = false;
    float value = std::numeric_limits<float>::quiet_NaN();
    bool frontmost = false;
    void add(float nextValue);
    DepthSampleAggregate result() const;
};
float ngDepthSampleLerp(float a, float b, float t);
float ngDepthSamplePixelDepth01(std::span<const std::uint8_t> data, std::size_t index,
    DepthSampleChannel channel, bool invert);
float ngDepthSamplePixelDepth(std::span<const std::uint8_t> data, std::size_t index,
    DepthSampleChannel channel, bool invert, float backDepth, float frontDepth, float depthScale);
float ngDepthSampleValueToDepth01(float value, float backDepth, float frontDepth, float depthScale);
float ngDepthSampleOpacity01(std::uint8_t opacity);
float ngDepthSampleEffectiveAlpha(std::span<const std::uint8_t> rgba, std::size_t index, float opacity);
bool ngDepthSampleAcceptsAlpha(float alpha, float threshold);
DepthSamplePoint ngDepthSampleMissingPoint();
std::uint8_t ngDepthSampleAlphaByte(float alpha);
int ngDepthSampleConvolutionRadius(DepthSampleConvolution convolution, int customRadius);
float ngDepthSampleKernelWeight(DepthSampleConvolution convolution, int customRadius, int dx, int dy);
bool ngDepthSampleConvolutionUsesWeightedAverage(DepthSampleConvolution convolution);
bool ngDepthSampleConvolutionUsesMedian(DepthSampleConvolution convolution);
bool ngDepthSampleConvolutionUsesFrontmost(DepthSampleConvolution convolution);
bool ngDepthSampleConvolutionUsesBackmost(DepthSampleConvolution convolution);
DepthSampleAggregate ngDepthSampleWeightedAverage(std::span<const float> values, std::span<const float> weights);
DepthSampleAggregate ngDepthSampleMedian(std::span<const float> values);
DepthSampleAggregate ngDepthSampleExtreme(std::span<const float> values, bool frontmost);

template<class SampleAt>
DepthSampleAggregate ngDepthSampleConvolve(SampleAt&& sampleAt, DepthSampleConvolution convolution,
    int customRadius, int centerX, int centerY) {
    auto radius = ngDepthSampleConvolutionRadius(convolution, customRadius);
    if (convolution == DepthSampleConvolution::Nearest) {
        auto sample = sampleAt(centerX, centerY);
        return sample.valid ? DepthSampleAggregate{true, sample.value, sample.weight}
                            : DepthSampleAggregate{false, 0.0f, 0.0f};
    }
    if (ngDepthSampleConvolutionUsesWeightedAverage(convolution)) {
        DepthSampleWeightedAccumulator accumulator;
        for (int dy = -radius; dy <= radius; dy = detail::add32(dy, 1)) {
            for (int dx = -radius; dx <= radius; dx = detail::add32(dx, 1)) {
                auto sample = sampleAt(detail::add32(centerX, dx), detail::add32(centerY, dy));
                if (!sample.valid) continue;
                auto weight = ngDepthSampleKernelWeight(convolution, customRadius, dx, dy) * sample.weight;
                accumulator.add(sample.value, weight);
            }
        }
        return accumulator.result();
    }
    if (ngDepthSampleConvolutionUsesMedian(convolution)) {
        compat::Slice<float> values;
        for (int dy = -radius; dy <= radius; dy = detail::add32(dy, 1)) {
            for (int dx = -radius; dx <= radius; dx = detail::add32(dx, 1)) {
                auto sample = sampleAt(detail::add32(centerX, dx), detail::add32(centerY, dy));
                if (sample.valid) values.append(sample.value);
            }
        }
        return ngDepthSampleMedian(std::span<const float>{values.data(), values.size()});
    }
    if (ngDepthSampleConvolutionUsesFrontmost(convolution) || ngDepthSampleConvolutionUsesBackmost(convolution)) {
        DepthSampleExtremeAccumulator accumulator;
        accumulator.frontmost = ngDepthSampleConvolutionUsesFrontmost(convolution);
        for (int dy = -radius; dy <= radius; dy = detail::add32(dy, 1)) {
            for (int dx = -radius; dx <= radius; dx = detail::add32(dx, 1)) {
                auto sample = sampleAt(detail::add32(centerX, dx), detail::add32(centerY, dy));
                if (sample.valid) accumulator.add(sample.value);
            }
        }
        return accumulator.result();
    }
    return {false, 0.0f, 0.0f};
}
}

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::io::depthsample::DepthSampleAggregate> {
    using Value = ::nijigenerate::io::depthsample::DepthSampleAggregate;
    inline static constexpr std::string_view typeName = "DepthSampleAggregate";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("valid", offsetof(Value, valid), &Value::valid),
            ngFormatAggregateMember("value", offsetof(Value, value), &Value::value),
            ngFormatAggregateMember("weight", offsetof(Value, weight), &Value::weight)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::io::depthsample::DepthSampleResult> {
    using Value = ::nijigenerate::io::depthsample::DepthSampleResult;
    inline static constexpr std::string_view typeName = "DepthSampleResult";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("valid", offsetof(Value, valid), &Value::valid),
            ngFormatAggregateMember("value", offsetof(Value, value), &Value::value)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::io::depthsample::DepthSamplePoint> {
    using Value = ::nijigenerate::io::depthsample::DepthSamplePoint;
    inline static constexpr std::string_view typeName = "DepthSamplePoint";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("valid", offsetof(Value, valid), &Value::valid),
            ngFormatAggregateMember("value", offsetof(Value, value), &Value::value),
            ngFormatAggregateMember("weight", offsetof(Value, weight), &Value::weight)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::io::depthsample::DepthSampleWeightedAccumulator> {
    using Value = ::nijigenerate::io::depthsample::DepthSampleWeightedAccumulator;
    inline static constexpr std::string_view typeName = "DepthSampleWeightedAccumulator";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("total", offsetof(Value, total), &Value::total),
            ngFormatAggregateMember("weightTotal", offsetof(Value, weightTotal), &Value::weightTotal)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::io::depthsample::DepthSampleExtremeAccumulator> {
    using Value = ::nijigenerate::io::depthsample::DepthSampleExtremeAccumulator;
    inline static constexpr std::string_view typeName = "DepthSampleExtremeAccumulator";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("valid", offsetof(Value, valid), &Value::valid),
            ngFormatAggregateMember("value", offsetof(Value, value), &Value::value),
            ngFormatAggregateMember("frontmost", offsetof(Value, frontmost), &Value::frontmost)};
    }
};

template<> struct FormatEnumTraits<::nijigenerate::io::depthsample::DepthSampleChannel> {
    using Value = ::nijigenerate::io::depthsample::DepthSampleChannel;
    inline static constexpr std::string_view name = "DepthSampleChannel";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::AverageRGB, "AverageRGB"},
        FormatEnumMember<Value>{Value::R, "R"},
        FormatEnumMember<Value>{Value::G, "G"},
        FormatEnumMember<Value>{Value::B, "B"},
        FormatEnumMember<Value>{Value::Luminance, "Luminance"}};
};
template<> struct FormatEnumTraits<::nijigenerate::io::depthsample::DepthSampleConvolution> {
    using Value = ::nijigenerate::io::depthsample::DepthSampleConvolution;
    inline static constexpr std::string_view name = "DepthSampleConvolution";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::Nearest, "Nearest"},
        FormatEnumMember<Value>{Value::Box3x3, "Box3x3"},
        FormatEnumMember<Value>{Value::Box5x5, "Box5x5"},
        FormatEnumMember<Value>{Value::Gaussian3x3, "Gaussian3x3"},
        FormatEnumMember<Value>{Value::Gaussian5x5, "Gaussian5x5"},
        FormatEnumMember<Value>{Value::Median3x3, "Median3x3"},
        FormatEnumMember<Value>{Value::Frontmost3x3, "Frontmost3x3"},
        FormatEnumMember<Value>{Value::Backmost3x3, "Backmost3x3"},
        FormatEnumMember<Value>{Value::BoxCustom, "BoxCustom"},
        FormatEnumMember<Value>{Value::GaussianCustom, "GaussianCustom"},
        FormatEnumMember<Value>{Value::MedianCustom, "MedianCustom"},
        FormatEnumMember<Value>{Value::FrontmostCustom, "FrontmostCustom"},
        FormatEnumMember<Value>{Value::BackmostCustom, "BackmostCustom"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::io::depthsample::DepthSampleChannel> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.io.depthsample.DepthSampleChannel)"),
            RetainedString::fromStaticLiteral("(DepthSampleChannel)"),
            RetainedString::fromStaticLiteral("nijigenerate.io.depthsample.DepthSampleChannel"),
            RetainedString::fromStaticLiteral("DepthSampleChannel")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::io::depthsample::DepthSampleConvolution> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.io.depthsample.DepthSampleConvolution)"),
            RetainedString::fromStaticLiteral("(DepthSampleConvolution)"),
            RetainedString::fromStaticLiteral("nijigenerate.io.depthsample.DepthSampleConvolution"),
            RetainedString::fromStaticLiteral("DepthSampleConvolution")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::io::depthsample::DepthSampleAggregate> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.io.depthsample.DepthSampleAggregate)"),
            RetainedString::fromStaticLiteral("(DepthSampleAggregate)"),
            RetainedString::fromStaticLiteral("nijigenerate.io.depthsample.DepthSampleAggregate"),
            RetainedString::fromStaticLiteral("DepthSampleAggregate")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::io::depthsample::DepthSampleResult> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.io.depthsample.DepthSampleResult)"),
            RetainedString::fromStaticLiteral("(DepthSampleResult)"),
            RetainedString::fromStaticLiteral("nijigenerate.io.depthsample.DepthSampleResult"),
            RetainedString::fromStaticLiteral("DepthSampleResult")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::io::depthsample::DepthSamplePoint> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.io.depthsample.DepthSamplePoint)"),
            RetainedString::fromStaticLiteral("(DepthSamplePoint)"),
            RetainedString::fromStaticLiteral("nijigenerate.io.depthsample.DepthSamplePoint"),
            RetainedString::fromStaticLiteral("DepthSamplePoint")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::io::depthsample::DepthSampleWeightedAccumulator> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.io.depthsample.DepthSampleWeightedAccumulator)"),
            RetainedString::fromStaticLiteral("(DepthSampleWeightedAccumulator)"),
            RetainedString::fromStaticLiteral("nijigenerate.io.depthsample.DepthSampleWeightedAccumulator"),
            RetainedString::fromStaticLiteral("DepthSampleWeightedAccumulator")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::io::depthsample::DepthSampleExtremeAccumulator> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.io.depthsample.DepthSampleExtremeAccumulator)"),
            RetainedString::fromStaticLiteral("(DepthSampleExtremeAccumulator)"),
            RetainedString::fromStaticLiteral("nijigenerate.io.depthsample.DepthSampleExtremeAccumulator"),
            RetainedString::fromStaticLiteral("DepthSampleExtremeAccumulator")};
    }
};
} // namespace nicxlive::core::d_runtime
