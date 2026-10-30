#include "skeletonize.hpp"
#include "meta.hpp"
#include "common.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include "nijigenerate/compat/contiguous_view.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/compat/numeric.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include "nijigenerate/core/math/path.hpp"
#include "nijigenerate/core/math/skeletonize.hpp"
#include <nodes/grid_deformer.hpp>
#include <bit>
#include <cmath>
#include <new>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace nijigenerate::viewport::vertex::automesh::skeletonize {
namespace {
// The actual duplicated alpha storage, with the indexing surface used by the
// canonical skeletonization and path templates. Copies retain the same pixels.
struct AlphaPlane {
    compat::ContiguousView<std::uint8_t, 2> pixels;
    const auto& shape() const { return pixels.shape(); }
    std::size_t size() const { return pixels.elementCount(); }
    std::uint8_t& operator()(std::ptrdiff_t row, std::ptrdiff_t column) {
        const auto y = static_cast<std::size_t>(row);
        const auto x = static_cast<std::size_t>(column);
#ifndef NDEBUG
        if (x >= shape()[1])
            throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral("2D slice of ubyte: bounds check failed at 2 dimension"),
                compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 788);
        if (y >= shape()[0])
            throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral("2D slice of ubyte: bounds check failed at 1 dimension"),
                compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 794);
#endif
        // The source MIR slice indexes its retained raw iterator after assertions.
        return pixels.storage().data()[y * shape()[1] + x];
    }
};
}

SkeletonExtractor::Mesh SkeletonExtractor::autoMesh(Target target, Mesh mesh,
    bool, float, bool, float) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    if (std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(target)) return mesh;
    auto input = common::getAlphaInput(target);
    if (input.w <= 0 || input.h <= 0 || !input.img) return mesh;

    auto image = input.img->sliced();
    const auto height = image.shape()[0];
    const auto width = image.shape()[1];
    // Validate channel selection even for an empty outer dimension.
#ifndef NDEBUG
    if (image.shape()[2] <= 3)
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Slice.opIndex: index must be less than length"),
            compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 2292);
#endif
    compat::Slice<std::uint8_t> alpha;
    if (height * width) {
        try {
            alpha = compat::Slice<std::uint8_t>(std::vector<std::uint8_t>(height * width));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 519);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 519);
        }
    }
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x)
            alpha[y * width + x] = image.storage().data()[(y * width + x) * image.shape()[2] + 3];
    }
    AlphaPlane binary{{std::move(alpha), {height, width}}};
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const auto threshold = static_cast<std::uint8_t>(compat::ngFloatToInt32(maskThreshold));
            binary(y, x) = binary(y, x) < threshold ? 0 : 255;
        }
    }
    core::math::skeletonize::skeletonizeImage(binary);
    auto path = core::math::path::extractPath(binary, input.w, input.h);
    controlPoints = simplifyByTargetCount(path, compat::ngFloatToInt32(targetPointCount));

    compat::ngRequireObject(mesh)->clear();
    const nicxlive::core::math::Vec2 center{static_cast<float>(input.w / 2), static_cast<float>(input.h / 2)};
    const auto points = controlPoints;
    for (const Point point : points) {
        const nicxlive::core::math::Vec2 position{static_cast<float>(point.x), static_cast<float>(point.y)};
        auto vertex = core::math::mesh::ngCreateMeshVertexSource([&] {
            return nicxlive::core::math::Vec2{position.x - center.x, position.y - center.y};
        });
        mesh->vertices.append(vertex);
    }
    common::mapImageCenteredMeshToTargetLocal(mesh, target, input);
    if (std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(target)) return mesh->autoTriangulate();
    return mesh;
}

PointArray SkeletonExtractor::getControlPoints() { return controlPoints; }

PointArray SkeletonExtractor::simplifyByTargetCount(PointArray points, std::int32_t targetCount) {
    const auto count = static_cast<std::size_t>(targetCount);
    if (points.size() <= count) return points.duplicate();
    auto simplified = points.duplicate();
    const auto triangleArea = [](Point a, Point b, Point c) {
        const double ax = a.x, ay = a.y;
        const double bx = b.x, by = b.y;
        const double cx = c.x, cy = c.y;
        return std::abs(ax * (by - cy) + bx * (cy - ay) + cx * (ay - by)) / 2.0;
    };
    const auto areaAt = [&](std::int32_t first, std::int32_t middle, std::int32_t last,
            std::size_t sourceLine) {
        // D evaluates arguments left to right, including failing array reads.
        const auto pointAt = [&](std::int32_t index) {
            const auto offset = static_cast<std::size_t>(index);
            if (offset >= simplified.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                offset, simplified.size(),
                compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/vertex/automesh/skeletonize.d"),
                sourceLine);
            return simplified[offset];
        };
        const Point a = pointAt(first);
        const Point b = pointAt(middle);
        const Point c = pointAt(last);
        return triangleArea(a, b, c);
    };
    while (simplified.size() > count) {
        std::int32_t removeIndex = 1;
        double minArea = areaAt(0, 1, 2, 113);
        for (std::int32_t i = 2; static_cast<std::size_t>(i) < simplified.size() - 1;
                i = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i) + 1u)) {
            const auto next = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i) + 1u);
            const auto previous = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i) - 1u);
            const double area = areaAt(previous, i, next, 116);
            if (area < minArea) {
                minArea = area;
                removeIndex = i;
            }
        }
        // Evaluate both source slices before constructing the new concatenation backing.
        const auto left = simplified.slice(0, static_cast<std::size_t>(removeIndex));
        const auto nextIndex = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(removeIndex) + 1u);
        const auto right = simplified.slice(static_cast<std::size_t>(nextIndex), simplified.size());
        PointArray replacement;
        try {
            replacement = PointArray{std::vector<Point>(left.size() + right.size())};
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        for (std::size_t i = 0; i < left.size(); ++i) replacement[i] = left[i];
        for (std::size_t i = 0; i < right.size(); ++i) replacement[left.size() + i] = right[i];
        simplified = std::move(replacement);
    }
    return simplified;
}

compat::RetainedString SkeletonExtractor::icon() {
    return compat::RetainedString::fromStaticLiteral("\xee\xae\xbb");
}

compat::RetainedString SkeletonExtractor::procId() {
    return meta::AutoMeshClassInfoBody<SkeletonExtractor>(*this).procId();
}
compat::RetainedString SkeletonExtractor::displayName() {
    return meta::AutoMeshClassInfoBody<SkeletonExtractor>(*this).displayName();
}
int SkeletonExtractor::order() {
    return meta::AutoMeshClassInfoBody<SkeletonExtractor>(*this).order();
}

} // namespace nijigenerate::viewport::vertex::automesh::skeletonize
