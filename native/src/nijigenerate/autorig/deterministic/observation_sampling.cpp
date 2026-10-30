#include "detail/observation.hpp"
#include <array>
namespace nijigenerate::autorig::deterministic::observation {
using namespace detail;
Json ngRigAlphaRuns(compat::ConstSlice<std::uint8_t> rgba, std::uint8_t threshold) {
    core::settings::SettingsJsonArray runs; std::size_t start = 0; bool inside = false;
    for (std::size_t pixel = 0; pixel < rgba.size() / 4; ++pixel) {
        bool opaque = rgba[pixel * 4 + 3] > threshold;
        if (opaque && !inside) { start = pixel; inside = true; }
        if (!opaque && inside) { runs.append(Json(start)); runs.append(Json(pixel - start)); inside = false; }
    }
    if (inside) { runs.append(Json(start)); runs.append(Json(rgba.size() / 4 - start)); }
    return runs;
}
double ngRigAlphaCoverage(Json material, compat::Slice<Point2> points, compat::RetainedStringSlice support,
    compat::Slice<bool>* occupancy) {
    auto encoded = object(material).if_contains(support.view()); if (!encoded || points.empty()) return 0;
    auto runs = encoded->as_array(); auto mapping = at(material, "source_root_mapping");
    auto vertices = ngRigPoints(at(mapping, "vertices")), uvs = ngRigPoints(at(mapping, "uv"));
    auto dimensions = ngRigNumbers(at(material, "texture_size"));
    std::size_t width = index(dimensions[0]), height = index(dimensions[1]), hits = 0;
    if (occupancy) {
        compat::Slice<bool> allocated;
        allocated.resize(points.size());
        *occupancy = allocated;
    }
    for (std::size_t pointIndex = 0; pointIndex < points.size(); ++pointIndex) for (auto triangle : at(mapping, "triangles").as_array()) {
        auto point = points[pointIndex]; auto indices = ngRigNumbers(triangle);
        auto ia = index(indices[0]), ib = index(indices[1]), ic = index(indices[2]);
        auto a = vertices[ia], b = vertices[ib], c = vertices[ic];
        double determinant = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
        if (std::abs(determinant) < 1e-14) continue;
        double s = ((point[0] - a[0]) * (c[1] - a[1]) - (point[1] - a[1]) * (c[0] - a[0])) / determinant;
        double t = ((b[0] - a[0]) * (point[1] - a[1]) - (b[1] - a[1]) * (point[0] - a[0])) / determinant;
        if (s < -1e-8 || t < -1e-8 || s + t > 1 + 1e-8) continue;
        Point2 uv{uvs[ia][0] * (1 - s - t) + uvs[ib][0] * s + uvs[ic][0] * t,
            uvs[ia][1] * (1 - s - t) + uvs[ib][1] * s + uvs[ic][1] * t};
        if (uv[0] >= 0 && uv[0] < 1 && uv[1] >= 0 && uv[1] < 1) {
            std::size_t pixel = index(uv[1] * height) * width + index(uv[0] * width), low = 0, high = runs.size() / 2;
            while (low < high) {
                auto mid = (low + high) / 2; if (ngRigUnsigned(runs[mid * 2]) <= pixel) low = mid + 1; else high = mid;
            }
            if (low) {
                auto start = static_cast<std::size_t>(ngRigUnsigned(runs[(low - 1) * 2]));
                auto length = static_cast<std::size_t>(ngRigUnsigned(runs[(low - 1) * 2 + 1]));
                if (pixel - start < length) { ++hits; if (occupancy) (*occupancy)[pointIndex] = true; }
            }
        }
        break;
    }
    return static_cast<double>(hits) / points.size();
}
compat::Slice<std::uint8_t> ngRigLargestAlphaComponent(compat::ConstSlice<std::uint8_t> rgba,
    std::size_t width, std::size_t height, Context context) {
    imageSize(rgba.size(), width, height,322); compat::Slice<bool> visited; visited.resize(width * height);
    compat::Slice<std::size_t> largest;
    for (std::size_t start = 0; start < visited.size(); ++start) {
        if (start % 65536 == 0) ngRigCheckpoint(context);
        if (visited[start] || rgba[start * 4 + 3] <= 32) continue;
        compat::Slice<std::size_t> queue{start}; visited[start] = true;
        for (std::size_t cursor = 0; cursor < visited.size(); ++cursor) {
            if (cursor >= queue.size()) break;
            if (cursor % 65536 == 0) ngRigCheckpoint(context);
            auto pixel = queue[cursor], x = pixel % width, y = pixel / width;
            for (auto ny = y > 0 ? y - 1 : 0; ny < std::min(height, y + 2); ++ny)
                for (auto nx = x > 0 ? x - 1 : 0; nx < std::min(width, x + 2); ++nx) {
                    auto neighbor = ny * width + nx;
                    if (!visited[neighbor] && rgba[neighbor * 4 + 3] > 32) { visited[neighbor] = true; queue.append(neighbor); }
                }
        }
        if (queue.size() > largest.size()) largest = queue;
    }
    compat::Slice<std::uint8_t> result; result.resize(rgba.size());
    for (auto pixel : largest) result[pixel * 4 + 3] = rgba[pixel * 4 + 3]; return result;
}
double ngRigAlphaPerimeterCoverage(compat::ConstSlice<std::uint8_t> rgba, std::size_t width, std::size_t height) {
    imageSize(rgba.size(), width, height,350);
    std::size_t band = std::max<std::size_t>(1, index(std::min(width, height) * .04)), opaque = 0, total = 0;
    for (std::size_t y = 0; y < height; ++y) for (std::size_t x = 0; x < width; ++x) {
        std::size_t copies = (y < band ? 1 : 0) + (y >= height - band ? 1 : 0) + (x < band ? 1 : 0) + (x >= width - band ? 1 : 0);
        total += copies; if (rgba[(y * width + x) * 4 + 3] > 32) opaque += copies;
    }
    return static_cast<double>(opaque) / total;
}
compat::Slice<Point2> ngRigTextureSupport(compat::Slice<std::uint8_t> rgba, std::size_t width, std::size_t height,
    Json mesh, std::size_t maximumSamples, Context context) {
    imageSize(rgba.size(), width, height,365);
    auto vertices = ngRigPoints(at(mesh, "vertices")), uv = ngRigPoints(at(mesh, "uv")); auto triangles = at(mesh, "triangles").as_array();
    require(vertices.size() == uv.size() && !triangles.empty() && maximumSamples > 0, "Invalid imported UV mesh",369);
    struct UVTriangle { std::array<std::size_t, 3> indices{}; Point2 a, b, c;
        double determinant=std::numeric_limits<double>::quiet_NaN(); };
    compat::Slice<UVTriangle> prepared;
    for (auto triangle : triangles) {
        auto values = ngRigNumbers(triangle); require(values.size() == 3, "Invalid imported UV triangle",374); UVTriangle item;
        for (std::size_t i = 0; i < values.size(); ++i) {
            double value = values[i]; require(value >= 0 && value < vertices.size() && value == index(value), "UV index out of range",377);
            item.indices[i] = index(value);
        }
        item.a = uv[item.indices[0]]; item.b = uv[item.indices[1]]; item.c = uv[item.indices[2]];
        item.determinant = (item.b[0] - item.a[0]) * (item.c[1] - item.a[1]) - (item.b[1] - item.a[1]) * (item.c[0] - item.a[0]);
        if (std::abs(item.determinant) >= 1e-14) prepared.append(item);
    }
    std::size_t count = 0;
    for (std::size_t pixel = 0; pixel < width * height; ++pixel) {
        if (pixel % 65536 == 0) ngRigCheckpoint(context); if (rgba[pixel * 4 + 3] > 32) ++count;
    }
    std::size_t stride = std::max<std::size_t>(1, (count + maximumSamples - 1) / maximumSamples), ordinal = 0;
    compat::Slice<Point2> result;
    for (std::size_t pixel = 0; pixel < width * height; ++pixel) {
        if (pixel % 65536 == 0) ngRigCheckpoint(context);
        if (rgba[pixel * 4 + 3] <= 32) continue; if (ordinal++ % stride) continue;
        Point2 p{(pixel % width + .5) / width, (pixel / width + .5) / height};
        for (auto triangle : prepared) {
            auto ids = triangle.indices; auto a = triangle.a, b = triangle.b, c = triangle.c; double determinant = triangle.determinant;
            double u = ((p[0] - a[0]) * (c[1] - a[1]) - (p[1] - a[1]) * (c[0] - a[0])) / determinant;
            double v = ((b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])) / determinant;
            if (u < -1e-8 || v < -1e-8 || u + v > 1 + 1e-8) continue;
            result.append(Point2{(1 - u - v) * vertices[ids[0]][0] + u * vertices[ids[1]][0] + v * vertices[ids[2]][0],
                (1 - u - v) * vertices[ids[0]][1] + u * vertices[ids[1]][1] + v * vertices[ids[2]][1]}); break;
        }
    }
    return result;
}
}
