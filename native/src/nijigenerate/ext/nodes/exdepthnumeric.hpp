#pragma once

#include <limits>
#include <span>
#include <cstddef>
#include <vector>

namespace nijigenerate::ext::nodes {

inline float ngFiniteDepthOrZero(float value) noexcept {
    constexpr float infinity = std::numeric_limits<float>::infinity();
    return value == value && value != infinity && value != -infinity ? value : 0.0f;
}

inline void ngNormalizeDepths(std::span<float> values) noexcept {
    for (float& value : values) value = ngFiniteDepthOrZero(value);
}

namespace depth_detail {
// The caller has validated counts and published the actual Source output array.
// This value kernel neither allocates nor changes an input/output descriptor.
inline void resamplePathValues(std::span<const float> source, std::span<float> result) {
    const auto newVertexCount = result.size();
    if (!newVertexCount) return;
    if (source.size() == 1) {
        const float value = ngFiniteDepthOrZero(source[0]);
        for (float& destination : result) destination = value;
        return;
    }
    for (std::size_t i = 0; i < newVertexCount; ++i) {
        const float normalized = newVertexCount > 1
            ? static_cast<float>(i) / static_cast<float>(newVertexCount - 1) : 0.0f;
        const float oldPosition = normalized * static_cast<float>(source.size() - 1);
        std::size_t segment = static_cast<std::size_t>(oldPosition);
        if (segment >= source.size() - 1) segment = source.size() - 2;
        const float weight = oldPosition - static_cast<float>(segment);
        result[i] = ngFiniteDepthOrZero(
            source[segment] * (1.0f - weight) + source[segment + 1] * weight);
    }
}
} // namespace depth_detail

// The D routine observes vertex-array lengths only; coordinates do not affect
// this interpolation by normalized control-point index.
inline bool ngResamplePathDepths(std::size_t oldVertexCount, std::span<const float> oldDepths,
    std::size_t newVertexCount, std::vector<float>& result) {
    if (oldVertexCount == 0 || oldDepths.size() != oldVertexCount) {
        result.clear();
        return false;
    }
    // Retain source values when the caller supplies a view into result.
    const std::vector<float> source(oldDepths.begin(), oldDepths.end());
    result.clear();
    result.resize(newVertexCount);
    depth_detail::resamplePathValues(source, result);
    return true;
}

} // namespace nijigenerate::ext::nodes
