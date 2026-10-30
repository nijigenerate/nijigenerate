#include "detail/observation.hpp"
namespace nijigenerate::autorig::deterministic::observation {
using namespace detail;
namespace {
compat::Slice<double> distanceLine(compat::Slice<double> values) {
    compat::Slice<double> distance, boundary; distance.resize(values.size(),std::numeric_limits<double>::quiet_NaN());
    boundary.resize(values.size()+1,std::numeric_limits<double>::quiet_NaN());
    compat::Slice<std::size_t> envelope; envelope.resize(values.size());
    std::size_t last = 0; boundary[0] = -std::numeric_limits<double>::infinity(); boundary[1] = std::numeric_limits<double>::infinity();
    for (std::size_t q = 1; q < values.size(); ++q) {
        double intersection;
        for (;;) {
            auto v = envelope[last];
            intersection = (values[q] + static_cast<double>(q) * q - values[v] - static_cast<double>(v) * v) / (2. * (q - v));
            if (intersection > boundary[last] || last == 0) break; --last;
        }
        ++last; envelope[last] = q; boundary[last] = intersection; boundary[last + 1] = std::numeric_limits<double>::infinity();
    }
    last = 0;
    for (std::size_t q = 0; q < values.size(); ++q) {
        while (boundary[last + 1] < q) ++last;
        double delta = static_cast<double>(q) - envelope[last]; distance[q] = delta * delta + values[envelope[last]];
    }
    return distance;
}
}
compat::Slice<double> detail::alphaDistance(compat::Slice<bool> mask, std::size_t width, std::size_t height,
    bool foreground, Context context) {
    compat::Slice<double> result; result.resize(mask.size(),std::numeric_limits<double>::quiet_NaN());
    for (std::size_t y = 0; y < height; ++y) {
        ngRigCheckpoint(context); compat::Slice<double> line; line.resize(width,std::numeric_limits<double>::quiet_NaN());
        for (std::size_t x = 0; x < width; ++x) line[x] = mask[y * width + x] == foreground ? 1e20 : 0.;
        auto transformed = distanceLine(line); for (std::size_t x = 0; x < width; ++x) result[y * width + x] = transformed[x];
    }
    for (std::size_t x = 0; x < width; ++x) {
        if (x % 64 == 0) ngRigCheckpoint(context); compat::Slice<double> line; line.resize(height,std::numeric_limits<double>::quiet_NaN());
        for (std::size_t y = 0; y < height; ++y) line[y] = result[y * width + x]; line = distanceLine(line);
        for (std::size_t y = 0; y < height; ++y) result[y * width + x] = std::sqrt(line[y]);
    }
    return result;
}
Json ngRigAlphaContour(Json material, Context context, bool nominalPixelFrame) {
    auto dimensions = ngRigNumbers(at(material, "texture_size")); std::size_t width = index(dimensions[0]), height = index(dimensions[1]);
    require(width > 1 && height > 1 && width <= std::numeric_limits<std::size_t>::max() / height, "Invalid contour texture dimensions",150);
    compat::Slice<bool> mask; mask.resize(width * height); std::size_t opaque = 0; auto runs = at(material, "alpha_runs_32").as_array();
    for (std::size_t i = 0; i < runs.size(); i += 2) {
        auto start = static_cast<std::size_t>(ngRigUnsigned(runs[i])), count = static_cast<std::size_t>(ngRigUnsigned(runs[i + 1]));
        require(start <= mask.size() && count <= mask.size() - start, "Invalid alpha support run",155);
        for (std::size_t j = start; j < start + count; ++j) mask[j] = true; opaque += count;
    }
    auto outside = alphaDistance(mask, width, height, false, context), inside = alphaDistance(mask, width, height, true, context);
    for (std::size_t i = 0; i < outside.size(); ++i) outside[i] -= inside[i];
    auto mapping = at(material, "source_root_mapping");
    auto vertices = ngRigPoints(at(mapping, "vertices")), uv = ngRigPoints(at(mapping, "uv")); linear::RigMatrix design, world;
    const auto vertexOwner=vertices.retentionOwner();
    for (std::size_t i = 0; i < uv.size(); ++i) {
        auto p = uv[i]; design.append(linear::RigRow{p[0] * width, p[1] * height, 1.});
        world.append(linear::RigRow::retainedView(vertexOwner,vertices[i].data(),Point2::size()));
    }
    auto affine = linear::ngRigLeastSquares(design, world);
    if (nominalPixelFrame) {
        auto bounds = ngRigNumbers(at(material, "bounds"));
        affine = {{(bounds[2] - bounds[0]) / (width - 1), 0.}, {0., (bounds[3] - bounds[1]) / (height - 1)}, {bounds[0], bounds[1]}};
    }
    double determinant = affine[0][0] * affine[1][1] - affine[0][1] * affine[1][0];
    require(std::abs(determinant) > 1e-12, "Singular contour frame",172); compat::Slice<Point2> points, normals;
    for (std::size_t y = 0; y < height; ++y) for (std::size_t x = 0; x < width; ++x) {
        auto pixel = y * width + x; if (!mask[pixel]) continue;
        if (x > 0 && y > 0 && x + 1 < width && y + 1 < height && mask[pixel - 1] && mask[pixel + 1]
            && mask[pixel - width] && mask[pixel + width]) continue;
        double dx = x == 0 ? outside[pixel + 1] - outside[pixel] : x + 1 == width ? outside[pixel] - outside[pixel - 1]
            : (outside[pixel + 1] - outside[pixel - 1]) / 2;
        double dy = y == 0 ? outside[pixel + width] - outside[pixel] : y + 1 == height ? outside[pixel] - outside[pixel - width]
            : (outside[pixel + width] - outside[pixel - width]) / 2;
        Point2 normal{(dx * affine[1][1] - dy * affine[0][1]) / determinant, (dy * affine[0][0] - dx * affine[1][0]) / determinant};
        double length = std::max(1e-12, std::sqrt(square(normal[0]) + square(normal[1]))); for (auto& v : normal) v /= length;
        points.append(Point2{(x + .5) * affine[0][0] + (y + .5) * affine[1][0] + affine[2][0],
            (x + .5) * affine[0][1] + (y + .5) * affine[1][1] + affine[2][1]}); normals.append(normal);
    }
    double xx = square(affine[0][0]) + square(affine[0][1]), yy = square(affine[1][0]) + square(affine[1][1]);
    double xy = affine[0][0] * affine[1][0] + affine[0][1] * affine[1][1];
    double step = std::sqrt(std::max(0., (xx + yy - std::sqrt(square(xx - yy) + 4 * xy * xy)) / 2));
    require(step > 1e-8, "Degenerate source pixel frame",193); core::settings::SettingsJsonObject result;
    result["points"] = ngRigPointsJson(points); result["normals"] = ngRigPointsJson(normals);
    result["pixel_step"] = step; result["opaque_pixels"] = opaque; return result;
}
}
