#include "controls.hpp"
#include "controls_numeric.hpp"
#include "controls_skin.hpp"
#include "controls_projection.hpp"
#include "observation.hpp"
#include "registered.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <common/resource_key_map.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <string_view>

namespace nijigenerate::autorig::deterministic::controls {
namespace {
using Json = core::settings::SettingsJsonValue;
using Array = core::settings::SettingsJsonArray;
using Object = core::settings::SettingsJsonObject;
using Point2 = contracts::Point2;
using Point3 = contracts::Point3;
using controls_detail::Frame;
using controls_detail::SkinProfile;
using controls_detail::sourceControlIndex;
using controls_detail::readControlMap;
template<class T> using Slice = compat::Slice<T>;
struct ControlUuidHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        // Source integral hashing folds high bits only when wider than size_t.
        if constexpr (sizeof(std::size_t) == 4)
            return static_cast<std::size_t>(value ^ (value >> 32));
        return static_cast<std::size_t>(value);
    }
};
using ControlUuidMap = nicxlive::core::common::ResourceKeyMap<std::uint64_t, Json,
    ControlUuidHash, std::equal_to<std::uint64_t>>;
Json read(Json value, std::string_view key) { return value.sourceIndexValue(key); }
Json record(std::initializer_list<std::pair<const char*, Json>> values) {
    Object result;
    for (const auto& [key, value] : values) result[key] = value;
    return Json(result);
}
template<class T> Json array(Slice<T> values) {
    Array result;
    for (const auto value : values) result.append(Json(value));
    return Json(result);
}
Json point(Point2 p) { return Json(Array{Json(p[0]), Json(p[1])}); }
template<std::size_t N>
void enforce(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) throw compat::ThrowableException(contracts::detail::copyContractMessage(std::string_view(message,N-1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), line);
}
double curve(Slice<double> axis, Slice<double> values, double x) {
    return controls_detail::curveSample(std::span<const double>(axis.data(), axis.size()),
        std::span<const double>(values.data(), values.size()), x);
}
double square(double value) { return value * value; }
} // namespace

Json ngRigCompileCheekCorrections(Json state, Json program, std::shared_ptr<framework::AutoRigTaskContext> context) {
    Array operations, observations;
    if (contracts::ngRigString(state, "kind", "humanoid").view() != "humanoid")
        return record({{"applicable", Json(false)}, {"operations", Json(operations)}});
    const auto keys = read(state, "native_head_keys").as_array();
    enforce(contracts::ngRigDigest(Json(keys)).view() == read(state, "depth_angle_program_sha256").as_string().view(),
        "Native head bake ownership mismatch", 626);
    ControlUuidMap materials, targets;
    for (const auto material : read(state, "materials").as_array())
        materials[contracts::ngRigUnsigned(read(material, "uuid"))] = material;
    std::uint64_t skin = 0;
    std::size_t greatest = 0;
    for (const auto target : read(state, "targets").as_array()) {
        const auto id = contracts::ngRigUnsigned(read(target, "part")); targets[id] = target;
        if (read(readControlMap(materials, id, 632), "role").as_string().view() != "face") continue;
        std::size_t count = 0;
        const auto runs = read(readControlMap(materials, id, 633), "alpha_runs_32").as_array();
        for (std::size_t i = 1; i < runs.size(); i += 2) count += contracts::ngRigUnsigned(runs[i]);
        if (count > greatest) { greatest = count; skin = id; }
    }
    enforce(greatest > 0, "Cheek correction needs observed face skin", 637);
    skin = contracts::ngRigUnsigned(read(read(program, "hierarchy"), "face_origin"));
    enforce(targets.contains(skin) && read(readControlMap(materials, skin, 639), "role").as_string().view() == "face",
        "Cheek correction needs the hierarchy face origin", 639);
    Slice<Frame> eyes;
    Frame mouthFrame;
    bool hasMouth = false;
    auto geometry = contracts::ngRigGet(state, "control_geometry", read(state, "controls"));
    for (const auto mechanism : read(geometry, "mechanisms").as_array()) {
        const auto name = read(mechanism, "name").as_string();
        if (name.view().ends_with("::Blink")) {
            const auto curves = read(mechanism, "contact_curves");
            Frame eye;
            eye.origin = contracts::ngRigPoint(read(curves, "frame_origin"));
            eye.tangent = contracts::ngRigPoint(read(curves, "frame_tangent"));
            eye.normal = Point2{-eye.tangent[1], eye.tangent[0]};
            const auto axis = contracts::ngRigNumbers(read(curves, "tangent"));
            eye.width = axis[axis.size() - 1] - axis[0]; eyes.append(eye);
        }
        if (name.view() == "Mouth::Open") {
            const auto source = read(mechanism, "source_frame");
            mouthFrame.origin = contracts::ngRigPoint(read(source, "origin"));
            mouthFrame.tangent = contracts::ngRigPoint(read(source, "tangent"));
            mouthFrame.width = contracts::ngRigNumber(read(source, "width")); hasMouth = true;
        }
    }
    enforce(!eyes.empty() && hasMouth, "Cheek correction needs observed eye and mouth anchors", 655);
    SkinProfile profile;
    for (const auto eye : eyes) {
        profile.frame.origin[0] += eye.origin[0] / eyes.size(); profile.frame.origin[1] += eye.origin[1] / eyes.size();
    }
    profile.frame.tangent = eyes.size() >= 2 ? Point2{eyes[eyes.size() - 1].origin[0] - eyes[0].origin[0],
        eyes[eyes.size() - 1].origin[1] - eyes[0].origin[1]} : eyes[0].tangent;
    const double length = std::sqrt(square(profile.frame.tangent[0]) + square(profile.frame.tangent[1]));
    enforce(length > 0, "Degenerate cheek feature frame", 660);
    for (auto& value : profile.frame.tangent) value /= length;
    if (profile.frame.tangent[0] < 0) for (auto& value : profile.frame.tangent) value *= -1;
    profile.frame.normal = Point2{-profile.frame.tangent[1], profile.frame.tangent[0]};
    const auto contour = observation::ngRigAlphaContour(readControlMap(materials, skin, 663), context);
    auto cloud = contracts::ngRigPoints(read(contour, "points"));
    Slice<Point2> local;
    double minimumY = std::numeric_limits<double>::infinity(), maximumY = -std::numeric_limits<double>::infinity();
    for (const auto p : cloud) {
        const auto q = profile.frame.local(p); local.append(q);
        minimumY = std::min(minimumY, q[1]); maximumY = std::max(maximumY, q[1]);
    }
    const double step = contracts::ngRigNumber(read(contour, "pixel_step"));
    const std::size_t count = sourceControlIndex(std::round((maximumY - minimumY) / step)) + 1;
    Slice<double> left, right;
    left.resize(count, std::numeric_limits<double>::infinity());
    right.resize(count, -std::numeric_limits<double>::infinity());
    for (const auto p : local) {
        const auto bin = std::min(count - 1, sourceControlIndex(std::round((p[1] - minimumY) / step)));
        left[bin] = std::min(left[bin], p[0]); right[bin] = std::max(right[bin], p[0]);
    }
    for (std::size_t i = 0; i < count; ++i) if (right[i] > left[i]) {
        profile.y.append(minimumY + i * step); profile.left.append(left[i]); profile.right.append(right[i]);
    }
    enforce(profile.y.size() >= 2, "Unresolved face skin sections", 677);
    profile.eyeSpan = Point2{std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    profile.mouthSpan = Point2{std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    for (const auto eye : eyes) for (const int sign : {-1, 1}) {
        const Point2 p{eye.origin[0] + sign * eye.tangent[0] * eye.width / 2,
            eye.origin[1] + sign * eye.tangent[1] * eye.width / 2};
        const auto q = profile.frame.local(p); profile.eye += q[1] / (eyes.size() * 2);
        profile.eyeSpan[0] = std::min(profile.eyeSpan[0], q[0]); profile.eyeSpan[1] = std::max(profile.eyeSpan[1], q[0]);
    }
    for (const int sign : {-1, 1}) {
        const Point2 p{mouthFrame.origin[0] + sign * mouthFrame.tangent[0] * mouthFrame.width / 2,
            mouthFrame.origin[1] + sign * mouthFrame.tangent[1] * mouthFrame.width / 2};
        const auto q = profile.frame.local(p); profile.mouth += q[1] / 2;
        profile.mouthSpan[0] = std::min(profile.mouthSpan[0], q[0]); profile.mouthSpan[1] = std::max(profile.mouthSpan[1], q[0]);
    }
    profile.temple = std::numeric_limits<double>::infinity();
    Slice<Point2> anchors;
    anchors.resize(1);
    anchors[0] = contracts::ngRigPoint(read(readControlMap(targets, skin, 691), "origin"));
    const auto facial = registered::ngRigFacialLandmarks(state);
    for (const auto& [name, anchor] : facial) anchors.append(anchor);
    for (const auto& [id, material] : materials) {
        const auto feature = contracts::ngRigString(material, "feature", "");
        if (!targets.contains(id)) continue;
        const auto kind = feature.view();
        if (kind == "brow" || kind == "corner" || kind == "fold" || kind == "iris" || kind == "lower" ||
            kind == "sclera" || kind == "upper" || kind == "mouth")
            anchors.append(contracts::ngRigPoint(read(readControlMap(targets, id, 698), "origin")));
        if (kind == "brow" || kind == "upper") for (const auto p : contracts::ngRigPoints(read(material, "cloud")))
            profile.temple = std::min(profile.temple, profile.frame.local(p)[1]);
    }
    enforce(profile.temple < profile.eye && profile.eye < profile.mouth && profile.mouth < profile.y[profile.y.size() - 1],
        "Observed temple, eye, mouth and chin order is unresolved", 702);
    auto skinTarget = readControlMap(targets, skin, 704);
    auto skinWorld = contracts::ngRigPoints(read(skinTarget, "world"));
    auto skinLocal = contracts::ngRigPoints(read(read(skinTarget, "mapping"), "vertices"));
    auto skinTriangles = read(read(skinTarget, "mapping"), "triangles");
    Slice<bool> pinned; pinned.resize(skinWorld.size(), false);
    for (const auto triangle : skinTriangles.as_array()) {
        auto indices = contracts::ngRigNumbers(triangle);
        const auto a = skinLocal[sourceControlIndex(indices[0])], b = skinLocal[sourceControlIndex(indices[1])],
            c = skinLocal[sourceControlIndex(indices[2])];
        if (std::abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) < 1e-10) continue;
        for (const auto anchor : anchors) {
            const auto weights = controls_detail::barycentric(anchor, skinWorld[sourceControlIndex(indices[0])],
                skinWorld[sourceControlIndex(indices[1])], skinWorld[sourceControlIndex(indices[2])]);
            if (weights[0] >= -1e-7 && weights[1] >= -1e-7 && weights[2] >= -1e-7)
                for (const double index : indices) pinned[sourceControlIndex(index)] = true;
        }
    }
    auto matrix = contracts::ngRigNumbers(read(skinTarget, "parent_to_root"));
    const double m00 = matrix[0];
    const double m11 = matrix[4];
    const double m01 = matrix[1];
    const double m10 = matrix[3];
    const double determinant = m00 * m11 - m01 * m10;
    const auto gridLocal = [&](Point2 p) { return controls_detail::gridLocal(p, matrix, determinant); };
    auto xs = contracts::ngRigNumbers(read(skinTarget, "xs")), ys = contracts::ngRigNumbers(read(skinTarget, "ys"));
    Slice<Point2> depths;
    for (const double depth : contracts::ngRigNumbers(read(skinTarget, "depth"))) depths.append(Point2{depth, 0.});
    const double depthScale = contracts::ngRigNumber(read(program, "native_depth_scale"));
    for (const auto key : keys) {
        if (read(key, "grid") != read(skinTarget, "grid")) continue;
        contracts::ngRigCheckpoint(context);
        const auto projection = contracts::ngRigNumbers(read(key, "projection"));
        enforce(projection.size() == 8, "Missing native XYZ projection", 728);
        const Point3 depthAxis{projection[2] * projection[5] - projection[4] * projection[3],
            projection[4] * projection[1] - projection[0] * projection[5], projection[0] * projection[3] - projection[2] * projection[1]};
        const double norm = std::sqrt(square(depthAxis[0]) + square(depthAxis[1]) + square(depthAxis[2]));
        enforce(norm > 1e-12, "Degenerate viewing-depth axis", 731);
        const double slope = (depthAxis[0] * profile.frame.tangent[0] + depthAxis[1] * profile.frame.tangent[1]) / norm;
        const int side = std::abs(slope) <= 1e-6 ? 0 : slope > 0 ? 1 : -1;
        const auto values = contracts::ngRigNumbers(read(key, "offsets"));
        Slice<Point2> offsets;
        for (std::size_t i = 0; i < values.size(); i += 2) offsets.append(Point2{values[i], values[i + 1]});
        const auto projected = [&](Point2 section) {
            return controls_detail::projected(section, profile, matrix, determinant, offsets, xs, ys);
        };
        const auto allowedSkin = controls_detail::cheekSupport(skinWorld, skinTriangles, profile, side);
        Slice<Point2> skinField;
        for (std::size_t i = 0; i < skinWorld.size(); ++i) {
            const auto world = skinWorld[i];
            Point2 result{0., 0.};
            const auto p = profile.frame.local(world);
            const double lo = curve(profile.y, profile.left, p[1]), hi = curve(profile.y, profile.right, p[1]);
            const double vertical = controls_detail::smoothStep((p[1] - profile.temple) / (profile.eye - profile.temple)) *
                controls_detail::smoothStep((profile.y[profile.y.size() - 1] - p[1]) / (profile.y[profile.y.size() - 1] - profile.mouth));
            if (side && allowedSkin[i] && !pinned[i] && hi > lo && vertical > 0) {
                const Point2 a{lo, p[1]}, b{hi, p[1]};
                const auto pa = projected(a), pb = projected(b);
                Slice<double> sectionDepth;
                for (std::size_t station = 0; station < 129; ++station) {
                    const double t = station / 128.;
                    const Point2 localSection{lo + (hi - lo) * t, p[1]};
                    const Point2 wp{profile.frame.origin[0] + localSection[0] * profile.frame.tangent[0] + p[1] * profile.frame.normal[0],
                        profile.frame.origin[1] + localSection[0] * profile.frame.tangent[1] + p[1] * profile.frame.normal[1]};
                    sectionDepth.append(controls_detail::gridSample(depths, xs, ys, gridLocal(wp))[0] * depthScale);
                }
                double numerator = 0, denominator = 0;
                for (std::size_t station = 0; station < sectionDepth.size(); ++station) {
                    const double z = sectionDepth[station], t = station / 128.;
                    const double ellipse = std::sqrt(std::max(0., 1 - square(2 * t - 1)));
                    numerator += (z - sectionDepth[0] * (1 - t) - sectionDepth[sectionDepth.size() - 1] * t) * ellipse;
                    denominator += ellipse * ellipse;
                }
                const double radius = std::max(0., numerator / denominator);
                const Point2 u{(pb[0] - pa[0]) / 2, (pb[1] - pa[1]) / 2}, v{radius * projection[4], radius * projection[5]};
                const double len = std::sqrt(square(u[0]) + square(u[1]));
                if (len > 1e-8 && radius > 0) {
                    const Point2 direction{u[0] / len, u[1] / len};
                    const double du = len, dv = v[0] * direction[0] + v[1] * direction[1];
                    const double divisor = std::max(1e-8, std::sqrt(du * du + dv * dv));
                    const Point2 edge = side < 0 ? pa : pb;
                    const double levels[2]{profile.eye, profile.mouth};
                    const double spans[2]{profile.eyeSpan[side < 0 ? 0 : 1], profile.mouthSpan[side < 0 ? 0 : 1]};
                    const double sampled = controls_detail::curveSample(std::span<const double>(levels), std::span<const double>(spans), p[1]);
                    const double inner = side < 0 ? std::max(lo, sampled) : std::min(hi, sampled);
                    const double weight = side < 0 ? controls_detail::smoothStep((inner - p[0]) / std::max(1e-8, inner - lo)) :
                        controls_detail::smoothStep((p[0] - inner) / std::max(1e-8, hi - inner));
                    for (std::size_t axis = 0; axis < 2; ++axis) result[axis] = ((pa[axis] + pb[axis]) / 2 +
                        side * (u[axis] * du + v[axis] * dv) / divisor - edge[axis]) * weight * vertical;
                }
            }
            const auto skinInverse = contracts::ngRigNumbers(read(skinTarget, "root_to_local_direction"));
            const Point2 rounded{std::rint((skinInverse[0] * result[0] + skinInverse[1] * result[1]) * 1e5) / 1e5,
                std::rint((skinInverse[2] * result[0] + skinInverse[3] * result[1]) * 1e5) / 1e5};
            const double skinDeterminant = skinInverse[0] * skinInverse[3] - skinInverse[1] * skinInverse[2];
            skinField.append(Point2{(skinInverse[3] * rounded[0] - skinInverse[1] * rounded[1]) / skinDeterminant,
                (skinInverse[0] * rounded[1] - skinInverse[2] * rounded[0]) / skinDeterminant});
        }
        for (const auto& [id, target] : targets) {
            if (read(readControlMap(materials, id, 787), "role").as_string().view() != "face") continue;
            const auto world = contracts::ngRigPoints(read(target, "world"));
            const auto triangles = read(read(target, "mapping"), "triangles");
            const auto allowed = controls_detail::cheekSupport(world, triangles, profile, side);
            const auto inverse = contracts::ngRigNumbers(read(target, "root_to_local_direction"));
            Slice<double> displacement;
            double maximum = 0;
            for (std::size_t i = 0; i < world.size(); ++i) {
                const auto p = world[i];
                Point2 field{0., 0.};
                if (id == skin) field = skinField[i];
                else {
                    double nearest = std::numeric_limits<double>::infinity();
                    for (std::size_t vertex = 0; vertex < skinWorld.size(); ++vertex) {
                        const auto q = skinWorld[vertex];
                        const double distance = square(p[0] - q[0]) + square(p[1] - q[1]);
                        if (distance < nearest) { nearest = distance; field = skinField[vertex]; }
                    }
                    for (const auto triangle : skinTriangles.as_array()) {
                        const auto index = contracts::ngRigNumbers(triangle);
                        const std::array<std::size_t, 3> ids{sourceControlIndex(index[0]), sourceControlIndex(index[1]), sourceControlIndex(index[2])};
                        const auto a = skinWorld[ids[0]], b = skinWorld[ids[1]], c = skinWorld[ids[2]];
                        if (std::abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) < 1e-10) continue;
                        const auto weights = controls_detail::barycentric(p, skinWorld[ids[0]], skinWorld[ids[1]], skinWorld[ids[2]]);
                        if (!(weights[0] >= -1e-7 && weights[1] >= -1e-7 && weights[2] >= -1e-7)) continue;
                        Point2 candidate{0., 0.};
                        for (std::size_t j = 0; j < 3; ++j) for (std::size_t axis = 0; axis < 2; ++axis)
                            candidate[axis] += skinField[ids[j]][axis] * weights[j];
                        field = candidate; break;
                    }
                }
                Point2 localDelta{inverse[0] * field[0] + inverse[1] * field[1], inverse[2] * field[0] + inverse[3] * field[1]};
                if (!allowed[i] || (id == skin && pinned[i])) localDelta = Point2{0., 0.};
                for (auto& delta : localDelta) {
                    enforce(std::isfinite(delta), "Nonfinite cheek residual", 818);
                    delta = std::rint(delta * 1e5) / 1e5; maximum = std::max(maximum, std::abs(delta)); displacement.append(delta);
                }
            }
            observations.append(record({{"part", read(target, "part")}, {"key", read(key, "key")}, {"near_side", Json(side)},
                {"near_depth_slope", Json(slope)}, {"maximum_local_residual", Json(maximum)}}));
            operations.append(record({{"part", read(target, "part")}, {"key", read(key, "key")}, {"values", array(displacement)},
                {"allowed_vertices", array(allowed)}, {"near_side", Json(side)}}));
        }
    }
    auto report = record({{"applicable", Json(true)}, {"mechanism", Json("alpha_near_cheek_section_envelope_v4")},
        {"skin", Json(skin)}, {"operations", Json(operations)}, {"observations", Json(observations)}, {"skin_protected_vertices", array(pinned)},
        {"profile", record({{"origin", point(profile.frame.origin)}, {"tangent", point(profile.frame.tangent)},
            {"y", array(profile.y)}, {"left", array(profile.left)}, {"right", array(profile.right)}, {"temple_y", Json(profile.temple)},
            {"eye_y", Json(profile.eye)}, {"mouth_y", Json(profile.mouth)}})},
        {"program_sha256", read(program, "content_sha256")}, {"depth_angle_program_sha256", read(state, "depth_angle_program_sha256")},
        {"source_uv_program_sha256", read(state, "source_uv_program_sha256")}, {"source_sha256", read(state, "source_sha256")},
        {"visually_reviewed", Json(false)}});
    report.as_object()["content_sha256"] = core::settings::SettingsString(contracts::ngRigDigest(report));
    return report;
}

} // namespace nijigenerate::autorig::deterministic::controls
