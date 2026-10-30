#pragma once

#include <math/types.hpp>
#include <concepts>
#include <type_traits>
#include <cstdint>
#include <math/veca.hpp>
#include "mesh_vertices.hpp"
#include <common/shared_array.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/viewport/common/mesheditor/brushes/base.hpp>
#include <cmath>
#include <bit>
#include <limits>
#include <memory>
#include <utility>

namespace nijigenerate::core::math::vertex {

namespace detail {
// D module-private mutable storage is thread-local by default.
inline thread_local float selectRadius = 16.0f;

#if defined(_MSC_VER)
using DistanceReal = double;
#else
using DistanceReal = long double;
#endif

inline float vectorDistance(::nicxlive::core::math::Vec2 value, ::nicxlive::core::math::Vec2 point) {
    const volatile float x = value.x - point.x, y = value.y - point.y;
    DistanceReal squared = 0;
    const volatile float xx = x * x;
    squared += static_cast<DistanceReal>(xx);
    const volatile float yy = y * y;
    squared += static_cast<DistanceReal>(yy);
    return static_cast<float>(std::sqrt(squared));
}

template<class T>
inline ::nicxlive::core::math::Vec2 directPosition(T& value) {
    if constexpr (requires { value.position; }) return value.position;
    else if constexpr (requires { value->position; }) return compat::ngRequireObject(value)->position;
    else if constexpr (requires { value.position(); }) return value.position();
    else return compat::ngRequireObject(value)->position();
}
} // namespace detail

template<class T> requires std::same_as<std::remove_const_t<T>, ::nicxlive::core::math::Vec2>
inline ::nicxlive::core::math::Vec2 position(T& value) {
    return value;
}

template<class T> requires (std::same_as<T, ::nicxlive::core::math::Vec2> ||
    std::same_as<T, ::nicxlive::core::math::Vec2View> ||
    std::same_as<T, ::nicxlive::core::math::Vec2ViewConst>)
inline bool groupIdEquals(T, std::uint32_t) {
    return true;
}

template<class T> requires std::same_as<std::remove_cv_t<T>, mesh::MeshVertex>
inline ::nicxlive::core::math::Vec2 position(T& value) { return value.position; }

template<class T> requires (std::is_pointer_v<T> &&
    std::same_as<std::remove_cv_t<std::remove_pointer_t<T>>, mesh::MeshVertex>)
inline ::nicxlive::core::math::Vec2 position(T& value) { return compat::ngRequireObject(value)->position; }

inline ::nicxlive::core::math::Vec2 position(mesh::MeshVertexSlot value) {
    return compat::ngRequireObject(value.get())->position;
}

template<class T> requires (std::same_as<T, ::nicxlive::core::math::Vec2View> ||
    std::same_as<T, ::nicxlive::core::math::Vec2ViewConst>)
inline ::nicxlive::core::math::Vec2 position(T value) { return value.toVector(); }

template<class T> requires std::same_as<T, mesh::MeshVertex>
inline bool groupIdEquals(T value, std::uint32_t group) { return value.groupId == group; }

template<class T> requires (std::is_pointer_v<T> &&
    std::same_as<std::remove_cv_t<std::remove_pointer_t<T>>, mesh::MeshVertex>)
inline bool groupIdEquals(T value, std::uint32_t group) {
    return compat::ngRequireObject(value)->groupId == group;
}

template<class T> requires (std::same_as<std::remove_cv_t<T>, ::nicxlive::core::math::Vec2> ||
    std::same_as<std::remove_cv_t<T>, ::nicxlive::core::math::Vec2View> ||
    std::same_as<std::remove_cv_t<T>, ::nicxlive::core::math::Vec2ViewConst> ||
    std::same_as<std::remove_cv_t<T>, mesh::MeshVertex> ||
    (std::is_pointer_v<T> && std::same_as<std::remove_cv_t<std::remove_pointer_t<T>>, mesh::MeshVertex>))
float pointDistance(T& vertex, ::nicxlive::core::math::Vec2 point) {
    return detail::vectorDistance(position(vertex), point);
}

inline float pointDistance(mesh::MeshVertexSlot& vertex, ::nicxlive::core::math::Vec2 point) {
    return detail::vectorDistance(position(vertex), point);
}

namespace detail {
template<class R> decltype(auto) mutableVertex(R& vertices, std::size_t index) {
    if constexpr (std::same_as<std::remove_cvref_t<R>, mesh::MeshVertexArray>)
        return vertices.mutableSlot(index);
    else
        return (vertices[index]);
}
}

template<class T>
bool isOverlapped(T vertex, ::nicxlive::core::math::Vec2 point, float zoomRate) {
    return std::abs(pointDistance(vertex, point)) < detail::selectRadius / zoomRate;
}

template<class R>
bool isPointOverVertex(R vertices, ::nicxlive::core::math::Vec2 point, float zoomRate) {
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        auto vertex = vertices[i];
        if (std::abs(pointDistance(vertex, point)) < detail::selectRadius / zoomRate) return true;
    }
    return false;
}

template<class R, class Remove>
void removeVertexAt(R& vertices, ::nicxlive::core::math::Vec2 point, float zoomRate, Remove&& remove) {
    const auto count = vertices.size();
    for (std::size_t i = 0; i < count; ++i) {
        decltype(auto) vertex = detail::mutableVertex(vertices, i);
        if (std::abs(pointDistance(vertex, point)) < detail::selectRadius / zoomRate) {
            decltype(auto) selected = detail::mutableVertex(vertices, i);
            remove(selected);
            return;
        }
    }
}
template<auto Remove, class R>
void removeVertexAt(R& vertices, ::nicxlive::core::math::Vec2 point, float zoomRate) {
    removeVertexAt(vertices, point, zoomRate, Remove);
}

template<class R>
std::uint64_t getVertexFromPoint(R vertices, ::nicxlive::core::math::Vec2 point, float zoomRate) {
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        decltype(auto) vertex = detail::mutableVertex(vertices, i);
        if (std::abs(pointDistance(vertex, point)) < detail::selectRadius / zoomRate) return i;
    }
    return std::numeric_limits<std::uint64_t>::max();
}

template<class R>
compat::Slice<float> getVerticesInBrush(R vertices, ::nicxlive::core::math::Vec2 point,
    std::shared_ptr<viewport::common::mesheditor::brushes::base::Brush> brush) {
    compat::Slice<float> weights;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        decltype(auto) vertex = detail::mutableVertex(vertices, i);
        weights.append(compat::ngRequireObject(brush)->weightAt(point, position(vertex)));
    }
    return weights;
}

namespace detail {
inline compat::Slice<::nicxlive::core::math::Vec2> vertexArraySnapshot(
    const ::nicxlive::core::math::Vec2Array& vertices) {
    auto values = vertices.toArray();
    if (values.empty()) return {};
    return compat::Slice<::nicxlive::core::math::Vec2>(std::move(values));
}
}
inline bool isPointOverVertex(::nicxlive::core::math::Vec2Array vertices,
    ::nicxlive::core::math::Vec2 point, float zoomRate) {
    return isPointOverVertex(detail::vertexArraySnapshot(vertices), point, zoomRate);
}
inline std::uint64_t getVertexFromPoint(::nicxlive::core::math::Vec2Array vertices,
    ::nicxlive::core::math::Vec2 point, float zoomRate) {
    return getVertexFromPoint(detail::vertexArraySnapshot(vertices), point, zoomRate);
}
inline compat::Slice<float> getVerticesInBrush(::nicxlive::core::math::Vec2Array vertices,
    ::nicxlive::core::math::Vec2 point,
    std::shared_ptr<viewport::common::mesheditor::brushes::base::Brush> brush) {
    return getVerticesInBrush(detail::vertexArraySnapshot(vertices), point, std::move(brush));
}

template<class R>
void getBounds(R vertices, ::nicxlive::core::math::Vec2& minimum, ::nicxlive::core::math::Vec2& maximum) {
    constexpr float infinity = std::numeric_limits<float>::infinity();
    minimum = {infinity, infinity};
    maximum = {-infinity, -infinity};
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        auto vertex = vertices[i];
        if (minimum.x > position(vertex).x) minimum.x = position(vertex).x;
        if (minimum.y > position(vertex).y) minimum.y = position(vertex).y;
        if (maximum.x < position(vertex).x) maximum.x = position(vertex).x;
        if (maximum.y < position(vertex).y) maximum.y = position(vertex).y;
    }
}

template<class R>
::nicxlive::core::common::SharedArray<std::uint64_t> getInRect(R vertices,
    ::nicxlive::core::math::Vec2 minimum, ::nicxlive::core::math::Vec2 maximum, std::uint32_t groupId = 0) {
    if (minimum.x > maximum.x) std::swap(minimum.x, maximum.x);
    if (minimum.y > maximum.y) std::swap(minimum.y, maximum.y);
    ::nicxlive::core::common::SharedArray<std::uint64_t> result;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        auto vertex = vertices[i];
        if (minimum.x > position(vertex).x || minimum.y > position(vertex).y ||
            maximum.x < position(vertex).x || maximum.y < position(vertex).y) continue;
        if (groupId > 0 && !groupIdEquals(vertex, groupId)) continue;
        result.push_back(static_cast<std::uint64_t>(i));
    }
    return result;
}

template<class R>
std::int32_t findPoint(R vertices, ::nicxlive::core::math::Vec2 point, float zoomRate) {
    std::uint32_t bestIndex = 0;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        auto vertex = vertices[i];
        const float distance = detail::vectorDistance(detail::directPosition(vertex), point);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = static_cast<std::uint32_t>(i);
        }
    }
    if (bestDistance > detail::selectRadius / zoomRate) return -1;
    return std::bit_cast<std::int32_t>(bestIndex);
}

} // namespace nijigenerate::core::math::vertex
