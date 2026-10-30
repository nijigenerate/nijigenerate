#include "optimum_detail.hpp"
#include "nijigenerate/compat/numeric.hpp"
#include "nijigenerate/core/math/triangle.hpp"
#include <cmath>
#include <tuple>
#include <vector>

namespace nijigenerate::viewport::vertex::automesh::optimum::detail {

template<class T> bool completeUncoveredArea(T compensated, Vec2Array vertices, Triangles triangles,
    Vec2Array contour, float minDistance, Vec2Array& outVertices, Triangles& outTriangles,
    std::int32_t width, std::int32_t height, Vec4 bounds) {
    // D out initialization happens after by-value input descriptors are copied.
    // Inputs intentionally may alias these exact caller output variables.
    outVertices = Vec2Array{};
    outTriangles = Triangles{};
    const auto pixels = compensated.flattened();
    const auto positions = vertices.toArray();
    for (std::size_t i = 0; i < triangles.size(); ++i) {
        core::math::triangle::fillPoly(pixels, width, height, bounds, positions, triangles, i, std::uint8_t{0});
    }
    const auto stride = add(width, 1);
    auto sat = allocatePlaneArray<std::int32_t>(
        static_cast<std::size_t>(multiply(add(height, 1), add(width, 1))));
    for (std::int32_t y = 0; y < height; ++y) {
        std::int32_t rowSum = 0;
        for (std::int32_t x = 0; x < width; ++x) {
            const std::int32_t value = pixels[index(y, width, x)] != 0 ? 1 : 0;
            rowSum = add(rowSum, value);
            sat[index(add(y, 1), stride, add(x, 1))] = add(sat[index(y, stride, add(x, 1))], rowSum);
        }
    }
    const auto initialArea = sat[index(height, stride, width)];
    const auto unchanged = [&] {
        outVertices = vertices;
        outTriangles = triangles;
        return false;
    };
    if (initialArea == 0) return unchanged();

    Vec2Array candidates;
    const float minDist = minDistance * 0.5f;
    const float minDistSquared = minDist * minDist;
    auto gridSize = compat::ngFloatToInt32(minDist);
    if (static_cast<float>(gridSize) < minDist) gridSize = add(gridSize, 1);
    if (gridSize < 1) gridSize = 1;
    const auto gridWidth = subtract(add(width, gridSize), 1) / gridSize;
    const auto gridHeight = subtract(add(height, gridSize), 1) / gridSize;
    const auto gridCount = static_cast<std::size_t>(gridWidth) * static_cast<std::size_t>(gridHeight);
    auto grid = allocatePlaneArray<Vec2Array>(gridCount);
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const auto vertex = vertices.at(i);
        const auto gx = compat::ngFloatToInt32(std::floor(vertex.x / static_cast<float>(gridSize)));
        const auto gy = compat::ngFloatToInt32(std::floor(vertex.y / static_cast<float>(gridSize)));
        if (gx < 0 || gy < 0 || gx >= gridWidth || gy >= gridHeight) continue;
        grid[index(gy, gridWidth, gx)].push_back(vertex);
    }
    for (std::size_t i = 0; i < contour.size(); ++i) {
        const auto point = contour.at(i);
        bool skip = false;
        const auto x = compat::ngFloatToInt32(std::round(point.x));
        const auto y = compat::ngFloatToInt32(std::round(point.y));
        if (x < 0 || x >= width || y < 0 || y >= height) continue;
        if (pixels[index(y, width, x)] != 255) continue;
        const auto gx = compat::ngFloatToInt32(std::floor(point.x / static_cast<float>(gridSize)));
        const auto gy = compat::ngFloatToInt32(std::floor(point.y / static_cast<float>(gridSize)));
        if (gx < 0 || gy < 0 || gx >= gridWidth || gy >= gridHeight) continue;
        for (std::int32_t oy = -1; oy <= 1 && !skip; ++oy) {
            const auto ny = add(gy, oy);
            if (ny < 0 || ny >= gridHeight) continue;
            for (std::int32_t ox = -1; ox <= 1 && !skip; ++ox) {
                const auto nx = add(gx, ox);
                if (nx < 0 || nx >= gridWidth) continue;
                const auto bucket = grid[index(ny, gridWidth, nx)];
                for (std::size_t j = 0; j < bucket.size(); ++j) {
                    const auto vertex = bucket.at(j);
                    const float dx = vertex.x - point.x;
                    const float dy = vertex.y - point.y;
                    if (dx * dx + dy * dy < minDistSquared) { skip = true; break; }
                }
            }
        }
        if (!skip) candidates.push_back(point);
    }
    if (candidates.empty()) return unchanged();

    std::int32_t bestScore = -1;
    auto best = candidates.at(0);
    const auto window = compat::ngFloatToInt32(minDistance);
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto point = candidates.at(i);
        const auto x = compat::ngFloatToInt32(std::round(point.x));
        const auto y = compat::ngFloatToInt32(std::round(point.y));
        const auto x1 = maximum(std::int32_t{0}, subtract(x, window));
        const auto y1 = maximum(std::int32_t{0}, subtract(y, window));
        const auto x2 = minimum(subtract(width, 1), add(x, window));
        const auto y2 = minimum(subtract(height, 1), add(y, window));
        const auto ax2 = add(x2, 1), ay2 = add(y2, 1);
        auto score = subtract(sat[index(ay2, stride, ax2)], sat[index(y1, stride, ax2)]);
        score = subtract(score, sat[index(ay2, stride, x1)]);
        score = add(score, sat[index(y1, stride, x1)]);
        if (score > bestScore) { bestScore = score; best = point; }
    }
    if (bestScore < 0) return unchanged();
    auto finalVertices = vertices.dup();
    finalVertices.push_back(best);
    auto [newVertices, newTriangles] = core::math::triangle::triangulate(dense(finalVertices),
        Vec4{0, 0, static_cast<float>(width), static_cast<float>(height)});
    if (!newTriangles.isNull()) {
        for (std::size_t i = 0; i < newTriangles.size(); ++i) {
            core::math::triangle::fillPoly(pixels, width, height, bounds,
                newVertices.toArray(), newTriangles, i, std::uint8_t{0});
        }
        std::int32_t remaining = 0;
        for (const auto pixel : pixels) if (pixel != 0) remaining = add(remaining, 1);
        if (remaining < initialArea) {
            outVertices = newVertices;
            outTriangles = newTriangles;
            return true;
        }
    }
    return unchanged();
}
template bool completeUncoveredArea<Plane>(Plane compensated, Vec2Array vertices, Triangles triangles,
    Vec2Array contour, float minDistance, Vec2Array& outVertices, Triangles& outTriangles,
    std::int32_t width, std::int32_t height, Vec4 bounds);

} // namespace nijigenerate::viewport::vertex::automesh::optimum::detail
