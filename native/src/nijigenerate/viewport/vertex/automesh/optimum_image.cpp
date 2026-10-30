#include "optimum_detail.hpp"
#if defined(NG_DEBUG_AUTOMESH_OPT)
#include "optimum_diagnostics.hpp"
#endif
#include "nijigenerate/core/cv/distancetransform.hpp"
#include "nijigenerate/core/math/skeletonize.hpp"
#include "nijigenerate/core/math/path.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include <stdexcept>
#include <vector>

namespace nijigenerate::viewport::vertex::automesh::optimum::detail {

Plane Plane::channel(std::shared_ptr<Image> image, std::size_t channel) {
    auto view = compat::ngRequireObject(image)->sliced();
#ifndef NDEBUG
    if (channel >= view.shape()[2])
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Slice.opIndex: index must be less than length"),
            compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 2292);
#endif
    return Plane(view.storage(), {view.shape()[0], view.shape()[1]},
        view.shape()[1] * view.shape()[2], view.shape()[2], channel, std::move(image), Kind::Universal);
}
std::size_t Plane::offset(std::size_t y, std::size_t x) const {
#ifndef NDEBUG
    // MIR indexStride checks the last dimension before the earlier dimensions.
    if (x >= dimensions_[1])
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("2D slice of ubyte: bounds check failed at 2 dimension"),
            compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"),
            kind_ == Kind::Contiguous ? 788 : kind_ == Kind::Canonical ? 802 : 816);
    if (y >= dimensions_[0])
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("2D slice of ubyte: bounds check failed at 1 dimension"),
            compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"),
            kind_ == Kind::Contiguous ? 794 : kind_ == Kind::Canonical ? 809 : 820);
#endif
    return offset_ + y * rowStride_ + x * columnStride_;
}
std::uint8_t Plane::at(std::size_t y, std::size_t x) const { return storage_.data()[offset(y, x)]; }
std::uint8_t& Plane::at(std::size_t y, std::size_t x) { return storage_.data()[offset(y, x)]; }
Plane Plane::rectangle(std::size_t y0, std::size_t y1, std::size_t x0, std::size_t x1) const {
#ifndef NDEBUG
    if (y0 > y1)
        throw compat::ngStaticError<compat::AssertError>(compat::RetainedString::fromStaticLiteral(
            "Slice.opSlice!0: the left opSlice boundary must be less than or equal to the right bound."),
            compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 2159);
    if (y1 > dimensions_[0])
        throw compat::ngStaticError<compat::AssertError>(compat::RetainedString::fromStaticLiteral(
            "Slice.opSlice!0: right opSlice boundary must be less than or equal to the length of the given dimension."),
            compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 2162);
    if (x0 > x1)
        throw compat::ngStaticError<compat::AssertError>(compat::RetainedString::fromStaticLiteral(
            "Slice.opSlice!1: the left opSlice boundary must be less than or equal to the right bound."),
            compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 2159);
    if (x1 > dimensions_[1])
        throw compat::ngStaticError<compat::AssertError>(compat::RetainedString::fromStaticLiteral(
            "Slice.opSlice!1: right opSlice boundary must be less than or equal to the length of the given dimension."),
            compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 2162);
#endif
    return Plane(storage_, {y1 - y0, x1 - x0}, rowStride_, columnStride_,
        offset_ + y0 * rowStride_ + x0 * columnStride_, image_,
        kind_ == Kind::Universal ? Kind::Universal : Kind::Canonical);
}
Plane Plane::duplicate() const {
    Bytes result;
    if (size()) result = allocatePlaneArray<std::uint8_t>(size());
    for (std::size_t y = 0; y < dimensions_[0]; ++y) {
        for (std::size_t x = 0; x < dimensions_[1]; ++x) result[y * dimensions_[1] + x] = at(y, x);
    }
    return Plane(std::move(result), dimensions_, dimensions_[1]);
}
Bytes Plane::flattened() const {
    if (columnStride_ != 1 || rowStride_ != dimensions_[1])
        throw std::invalid_argument("Image plane reshape requires contiguous mono storage");
    return storage_.slice(offset_, offset_ + size());
}
template<class T> Plane dupMono(T input) {
    Bytes data;
    const auto allocationHeight = input.shape()[0];
    const auto allocationWidth = input.shape()[1];
    const auto count = allocationHeight * allocationWidth;
    if (count) data = allocatePlaneArray<std::uint8_t>(count);
    const auto height = input.shape()[0];
    for (std::size_t y = 0; y < height; ++y) {
        const auto width = input.shape()[1];
        for (std::size_t x = 0; x < width; ++x)
            data[y * input.shape()[1] + x] = static_cast<std::uint8_t>(input(y, x));
    }
    // Source new allocates its receiver before evaluating constructor arguments.
    auto image = core::cv::image::ngCreateImageDeferred<>(
        [&] { return input.shape()[1]; },
        [&] { return input.shape()[0]; },
        [] { return core::cv::image::ImageFormat::IF_MONO; },
        [] { return core::cv::image::BitDepth::BD_8; },
        [&] { return data; });
    return Plane::channel(std::move(image), 0);
}
template Plane dupMono<Plane>(Plane input);
template<class T> compat::Slice<float> calculateWidthMap(T input, Contour region) {
    std::int32_t xmin = region[0].x, xmax = region[0].x;
    std::int32_t ymin = region[0].y, ymax = region[0].y;
    for (const auto point : region) {
        if (point.x < xmin) xmin = point.x;
        if (point.x > xmax) xmax = point.x;
        if (point.y < ymin) ymin = point.y;
        if (point.y > ymax) ymax = point.y;
    }
    const auto width = add(subtract(xmax, xmin), 1);
    const auto height = add(subtract(ymax, ymin), 1);
    const auto rectangle = input.rectangle(ymin, add(ymax, 1), xmin, add(xmax, 1));
    Bytes regionData;
    const auto count = static_cast<std::size_t>(multiply(width, height));
    if (count) regionData = allocatePlaneArray<std::uint8_t>(count);
    for (std::int32_t y = 0; y < height; ++y) {
        for (std::int32_t x = 0; x < width; ++x) regionData[index(y, width, x)] = rectangle.at(y, x);
    }
    auto original = regionData.duplicate();
    auto regionImage = core::cv::image::ngCreateImageDeferred<>(
        [&] { return width; },
        [&] { return height; },
        [] { return core::cv::image::ImageFormat::IF_MONO; },
        [] { return core::cv::image::BitDepth::BD_8; },
        [&] { return regionData; });
    auto skeleton = Plane::channel(std::move(regionImage), 0);
    core::math::skeletonize::skeletonizeImage(skeleton);
    const auto path = core::math::path::extractPath(skeleton, width, height);
    // The genuine Image constructor has already checked the same shape product.
    Plane originalView(original, {static_cast<std::size_t>(height), static_cast<std::size_t>(width)}, width);
    core::cv::distancetransform::DistanceView distance;
    core::cv::distancetransform::NearestView nearest;
    core::cv::distancetransform::distanceTransform(originalView, distance, nearest);
    compat::Slice<float> widths;
    for (const auto point : path) {
        // Canonical path points are unsigned, so the source's negative checks
        // cannot pass. Its upper-shape guards still apply before indexing.
        if (point.y >= distance.shape()[0] || point.x >= distance.shape()[1]) continue;
        if (distance.at(point.y, point.x) > 0) {
            widths.append(2 * distance.at(point.y, point.x));
#if defined(NG_DEBUG_AUTOMESH_OPT)
            diagnostics::print("w, h = %d x %d, ", width, height);
            diagnostics::println(" distance: %.2f", distance.at(point.y, point.x));
#endif
        }
    }
    return widths;
}
template compat::Slice<float> calculateWidthMap<Plane>(Plane input, Contour region);

} // namespace nijigenerate::viewport::vertex::automesh::optimum::detail
