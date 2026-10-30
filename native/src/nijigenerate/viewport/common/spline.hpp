#pragma once
#include "mesh.hpp"
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <common/shared_array.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <source_location>

namespace nijigenerate::viewport::common::spline {
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Vec4;
using nicxlive::core::math::Mat4;
template<class T> using Array = nicxlive::core::common::SharedArray<T>;
using Positions = nicxlive::core::math::Vec2Array;
using Offsets = nicxlive::core::math::Vec3Array;
using MeshRef = std::shared_ptr<mesh::IncMesh>;
using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;

namespace detail {
// Source foreach binds the parent lane descriptors and index, without an
// opIndex check or a snapshot. A virtual callback may replace those lanes.
inline float& sourceOffsetComponent(Offsets& parent, std::size_t index, std::size_t lane) {
    auto& values = lane == 0 ? parent.x : lane == 1 ? parent.y : parent.z;
    if (index >= values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 461);
    return values[index];
}
#if defined(_WIN32)
using SourceReal = double;
#else
using SourceReal = long double;
#endif
inline constexpr float nan = std::numeric_limits<float>::quiet_NaN();
inline Vec2 add(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 sub(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 mul(Vec2 a, float b) { return {a.x * b, a.y * b}; }
inline Vec2 divide(Vec2 a, float b) { return {a.x / b, a.y / b}; }
inline SourceReal length(Vec2 v) {
    SourceReal result = 0;
    result += v.x * v.x;
    result += v.y * v.y;
    return std::sqrt(result);
}
inline float distance(Vec2 a, Vec2 b) { return static_cast<float>(length(sub(a, b))); }
inline Vec2 normalized(Vec2 v) {
    const SourceReal value = length(v);
    if (value != 0) return {static_cast<float>(v.x / value), static_cast<float>(v.y / value)};
    return v;
}
inline Mat4 initialMatrix() {
    Mat4 result;
    for (int row = 0; row < 4; ++row) for (int column = 0; column < 4; ++column) result[row][column] = nan;
    return result;
}
}

struct SplinePoint {
    Vec2 position{detail::nan, detail::nan};
    float weightL = detail::nan;
    float weightR = detail::nan;
};
struct BezierSegment {
    std::array<Vec2, 4> p{{{detail::nan, detail::nan}, {detail::nan, detail::nan},
                         {detail::nan, detail::nan}, {detail::nan, detail::nan}}};
    void split(float t, BezierSegment& left, BezierSegment& right);
    Vec2 eval(float t);
};

class CatmullSpline : public std::enable_shared_from_this<CatmullSpline> {
    Positions interpolated;
    Offsets drawLines;
    Offsets drawPoints;
public:
    std::uint32_t resolution = 40;
    float selectRadius = 16;
    Array<SplinePoint> points;
    Positions refMesh;
    Positions initTangents;
    std::shared_ptr<CatmullSpline> target;
    Offsets refOffsets;
    float origX = detail::nan, origY = detail::nan, origRotZ = detail::nan;
    virtual ~CatmullSpline() = default;
    CatmullSpline() = default;
    CatmullSpline(const CatmullSpline&) = delete;
    CatmullSpline& operator=(const CatmullSpline&) = delete;

    template<class T> void createTarget(T reference, Mat4 transform, const Positions* vertices = nullptr) {
        const auto keepAlive = weak_from_this().lock();
        target = std::allocate_shared<CatmullSpline>(
            nicxlive::core::d_runtime::SourceAllocator<CatmullSpline>{});
        {
            const auto receiver = compat::ngRequireObject(target);
            receiver->resolution = resolution;
        }
        {
            const auto receiver = compat::ngRequireObject(target);
            receiver->selectRadius = selectRadius;
        }
        {
            const auto receiver = compat::ngRequireObject(target);
            receiver->points = points.duplicate();
        }
        {
            const auto created = target;
            compat::ngRequireObject(created)->interpolate();
        }
        remapTarget(reference, transform, vertices);
    }
    template<class T> void remapTarget(T, Mat4 = Mat4::identity(), const Positions* = nullptr) {}
    virtual void remapTarget(MeshRef reference, Mat4 transform = Mat4::identity(), const Positions* vertices = nullptr);
    virtual void remapTarget(NodeRef node, Mat4 transform = Mat4::identity(), const Positions* vertices = nullptr);
    virtual void mapReference();
    template<class T> Mat4 exportTarget(T&, std::size_t, Vec2, Vec2, Vec2, Mat4, Vec2) { return Mat4::identity(); }
    template<class T> Mat4 exportTarget(T&, std::size_t, Vec2, Vec2, Vec2) { return Mat4::identity(); }
    virtual Mat4 exportTarget(MeshRef& mesh, std::size_t i, Vec2 vertex, Vec2 tangent,
        Vec2 initialTangent, Mat4 inverse, Vec2 deformation);
    virtual Mat4 exportTarget(NodeRef& node, std::size_t i, Vec2 vertex, Vec2 tangent, Vec2 initialTangent);
    template<class T> void resetTarget(T mesh) {
        const auto keepAlive = weak_from_this().lock();
        const auto count = refMesh.size();
        for (std::size_t i = 0; i < count; ++i) {
            nicxlive::core::math::Vec2View vertex(refMesh, i);
            exportTarget(mesh, i, vertex.toVector(), {0, 1}, {0, 1}, Mat4::identity(), {0, 0});
        }
    }
    template<class T> Mat4 updateTarget(T mesh, compat::Slice<std::uint64_t> selected = {},
        Mat4 inverse = Mat4::identity(), Positions deformations = {}) {
        const auto keepAlive = weak_from_this().lock();
        if (points.size() < 2) {
            resetTarget(mesh);
            return Mat4::identity();
        }
        const float epsilon = 0.0001f;
        const bool hasDeformations = !deformations.empty();
        Mat4 result = detail::initialMatrix();
        const auto count = refOffsets.size();
        for (std::size_t i = 0; i < count; ++i) {
            if (!std::isfinite(detail::sourceOffsetComponent(refOffsets, i, 2))) continue;
            if (!selected.isNull()) {
                bool found = false;
                for (auto index : selected) if (index == i) { found = true; break; }
                if (!found) continue;
            }
            const Vec2 point = [&] {
                const auto receiver = target;
                return compat::ngRequireObject(receiver)->eval(detail::sourceOffsetComponent(refOffsets, i, 2));
            }();
            const Vec2 next = [&] {
                const auto receiver = target;
                return compat::ngRequireObject(receiver)->eval(detail::sourceOffsetComponent(refOffsets, i, 2) + epsilon);
            }();
            Vec2 tangent = detail::sub(next, point);
            tangent = detail::divide(tangent, std::abs(detail::distance(tangent, {0, 0})));
            float vertexX = point.x + detail::sourceOffsetComponent(refOffsets, i, 0) * tangent.x;
            vertexX -= detail::sourceOffsetComponent(refOffsets, i, 1) * tangent.y;
            float vertexY = point.y + detail::sourceOffsetComponent(refOffsets, i, 1) * tangent.x;
            vertexY += detail::sourceOffsetComponent(refOffsets, i, 0) * tangent.y;
            const Vec2 vertex{vertexX, vertexY};
            const Vec2 initial = initTangents.size() > i ? initTangents[i].toVector() : tangent;
            if (!hasDeformations) result = exportTarget(mesh, i, vertex, tangent, initial);
            else if (i < deformations.size())
                result = exportTarget(mesh, i, vertex, tangent, initial, inverse, deformations[i].toVector());
            else result = exportTarget(mesh, i, vertex, tangent, initial);
        }
        return result;
    }
    virtual void update();
    virtual void interpolate();
    virtual Vec2 eval(float offset);
    virtual float findClosestPointOffset(Vec2 point);
    virtual float findClosestPointOffset(Vec2 point, Vec2& tangent);
    virtual void splitAt(float offset);
    virtual void prependPoint(Vec2 point);
    virtual void appendPoint(Vec2 point);
    virtual std::int32_t findPoint(Vec2 point, float zoomRate);
    virtual std::int32_t addPoint(Vec2 point, float zoomRate);
    virtual void removePoint(std::uint32_t index);
    virtual void draw(Mat4 transform, Vec4 color,
        std::uint32_t lockedPoint = std::numeric_limits<std::uint32_t>::max());
    virtual void resetFloating();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual int opCmp(std::shared_ptr<CatmullSpline> other);
    virtual bool opEquals(std::shared_ptr<CatmullSpline> other);
};
bool ngCatmullSplineObjectEquals(std::shared_ptr<CatmullSpline> left, std::shared_ptr<CatmullSpline> right);
} // namespace nijigenerate::viewport::common::spline

namespace nicxlive::core::d_runtime {
#define NG_SPLINE_SOURCE_TYPE(Type, Object) \
    template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::spline::Type> { \
        inline static constexpr bool isNativeObject = Object; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.common.spline." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.common.spline." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_SPLINE_SOURCE_TYPE(SplinePoint, false)
NG_SPLINE_SOURCE_TYPE(BezierSegment, false)
NG_SPLINE_SOURCE_TYPE(CatmullSpline, true)
#undef NG_SPLINE_SOURCE_TYPE
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::common::spline::CatmullSpline> {
    inline static constexpr char className[] = "nijigenerate.viewport.common.spline.CatmullSpline";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.common.spline.CatmullSpline)";
};
}

// Publish formatting metadata from the actual completed value declaration.
// The adapter's pragma-once includes preserve one canonical SplinePoint type.
#include <nijigenerate/core/typed_format_spline.hpp>
