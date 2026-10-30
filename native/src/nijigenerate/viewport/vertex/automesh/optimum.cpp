#include "optimum_detail.hpp"
#if defined(NG_DEBUG_AUTOMESH_OPT) || defined(NG_DEBUG_AUTOMESH_OPT_FULL)
#include "optimum_diagnostics.hpp"
#endif
#include "common.hpp"
#include "contours.hpp"
#include "nijigenerate/compat/numeric.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/core/math/triangle.hpp"
#include <nodes/grid_deformer.hpp>
#include <cmath>
#include <limits>
#include <tuple>

namespace nijigenerate::viewport::vertex::automesh::optimum {

OptimumAutoMeshProcessor::Mesh OptimumAutoMeshProcessor::autoMesh(Target target, Mesh mesh,
    bool mirrorHoriz, float axisHoriz, bool mirrorVert, float axisVert) {
    using namespace detail;
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    if (std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(target)) return mesh;
    [[maybe_unused]] const auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(target);
    auto input = common::getAlphaInput(target);
    if (input.w <= 0 || input.h <= 0 || !input.img) return mesh;
    auto image = input.img;
    const std::int32_t width = input.w, height = input.h;
    if (mirrorHoriz) axisHoriz += static_cast<float>(width / 2);
    if (mirrorVert) axisVert += static_cast<float>(height / 2);
    auto binary = Plane::channel(image, 3);
    for (std::size_t y = 0; y < binary.shape()[0]; ++y) {
        for (std::size_t x = 0; x < binary.shape()[1]; ++x) {
            const auto threshold = static_cast<std::uint8_t>(compat::ngFloatToInt32(MASK_THRESHOLD));
            binary.at(y, x) = binary.at(y, x) < threshold ? 0 : 255;
        }
    }
    const Vec2 center{static_cast<float>(width / 2), static_cast<float>(height / 2)};
    const float averageSize = static_cast<float>(static_cast<double>(add(width, height)) / 2.0);
    const float minDistance = maximum(static_cast<float>(maximum(width, height)) / DIV_PER_PART, MIN_DISTANCE);
    Vec2Array vertices, boundary;
    double sumWidth = 0, length = 0, widthMapLength = 0;
    Contours regions;
    compat::Slice<core::cv::contours::ContourHierarchy> hierarchy;
    core::cv::contours::findContours(binary.duplicate(), regions, hierarchy,
        core::cv::contours::RetrievalMode::EXTERNAL, core::cv::contours::ApproximationMethod::SIMPLE);
#if defined(NG_DEBUG_AUTOMESH_OPT_FULL)
    diagnostics::println("regionContours=%s", diagnostics::contourArgument(regions));
#endif
#if defined(NG_DEBUG_AUTOMESH_OPT)
    diagnostics::println("Region contours=%d, Region hierarchy=%d", regions.size(), hierarchy.size());
#endif
    Contours contourList;
    [[maybe_unused]] std::int32_t regionCount = 0;
    for (const auto region : regions) {
        regionCount = add(regionCount, 1);
        contourList.append(region);
#if defined(NG_DEBUG_AUTOMESH_OPT)
        diagnostics::println("calculateWidthMap for region %d", regionCount);
#endif
        const auto widths = detail::calculateWidthMap(binary, region);
        widthMapLength += static_cast<double>(widths.size());
#if defined(NG_DEBUG_AUTOMESH_OPT)
        diagnostics::println("  region %d: widthMapLength=%0.2f", regionCount, widthMapLength);
#endif
#if defined(NG_DEBUG_AUTOMESH_OPT_FULL)
        diagnostics::println("widthMap=%s", widths);
#endif
        auto filtered = stdFilter<[](float value) { return value > 0; }>(widths);
        const auto validWidths = compat::phobos::ngArray(std::move(filtered));
        sumWidth += sumWidths(validWidths);
        length += static_cast<double>(validWidths.size());
    }
#if defined(NG_DEBUG_AUTOMESH_OPT)
    diagnostics::println("contours=%d", contourList.size());
#endif
    const auto contour = detail::contoursToVec2s(contourList);
#if defined(NG_DEBUG_AUTOMESH_OPT)
    diagnostics::println("contourVec=%d", contour.size());
#endif
    if (contour.size() < 3) return mesh;
    compat::ngRequireObject(mesh)->clear();
    double averageWidth = length > 0 ? sumWidth / length : 0;
    [[maybe_unused]] double ratio = widthMapLength > 0 ? sumWidth / widthMapLength : 0;
    double widthLengthRatio = length > 0 ? averageWidth / length : std::numeric_limits<double>::infinity();
    if (!std::isfinite(averageWidth)) averageWidth = 0;
    if (!std::isfinite(ratio)) ratio = 0;
    if (!std::isfinite(widthLengthRatio)) widthLengthRatio = std::numeric_limits<double>::infinity();
#if defined(NG_DEBUG_AUTOMESH_OPT)
    diagnostics::println("found=%d: avgW=%0.2f, len=%0.2f, avgW/len=%0.2f, ratio=%0.2f",
        contourList.size(), averageWidth, length, widthLengthRatio, ratio);
#endif
    const bool sharp = averageWidth < LARGE_THRESHOLD
        && (length < LENGTH_THRESHOLD || widthLengthRatio < RATIO_THRESHOLD);
    boundary.append(resampling(contour, minDistance, mirrorHoriz, axisHoriz, mirrorVert, axisVert));
    if (sharp) {
        vertices.append(sampleExpandedFromThinned(boundary, averageSize * SHARP_EXPANSION_FACTOR));
    } else {
        // The source performs this unused duplication before appending vertices.
        // Preserve its ownership/allocation/failure point, then use current SCALES.
        [[maybe_unused]] const auto unusedScalesCopy = SCALES.duplicate();
        vertices.append(boundary);
        vertices.append(sampleExpandedFromThinned(boundary, averageSize * NONSHARP_EXPANSION_FACTOR));
        vertices.append(sampleContractedFromThinned(boundary, averageSize * NONSHARP_CONTRACTION_FACTOR, 1));
        const auto scales = SCALES;
        for (const float scale : scales) {
            auto points = sampleCentroidContractedContour(boundary, scale, minDistance);
            points = sampledAtRate(points, scale);
            if (mirrorHoriz) points = horizontalMirrored(points, axisHoriz);
            vertices.append(points);
        }
        vertices = adjacentUnique(vertices);
    }
    const Vec4 bounds{0, 0, static_cast<float>(width), static_cast<float>(height)};
    auto result = core::math::triangle::triangulate(dense(vertices), bounds);
    vertices = std::get<0>(result);
    auto triangles = std::get<1>(result);
    auto compensated = detail::dupMono(binary);
    for (std::size_t y = 0; y < compensated.shape()[0]; ++y) {
        for (std::size_t x = 0; x < compensated.shape()[1]; ++x)
            compensated.at(y, x) = compensated.at(y, x) != 0 ? 255 : 0;
    }
    for (std::int32_t i = 0; i < 5; ++i) {
        const bool updated = detail::completeUncoveredArea(compensated, vertices, triangles, contour,
            minDistance, vertices, triangles, width, height, bounds);
#if defined(NG_DEBUG_AUTOMESH_OPT)
        diagnostics::println("complete uncovered:%s", updated);
#endif
        if (!updated) break;
    }
    if (vertices.size() < 1) return mesh;
    if (std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(target) && vertices.size() < 3) return mesh;
    auto newMesh = ::nijigenerate::viewport::common::mesh::IncMesh::create(mesh);
    newMesh->changed = true;
    newMesh->vertices.resize(0);
    const auto positions = vertices.toArray();
    Points translatedPositions;
    translatedPositions.resize(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i) {
        const auto point = positions[i];
        translatedPositions[i] = Vec2{point.x - center.x, point.y - center.y};
    }
    const auto translated = fromDense(translatedPositions);
    if (std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(target)) {
        newMesh->importVertsAndTris(translated, triangles);
    } else {
        for (std::size_t i = 0; i < translated.size(); ++i)
            newMesh->vertices.append(core::math::mesh::ngCreateMeshVertex(translated.at(i)));
    }
    common::mapImageCenteredMeshToTargetLocal(newMesh, target, input);
    newMesh->refresh();
    return newMesh;
}

} // namespace nijigenerate::viewport::vertex::automesh::optimum
