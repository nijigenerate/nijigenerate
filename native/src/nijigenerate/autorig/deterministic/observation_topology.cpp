#include "detail/observation.hpp"
#include <array>
namespace nijigenerate::autorig::deterministic::observation {
using namespace detail;
Json ngRigEyelashTopology(Json material, Point2 origin, Point2 tangent, Context context) {
    auto dimensions = ngRigNumbers(at(material, "texture_size"));
    std::size_t width = index(dimensions[0]), height = index(dimensions[1]);
    require(width > 0 && height > 0 && width <= std::numeric_limits<std::size_t>::max() / height, "Invalid eyelash alpha dimensions",16);
    compat::Slice<bool> mask; mask.resize(width * height); auto runs = at(material, "alpha_runs_32").as_array();
    for (std::size_t i = 0; i < runs.size(); i += 2) {
        auto start = static_cast<std::size_t>(ngRigUnsigned(runs[i])), length = static_cast<std::size_t>(ngRigUnsigned(runs[i + 1]));
        require(start <= mask.size() && length <= mask.size() - start, "Invalid eyelash alpha run",21);
        for (std::size_t j = start; j < start + length; ++j) mask[j] = true;
    }
    auto radius = alphaDistance(mask, width, height, true, context); auto skeleton = mask.duplicate();
    auto pixel = [&](std::ptrdiff_t x, std::ptrdiff_t y) {
        return x >= 0 && y >= 0 && static_cast<std::size_t>(x) < width && static_cast<std::size_t>(y) < height
            && skeleton[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)];
    };
    bool changed = true; std::size_t iterations = 0;
    while (changed) {
        ngRigCheckpoint(context); changed = false; ++iterations;
        for (int phase = 0; phase < 2; ++phase) {
            compat::Slice<std::size_t> deleted;
            for (std::size_t y = 0; y < height; ++y) for (std::size_t x = 0; x < width; ++x) {
                if (!skeleton[y * width + x]) continue;
                auto px = static_cast<std::ptrdiff_t>(x), py = static_cast<std::ptrdiff_t>(y);
                std::array<bool, 8> n{pixel(px, py - 1), pixel(px + 1, py - 1), pixel(px + 1, py), pixel(px + 1, py + 1),
                    pixel(px, py + 1), pixel(px - 1, py + 1), pixel(px - 1, py), pixel(px - 1, py - 1)};
                int count = 0, transitions = 0;
                for (std::size_t i = 0; i < n.size(); ++i) { count += n[i]; transitions += !n[i] && n[(i + 1) % 8]; }
                if (count < 2 || count > 6 || transitions != 1) continue;
                bool first = phase == 0 ? n[0] && n[2] && n[4] : n[0] && n[2] && n[6];
                bool second = phase == 0 ? n[2] && n[4] && n[6] : n[0] && n[4] && n[6];
                if (!first && !second) deleted.append(y * width + x);
            }
            for (auto id : deleted) skeleton[id] = false; changed = changed || !deleted.empty();
        }
    }
    auto mapping = at(material, "source_root_mapping");
    auto vertices = ngRigPoints(at(mapping, "vertices")), uv = ngRigPoints(at(mapping, "uv")); linear::RigMatrix design, values;
    for (std::size_t i = 0; i < uv.size(); ++i) {
        auto p = uv[i]; design.append(linear::RigRow{p[0], p[1], 1.}); values.append(linear::RigRow{vertices[i][0], vertices[i][1]});
    }
    auto affine = linear::ngRigLeastSquares(design, values);
    auto world = [&](std::size_t id) {
        double u = (id % width + .5) / width, v = (id / width + .5) / height;
        return Point2{u * affine[0][0] + v * affine[1][0] + affine[2][0], u * affine[0][1] + v * affine[1][1] + affine[2][1]};
    };
    auto neighbors = [&](std::size_t id) {
        compat::Slice<std::size_t> result; auto x = static_cast<std::ptrdiff_t>(id % width), y = static_cast<std::ptrdiff_t>(id / width);
        for (int dy = -1; dy < 2; ++dy) for (int dx = -1; dx < 2; ++dx) {
            if ((!dx && !dy) || !pixel(x + dx, y + dy)) continue;
            if (dx && dy && (pixel(x + dx, y) || pixel(x, y + dy))) continue;
            result.append(static_cast<std::size_t>(y + dy) * width + static_cast<std::size_t>(x + dx));
        }
        return result;
    };
    std::size_t endpoints = 0, junctions = 0; core::settings::SettingsJsonArray branches; bool left = false, right = false;
    for (std::size_t id = 0; id < skeleton.size(); ++id) if (skeleton[id]) {
        if (id % 65536 == 0) ngRigCheckpoint(context); auto links = neighbors(id);
        if (links.size() >= 3) ++junctions; if (links.size() != 1) continue; ++endpoints;
        std::size_t previous = id, current = links[0]; double length = 0; auto start = world(id);
        for (;;) {
            auto a = world(previous), b = world(current); length += std::sqrt(square(a[0] - b[0]) + square(a[1] - b[1]));
            auto adjacent = neighbors(current); if (adjacent.size() != 2) break;
            auto next = adjacent[0] == previous ? adjacent[1] : adjacent[0]; previous = current; current = next;
        }
        auto end = world(current);
        double tx = (start[0] - end[0]) * tangent[0] + (start[1] - end[1]) * tangent[1];
        double ny = -(start[0] - end[0]) * tangent[1] + (start[1] - end[1]) * tangent[0];
        auto sx = world(current % width + 1 < width ? current + 1 : current), sy = world(current + width < skeleton.size() ? current + width : current);
        double pixelScale = std::max(std::sqrt(square(sx[0] - end[0]) + square(sx[1] - end[1])),
            std::sqrt(square(sy[0] - end[0]) + square(sy[1] - end[1])));
        bool lash = neighbors(current).size() >= 3 && std::abs(ny) > std::abs(tx) && length > 2 * radius[current] * pixelScale;
        double coordinate = (end[0] - origin[0]) * tangent[0] + (end[1] - origin[1]) * tangent[1];
        if (lash) { if (coordinate < 0) left = true; else right = true; }
        core::settings::SettingsJsonObject branch; branch["endpoint"] = pointJson(start); branch["attachment"] = pointJson(end);
        branch["length"] = length; branch["lash_branch"] = lash; branch["attachment_tangent"] = coordinate; branches.append(Json(branch));
    }
    core::settings::SettingsJsonObject result; result["thinning_iterations"] = iterations; result["endpoints"] = endpoints;
    result["junctions"] = junctions; result["branches"] = branches; result["outer_left"] = left; result["outer_right"] = right;
    result["medial_resolved"] = left != right; result["inner_left"] = right && !left; return result;
}
}
