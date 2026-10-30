#pragma once

#include "optimum.hpp"
#include "nijigenerate/compat/vec3u.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include "nijigenerate/core/cv/contours.hpp"
#include "nijigenerate/core/cv/image.hpp"
#include <math/veca.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <new>
#include <source_location>
#include <stdexcept>
#include <vector>

// Private source-body adapters; this is not a public replacement image/math API.
namespace nijigenerate::viewport::vertex::automesh::optimum::detail {
using Vec2 = nicxlive::core::math::Vec2;
using Vec4 = nicxlive::core::math::Vec4;
using Vec2Array = nicxlive::core::math::Vec2Array;
using Points = compat::Slice<Vec2>;
using Triangles = compat::Slice<compat::Vec3u>;
using Bytes = compat::Slice<std::uint8_t>;
using Image = core::cv::image::Image;
using Contour = core::cv::contours::Contour;
using Contours = core::cv::contours::Contours;

template<class T> compat::Slice<T> allocatePlaneArray(std::size_t count,
    std::source_location source = std::source_location::current()) {
    try {
        return compat::Slice<T>(std::vector<T>(count));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}
#if defined(_WIN32) && defined(_MSC_VER)
using SourceReal = double;
#else
using SourceReal = long double;
#endif

inline std::int32_t add(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
inline std::int32_t subtract(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
inline std::int32_t multiply(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
inline std::size_t index(std::int32_t y, std::int32_t width, std::int32_t x) {
    return static_cast<std::size_t>(add(multiply(y, width), x));
}
template<class T> T minimum(T a, T b) { return b < a ? b : a; }
template<class T> T maximum(T a, T b) { return a < b ? b : a; }

// Actual retained byte backing plus rank-two strided metadata. The source
// MIR views use this exact data; no threshold/skeleton/contour provider is added.
class Plane {
public:
    enum class Kind { Contiguous, Canonical, Universal };
    Plane() = default;
    Plane(Bytes storage, std::array<std::size_t, 2> dimensions,
        std::size_t rowStride, std::size_t columnStride = 1, std::size_t offset = 0,
        std::shared_ptr<Image> image = {}, Kind kind = Kind::Contiguous)
        : storage_(std::move(storage)), dimensions_(dimensions), rowStride_(rowStride),
          columnStride_(columnStride), offset_(offset), image_(std::move(image)), kind_(kind) {}
    static Plane channel(std::shared_ptr<Image> image, std::size_t channel);
    const auto& shape() const { return dimensions_; }
    std::size_t size() const { return dimensions_[0] * dimensions_[1]; }
    std::uint8_t at(std::size_t y, std::size_t x) const;
    std::uint8_t& at(std::size_t y, std::size_t x);
    std::uint8_t operator()(std::ptrdiff_t y, std::ptrdiff_t x) const { return at(y, x); }
    std::uint8_t& operator()(std::ptrdiff_t y, std::ptrdiff_t x) { return at(y, x); }
    Plane rectangle(std::size_t y0, std::size_t y1, std::size_t x0, std::size_t x1) const;
    Plane duplicate() const;
    Bytes flattened() const;
private:
    std::size_t offset(std::size_t y, std::size_t x) const;
    Bytes storage_;
    std::array<std::size_t, 2> dimensions_{};
    std::size_t rowStride_ = 0, columnStride_ = 1, offset_ = 0;
    std::shared_ptr<Image> image_;
    Kind kind_ = Kind::Contiguous;
};

Points dense(Vec2Array points);
Vec2Array fromDense(Points points);
Vec2Array adjacentUnique(Vec2Array points);
SourceReal lengthSquared(Vec2 value);
template<class ContourType> Vec2Array contoursToVec2s(ContourType contours);
extern template Vec2Array contoursToVec2s<Contours>(Contours contours);
Vec2 calcMoment(Vec2Array contour);
Points scaling(Vec2Array contour, Vec2 moment, float scale, std::int32_t erodeDilate);
Vec2Array horizontalMirrored(Vec2Array sampled, float axis);
Vec2Array resampling(Vec2Array contour, double rate, bool mirrorHoriz, float axisHoriz,
    bool mirrorVert, float axisVert);
Vec2 calculateNormalVector(Vec2 p1, Vec2 p2);
Vec2Array sampleExpandedFromThinned(Vec2Array points, float distance);
Vec2Array sampleContractedFromThinned(Vec2Array points, float distance, float factor = 1);
Vec2Array sampleCentroidContractedContour(Vec2Array points, float scale, float minDistance);
Vec2Array sampledAtRate(Vec2Array points, float rate);
double sumWidths(compat::Slice<float> widths);
template<class T> Plane dupMono(T input);
extern template Plane dupMono<Plane>(Plane input);
template<class T> compat::Slice<float> calculateWidthMap(T input, Contour region);
extern template compat::Slice<float> calculateWidthMap<Plane>(Plane input, Contour region);
template<class T> bool completeUncoveredArea(T compensated, Vec2Array vertices, Triangles triangles,
    Vec2Array contour, float minDistance, Vec2Array& outVertices, Triangles& outTriangles,
    std::int32_t width, std::int32_t height, Vec4 bounds);
extern template bool completeUncoveredArea<Plane>(Plane compensated, Vec2Array vertices, Triangles triangles,
    Vec2Array contour, float minDistance, Vec2Array& outVertices, Triangles& outTriangles,
    std::int32_t width, std::int32_t height, Vec4 bounds);
} // namespace nijigenerate::viewport::vertex::automesh::optimum::detail
