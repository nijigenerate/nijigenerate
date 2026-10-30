#include "grid.hpp"
#include "meta.hpp"
#include "common.hpp"
#include "nijigenerate/compat/numeric.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <nodes/grid_axis_sort.hpp>
#include <bit>
#include <cstdint>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::vertex::automesh::grid {
namespace {
std::int32_t increment(std::int32_t value) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1u);
}
}

void GridAutoMeshProcessor::rebuildScalesFromSegments() {
    scaleY.resize(0);
    scaleX.resize(0);
    const auto ys = compat::ngFloatToInt32(ySegments < 1 ? 1 : ySegments);
    const auto xs = compat::ngFloatToInt32(xSegments < 1 ? 1 : xSegments);
    if (margin != 0) {
        scaleY.append(-margin);
        scaleX.append(-margin);
    }
    const auto yEnd = increment(ys);
    for (std::int32_t y = 0; y < yEnd; y = increment(y)) {
        scaleY.append(static_cast<float>(y) / static_cast<float>(ys));
    }
    const auto xEnd = increment(xs);
    for (std::int32_t x = 0; x < xEnd; x = increment(x)) {
        scaleX.append(static_cast<float>(x) / static_cast<float>(xs));
    }
    if (margin != 0) {
        scaleY.append(1 + margin);
        scaleX.append(1 + margin);
    }
}

void GridAutoMeshProcessor::ngPostParamWrite(compat::RetainedString id) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    if (id.view() == "x_segments" || id.view() == "y_segments" || id.view() == "margin") {
        rebuildScalesFromSegments();
    }
}

void GridAutoMeshProcessor::rebuildGridMesh(Mesh mesh, float minX, float maxX,
    float minY, float maxY, nicxlive::core::math::Vec2 center) {
    compat::ngRequireObject(mesh)->clear();
    auto& destination = mesh->axes;
    const auto axes = [] {
        try {
            return ::nijigenerate::viewport::common::mesh::GridAxes{{}, {}};
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    destination = axes;
    nicxlive::core::nodes::grid_axis_detail::sort({scaleY.data(), scaleY.size()});
    const auto yScales = scaleY;
    for (const float y : yScales) {
        mesh->axes[0].push_back((minY * (1 - y) + maxY * y) - center.y);
    }
    nicxlive::core::nodes::grid_axis_detail::sort({scaleX.data(), scaleX.size()});
    const auto xScales = scaleX;
    for (const float x : xScales) {
        mesh->axes[1].push_back((minX * (1 - x) + maxX * x) - center.x);
    }
    nicxlive::core::nodes::MeshData meshData;
    meshData.gridAxes = mesh->axes;
    meshData.regenerateGrid();
    mesh->copyFromMeshData(meshData);
}

GridAutoMeshProcessor::Mesh GridAutoMeshProcessor::autoMesh(Target target, Mesh mesh,
    bool, float, bool, float) {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    auto input = common::getAlphaInput(target);
    if (input.w <= 0 || input.h <= 0) return mesh;
    auto image = compat::ngRequireObject(input.img)->sliced();
#ifndef NDEBUG
    if (image.shape()[2] <= 3)
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Slice.opIndex: index must be less than length"),
            compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 2292);
#endif
    for (std::size_t y = 0; y < image.shape()[0]; ++y) {
        for (std::size_t x = 0; x < image.shape()[1]; ++x) {
            auto& pixel = image.storage().data()[(y * image.shape()[1] + x) * image.shape()[2] + 3];
            const auto threshold = static_cast<std::uint8_t>(compat::ngFloatToInt32(maskThreshold));
            pixel = pixel < threshold ? 0 : 255;
        }
    }
    std::int32_t minX = input.w, minY = input.h, maxX = -1, maxY = -1;
    for (std::size_t y = 0; y < image.shape()[0]; ++y) {
        for (std::size_t x = 0; x < image.shape()[1]; ++x) {
            if (image.storage().data()[(y * image.shape()[1] + x) * image.shape()[2] + 3] > 0) {
                const auto xi = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(x));
                const auto yi = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(y));
                if (xi < minX) minX = xi;
                if (yi < minY) minY = yi;
                if (xi > maxX) maxX = xi;
                if (yi > maxY) maxY = yi;
            }
        }
    }
    if (maxX < 0 || maxY < 0) return mesh;
    const auto center = common::alphaImageCenter(input);
    rebuildGridMesh(mesh, static_cast<float>(minX), static_cast<float>(maxX),
        static_cast<float>(minY), static_cast<float>(maxY), center);
    common::mapImageCenteredMeshToTargetLocal(mesh, target, input);
    return mesh;
}

compat::RetainedString GridAutoMeshProcessor::icon() {
    return compat::RetainedString::fromStaticLiteral("\xee\x8f\xac");
}
compat::RetainedString GridAutoMeshProcessor::procId() {
    return meta::AutoMeshClassInfoBody<GridAutoMeshProcessor>(*this).procId();
}
compat::RetainedString GridAutoMeshProcessor::displayName() {
    return meta::AutoMeshClassInfoBody<GridAutoMeshProcessor>(*this).displayName();
}
int GridAutoMeshProcessor::order() {
    return meta::AutoMeshClassInfoBody<GridAutoMeshProcessor>(*this).order();
}

} // namespace nijigenerate::viewport::vertex::automesh::grid
