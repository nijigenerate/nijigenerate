#include "detail/observation.hpp"
#include <common/resource_key_map.hpp>
#include <nijigenerate/compat/range_error.hpp>
namespace nijigenerate::autorig::deterministic::observation {
using namespace detail;
namespace {
struct SideIdHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        constexpr auto shift = sizeof(value) > sizeof(std::size_t) ? sizeof(std::size_t) * 8 : 0;
        return static_cast<std::size_t>(value ^ (shift ? value >> shift : 0));
    }
};
}
Json ngRigPrepareShoulders(Json state, Json program, Context context) {
    core::settings::SettingsJsonArray pairs;
    if (ngRigString(program,compat::RetainedString::fromStaticLiteral("kind"),
            compat::RetainedString::fromStaticLiteral("humanoid")) != "humanoid") return pairs;
    Json body, bodyContour; std::size_t greatest = 0;
    for (auto material : at(state, "materials").as_array())
        if (!at(material, "static").as_bool() && at(material, "role").as_string() == "torso") {
            auto support = object(material).if_contains("alpha_runs_32"); if (!support) continue;
            std::size_t count = 0; auto runs = support->as_array();
            for (std::size_t i = 1; i < runs.size(); i += 2) count += static_cast<std::size_t>(ngRigUnsigned(runs[i]));
            if (count > greatest) { greatest = count; body = material; }
        }
    if (!greatest) return pairs; bodyContour = ngRigAlphaContour(body, context, true);
    auto evidence = at(state, "evidence");
    auto landmark = [&](std::string_view name) {
        auto p = ngRigPoint(at(at(at(evidence, "landmarks"), name), "xy"));
        auto matrix = at(evidence, "source_to_model").as_array(); auto x = ngRigNumbers(matrix[0]), y = ngRigNumbers(matrix[1]);
        return Point2{x[0] * p[0] + x[1] * p[1] + x[2], y[0] * p[0] + y[1] * p[1] + y[2]};
    };
    auto neck = landmark("neck_base"), chest = landmark("chest"); Point2 up{neck[0] - chest[0], neck[1] - chest[1]};
    double norm = std::sqrt(square(up[0]) + square(up[1])); require(norm > 0, "Unresolved shoulder up axis",222);
    for (auto& v : up) v /= norm; Point2 tangent{-up[1], up[0]};
    nicxlive::core::common::ResourceKeyMap<std::uint64_t, compat::RetainedStringSlice,
        SideIdHash, std::equal_to<std::uint64_t>> sides;
    for (auto carrier : at(program, "carriers").as_array())
        sides[ngRigUnsigned(at(carrier, "part"))] = at(carrier, "side").as_string();
    for (auto material : at(state, "materials").as_array()) {
        if (at(material, "static").as_bool() || at(material, "role").as_string() != "arm"
            || !object(material).if_contains("alpha_runs_32")) continue;
        auto sideEntry = sides.find(ngRigUnsigned(at(material, "uuid")));
        if (sideEntry == sides.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/observation.d"),228);
        auto side = sideEntry->second; if (side != "L" && side != "R") continue;
        const auto originName = contracts::detail::allocateContractStorage([&] {
            return "shoulder." + std::string(side.view());
        });
        const auto origin = landmark(originName);
        const auto elbowName = contracts::detail::allocateContractStorage([&] {
            return "elbow." + std::string(side.view());
        });
        const auto elbow = landmark(elbowName);
        Point2 axis{elbow[0] - origin[0], elbow[1] - origin[1]}; double length = std::sqrt(square(axis[0]) + square(axis[1]));
        require(length > 0, "Degenerate upper arm",232); for (auto& v : axis) v /= length;
        auto region = [&](Json contour) {
            compat::Slice<Point2> result; auto points = ngRigPoints(at(contour, "points")), normals = ngRigPoints(at(contour, "normals"));
            for (std::size_t i = 0; i < points.size(); ++i) {
                auto p = points[i]; Point2 delta{p[0] - origin[0], p[1] - origin[1]};
                if (std::sqrt(square(delta[0]) + square(delta[1])) < .30 * length && delta[0] * axis[0] + delta[1] * axis[1] < .12 * length
                    && normals[i][0] * up[0] + normals[i][1] * up[1] > .5) result.append(p);
            }
            return result;
        };
        auto proximal = region(ngRigAlphaContour(material, context, true)), torso = region(bodyContour);
        std::size_t matched = 0; double low = std::numeric_limits<double>::infinity(), high = -std::numeric_limits<double>::infinity();
        for (auto p : proximal) {
            double nearest = std::numeric_limits<double>::infinity();
            for (auto q : torso) nearest = std::min(nearest, square(p[0] - q[0]) + square(p[1] - q[1]));
            if (nearest < square(.07 * length)) {
                ++matched; double station = p[0] * tangent[0] + p[1] * tangent[1]; low = std::min(low, station); high = std::max(high, station);
            }
        }
        double fraction = proximal.empty() ? 0. : static_cast<double>(matched) / proximal.size(), span = matched ? high - low : 0.;
        core::settings::SettingsJsonObject pair; pair["source"] = at(body, "uuid"); pair["target"] = at(material, "uuid");
        pair["side"] = Json(core::settings::SettingsString(side));
        pair["matching"] = fraction >= .6 && span >= .18 * length; pair["matched_fraction"] = fraction; pair["matched_span"] = span;
        pair["upper_arm_length"] = length; pair["origin"] = pointJson(origin); pair["up"] = pointJson(up);
        pair["tangent"] = pointJson(tangent); pair["arm_axis"] = pointJson(axis); pairs.append(Json(pair));
    }
    return pairs;
}
}
