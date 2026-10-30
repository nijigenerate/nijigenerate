#pragma once

#include <nijigenerate/ext/nodes/exdepthnumeric.hpp>
#include <math/veca.hpp>
#include <nodes/grid_axis_sort.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>

#include <algorithm>
#include <cmath>
#include <new>
#include <source_location>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nijigenerate::ext::nodes {
namespace depth_detail {
inline constexpr float DepthGridAxisTolerance = 1e-4f;

template<class Create> decltype(auto) allocateDepthArray(Create&& create) {
    try { return std::forward<Create>(create)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline bool sameDepthGridAxisValue(float a, float b) {
    if (a == b) return true;
    constexpr float infinity = std::numeric_limits<float>::infinity();
    if (a == infinity || a == -infinity) return false;
    if (b == infinity || b == -infinity) return false;
    const float difference = std::fabs(a - b);
    return difference <= DepthGridAxisTolerance * std::fabs(a)
        || difference <= DepthGridAxisTolerance * std::fabs(b)
        || difference <= DepthGridAxisTolerance;
}

inline std::span<float> depthGridAxis(std::span<float> values) {
    if (values.empty()) return values;
    nicxlive::core::nodes::grid_axis_detail::sort(values);
    std::size_t write = 1;
    for (std::size_t i = 1; i < values.size(); ++i) {
        if (!sameDepthGridAxisValue(values[write - 1], values[i])) values[write++] = values[i];
    }
    return values.first(write);
}

inline bool depthGridAxisIndex(std::span<const float> axis, float value, std::size_t& index) {
    index = 0;
    for (std::size_t i = 0; i < axis.size(); ++i) {
        if (!sameDepthGridAxisValue(value, axis[i])) continue;
        index = i;
        return true;
    }
    return false;
}

inline bool locateDepthGridInterval(std::span<const float> axis, float value,
    std::size_t& cell, float& weight) {
    cell = 0;
    weight = std::numeric_limits<float>::quiet_NaN();
    if (axis.size() < 2) return false;
    if (value <= axis[0]) {
        cell = 0;
        const float span = axis[1] - axis[0];
        weight = span > 0.0f ? (value - axis[0]) / span : 0.0f;
        return true;
    }
    if (value >= axis.back()) {
        cell = axis.size() - 2;
        const float span = axis.back() - axis[axis.size() - 2];
        weight = span > 0.0f ? (value - axis[axis.size() - 2]) / span : 1.0f;
        return true;
    }
    for (std::size_t i = 0; i < axis.size() - 1; ++i) {
        const float a = axis[i], b = axis[i + 1];
        if (value < a || value > b) continue;
        const float span = b - a;
        cell = i;
        weight = span > 0.0f ? (value - a) / span : 0.0f;
        return true;
    }
    return false;
}
template<class PublishOutput>
inline bool resampleGridValues(const nicxlive::core::math::Vec2Array& oldVertices,
    std::span<const float> oldDepths, const nicxlive::core::math::Vec2Array& newVertices,
    PublishOutput&& publish) {
    auto fail = [] { return false; };
    if (oldVertices.size() < 4 || oldDepths.size() != oldVertices.size()) return fail();
    auto xValues = allocateDepthArray([&] {
        return std::vector<float>(oldVertices.size(), std::numeric_limits<float>::quiet_NaN());
    });
    auto yValues = allocateDepthArray([&] {
        return std::vector<float>(oldVertices.size(), std::numeric_limits<float>::quiet_NaN());
    });
    const auto axisVertexCount = oldVertices.size();
    for (std::size_t i = 0; i < axisVertexCount; ++i) {
        const nicxlive::core::math::Vec2ViewConst vertex(oldVertices, i);
        xValues[i] = vertex.component(0);
        yValues[i] = vertex.component(1);
    }
    const auto xs = allocateDepthArray([&] { return depthGridAxis(xValues); });
    const auto ys = allocateDepthArray([&] { return depthGridAxis(yValues); });
    const std::size_t cols = xs.size(), rows = ys.size();
    if (cols < 2 || rows < 2 || cols * rows != oldVertices.size()) return fail();
    auto orderedDepths = allocateDepthArray([&] {
        return std::vector<float>(oldDepths.size(), std::numeric_limits<float>::quiet_NaN());
    });
    auto assigned = allocateDepthArray([&] { return std::vector<bool>(oldDepths.size(), false); });
    const auto orderedVertexCount = oldVertices.size();
    for (std::size_t i = 0; i < orderedVertexCount; ++i) {
        const nicxlive::core::math::Vec2ViewConst vertex(oldVertices, i);
        std::size_t x, y;
        if (!depthGridAxisIndex(xs, vertex.component(0), x) ||
            !depthGridAxisIndex(ys, vertex.component(1), y)) return fail();
        const auto index = y * cols + x;
        if (assigned[index]) return fail();
        orderedDepths[index] = oldDepths[i];
        assigned[index] = true;
    }
    for (bool value : assigned) if (!value) return fail();
    // Retain source depths before result replaces a potentially aliased vector.
    auto result = publish(newVertices.size());
    const auto resultVertexCount = newVertices.size();
    for (std::size_t i = 0; i < resultVertexCount; ++i) {
        const nicxlive::core::math::Vec2ViewConst vertex(newVertices, i);
        std::size_t cellX, cellY;
        float u, v;
        if (!locateDepthGridInterval(xs, vertex.component(0), cellX, u)
            || !locateDepthGridInterval(ys, vertex.component(1), cellY, v)) {
            result[i] = 0.0f;
            continue;
        }
        const auto index00 = cellY * cols + cellX, index10 = cellY * cols + cellX + 1;
        const auto index01 = (cellY + 1) * cols + cellX, index11 = (cellY + 1) * cols + cellX + 1;
        const float top = orderedDepths[index00] * (1.0f - u) + orderedDepths[index10] * u;
        const float bottom = orderedDepths[index01] * (1.0f - u) + orderedDepths[index11] * u;
        result[i] = ngFiniteDepthOrZero(top * (1.0f - v) + bottom * v);
    }
    return true;
}
} // namespace depth_detail

inline bool ngResampleGridDepths(nicxlive::core::math::Vec2Array oldVertices,
    std::span<const float> oldDepths, nicxlive::core::math::Vec2Array newVertices,
    std::vector<float>& result) {
    const bool success = depth_detail::resampleGridValues(oldVertices, oldDepths, newVertices,
        [&](std::size_t count) {
            result.clear();
            result.resize(count);
            return std::span<float>(result);
        });
    if (!success) result.clear();
    return success;
}
// Source-facing retained descriptors: out resets immediately while the input
// descriptor independently retains any shared backing from the previous result.
inline bool ngResampleGridDepths(nicxlive::core::math::Vec2Array oldVertices,
    compat::Slice<float> oldDepths,nicxlive::core::math::Vec2Array newVertices,
    compat::Slice<float>& result) {
    result = {};
    return depth_detail::resampleGridValues(oldVertices,
        std::span<const float>(oldDepths.data(), oldDepths.size()), newVertices,
        [&](std::size_t count) {
            try {
                if (count) result = compat::Slice<float>(
                    std::vector<float>(count, std::numeric_limits<float>::quiet_NaN()));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
            return std::span<float>(result.data(), result.size());
        });
}
inline bool ngResamplePathDepths(nicxlive::core::math::Vec2Array oldVertices,
    compat::Slice<float> oldDepths,nicxlive::core::math::Vec2Array newVertices,
    compat::Slice<float>& result) {
    result = {};
    if (oldVertices.size() == 0 || oldDepths.size() != oldVertices.size()) return false;
    try {
        if (newVertices.size()) result = compat::Slice<float>(
            std::vector<float>(newVertices.size(), std::numeric_limits<float>::quiet_NaN()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    depth_detail::resamplePathValues(
        std::span<const float>(oldDepths.data(), oldDepths.size()),
        std::span<float>(result.data(), result.size()));
    return true;
}
} // namespace nijigenerate::ext::nodes
