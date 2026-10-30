#include "controls.hpp"
#include "controls_dependencies.hpp"
#include "controls_displacement.hpp"
#include "controls_profile.hpp"
#include "controls_blink.hpp"
#include "geometry.hpp"
#include "detail/physics.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <nijigenerate/io/imageexport_case.hpp>
#include <common/resource_key_map.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::autorig::deterministic::controls {
namespace {
using Json = core::settings::SettingsJsonValue;
using Array = core::settings::SettingsJsonArray;
using Object = core::settings::SettingsJsonObject;
using Point2 = contracts::Point2;
using controls_detail::Frame;
using controls_detail::readControlMap;
template<class T> using Slice = compat::Slice<T>;
struct IdHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        constexpr auto shift = sizeof(std::size_t) < sizeof(value) ? sizeof(std::size_t) * 8 : 0;
        return static_cast<std::size_t>(value ^ (shift ? value >> shift : 0));
    }
};
struct StringHash {
    std::size_t operator()(const std::string& value) const noexcept {
        return compat::ngPhobosStringHash(value);
    }
};
template<class V> using IdMap = nicxlive::core::common::ResourceKeyMap<std::uint64_t, V,
    IdHash, std::equal_to<std::uint64_t>>;
using EyeMap = nicxlive::core::common::ResourceKeyMap<std::string, Frame,
    StringHash, std::equal_to<std::string>>;

Json read(Json value, std::string_view key) { return value.sourceIndexValue(key); }
std::string string(Json value) { return std::string(value.as_string().view()); }
Json record(std::initializer_list<std::pair<const char*, Json>> values) {
    Object result;
    for (const auto& [key, value] : values) result[key] = value;
    return Json(result);
}
Json numbers(Slice<double> values) {
    Array result;
    for (const double value : values) result.append(Json(value));
    return Json(result);
}
Json point(Point2 value) { return Json(Array{Json(value[0]), Json(value[1])}); }
Json key(std::size_t x, std::size_t y) { return Json(Array{Json(x), Json(y)}); }
template<std::size_t N>
void enforce(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) throw compat::ThrowableException(contracts::detail::copyContractMessage(std::string_view(message,N-1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), line);
}
double clamp(double value, double lower, double upper) {
    if (value < lower) return lower;
    if (value > upper) return upper;
    return value;
}
double percentile(Slice<double> values, double fraction) {
    return controls_detail::percentile(std::span<const double>(values.data(), values.size()), fraction);
}
double sample(controls_detail::Profile& profile, Slice<double> values, double x) {
    return profile.sample(std::span<const double>(values.data(), values.size()), x);
}
std::string lower(Json value) {
    return io::imageexport::detail::lowerExtension(value.as_string().view());
}
std::string feature(Json material) {
    if (const auto* classified = material.sourceObjectRef().if_contains("feature")) {
        auto kind = string(*classified);
        return kind.starts_with("mouth") ? "mouth" : kind;
    }
    auto name = lower(read(material, "name"));
    std::replace(name.begin(), name.end(), '-', '_');
    std::replace(name.begin(), name.end(), ':', '_');
    const compat::RetainedString retainedName(std::move(name));
    const std::pair<const char*, compat::RetainedStringSlice> patterns[]{
        {"sclera", compat::RetainedString::fromStaticLiteral("(sclera|eye_white)")},
        {"iris", compat::RetainedString::fromStaticLiteral("(iris|pupil)")},
        {"upper", compat::RetainedString::fromStaticLiteral("(lash_upper|upper_lash|upper_lid)")},
        {"lower", compat::RetainedString::fromStaticLiteral("(lash_lower|lower_lash|lower_lid)")},
        {"corner", compat::RetainedString::fromStaticLiteral("(eye_corner|canthus)")},
        {"fold", compat::RetainedString::fromStaticLiteral("(lid_fold|eyelid_fold)")},
        {"brow", compat::RetainedString::fromStaticLiteral("(brow|eyebrow)")},
        {"mouth", compat::RetainedString::fromStaticLiteral("(mouth|lip|tongue|teeth)")}};
    for (const auto& [kind, pattern] : patterns)
        if (physics::detail::physicsRegexSearch(compat::RetainedStringSlice(retainedName), pattern)) return kind;
    return "";
}
} // namespace

Json ngRigCompileControls(Json state, std::shared_ptr<framework::AutoRigTaskContext> context) {
    auto materials = read(state, "materials").as_array(), targets = read(state, "targets").as_array();
    IdMap<Json> materialById;
    for (const auto material : materials) materialById[contracts::ngRigUnsigned(read(material, "uuid"))] = material;
    const auto supports = [&](std::string_view kind, std::string_view side, bool landmarks = false) {
        Slice<Point2> points;
        for (const auto target : targets) {
            auto material = readControlMap(materialById, contracts::ngRigUnsigned(read(target, "part")), 205);
            if (feature(material) == kind && (side.empty() || read(target, "side").as_string().view() == side)) {
                const auto* landmark = material.sourceObjectRef().if_contains("landmark_cloud");
                points.append(contracts::ngRigPoints(!landmarks || landmark == nullptr ?
                    read(material, "cloud") : *landmark));
            }
        }
        return points;
    };
    EyeMap eyes;
    for (const auto* side : {"R", "L"}) {
        auto white = supports("sclera", side, true);
        if (!white.empty()) eyes[side] = controls_detail::eyeFrame(white);
    }
    IdMap<bool> sharedBrows;
    if (eyes.size() == 2) for (const auto target : targets) {
        auto material = readControlMap(materialById, contracts::ngRigUnsigned(read(target, "part")), 220);
        if (feature(material) != "brow") continue;
        std::size_t right = 0, left = 0;
        auto cloud = contracts::ngRigPoints(read(material, "cloud"));
        for (const auto p : cloud) {
            const auto distance = [p](Frame eye) {
                const double x = p[0] - eye.origin[0], y = p[1] - eye.origin[1];
                return x * x + y * y;
            };
            if (distance(readControlMap(eyes, "R", 225)) <= distance(readControlMap(eyes, "L", 225))) ++right;
            else ++left;
        }
        if (right >= cloud.size() * .05 && left >= cloud.size() * .05)
            sharedBrows[contracts::ngRigUnsigned(read(target, "part"))] = true;
    }
    Array mechanisms, masks;
    const auto mechanism = [&](std::string name, Slice<double> xs, Slice<double> ys,
        Slice<std::string> features, std::string side, Frame frame, auto displacement) {
        Array operations;
        for (const auto target : targets) {
            auto material = readControlMap(materialById, contracts::ngRigUnsigned(read(target, "part")), 234);
            auto kind = feature(material);
            bool selected = false;
            for (const auto& candidate : features) if (candidate == kind) selected = true;
            const bool sharedBrow = kind == "brow" && sharedBrows.contains(contracts::ngRigUnsigned(read(target, "part")));
            if (!selected || (!side.empty() && read(target, "side").as_string().view() != side && !sharedBrow)) continue;
            auto positions = contracts::ngRigPoints(read(target, "world"));
            auto matrix = contracts::ngRigNumbers(read(target, "root_to_local_direction"));
            enforce(matrix.size() == 4, "Missing native facial frame transform", 240);
            Array keys;
            for (std::size_t y = 0; y < ys.size(); ++y) for (std::size_t x = 0; x < xs.size(); ++x) {
                const double vy = ys[y], vx = xs[x];
                contracts::ngRigCheckpoint(context);
                Slice<double> offsets;
                for (const auto p : positions) {
                    auto d = displacement(kind, frame.local(p), vx, vy, material);
                    if (sharedBrow) {
                        auto other = readControlMap(eyes, side == "R" ? "L" : "R", 248).origin;
                        Point2 axis{frame.origin[0] - other[0], frame.origin[1] - other[1]};
                        const double distance = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1]);
                        for (auto& value : axis) value /= distance;
                        const Point2 midpoint{(frame.origin[0] + other[0]) / 2, (frame.origin[1] + other[1]) / 2};
                        const double weight = clamp(.5 + 2 * ((p[0] - midpoint[0]) * axis[0] +
                            (p[1] - midpoint[1]) * axis[1]) / distance, 0., 1.);
                        for (auto& value : d) value *= weight;
                    }
                    const Point2 world{d[0] * frame.tangent[0] + d[1] * frame.normal[0],
                        d[0] * frame.tangent[1] + d[1] * frame.normal[1]};
                    offsets.append(matrix[0] * world[0] + matrix[1] * world[1]);
                    offsets.append(matrix[2] * world[0] + matrix[3] * world[1]);
                }
                if (!name.ends_with("::Blink")) offsets = ngRigProjectControlOrientation(
                    contracts::ngRigPoints(read(read(target, "mapping"), "vertices")),
                    read(read(target, "mapping"), "triangles"), offsets, context);
                for (auto& offset : offsets) offset = std::round(offset * 100000) / 100000;
                if (!name.ends_with("::Blink")) enforce(geometry::ngRigTriangleMinimumRatio(
                    contracts::ngRigPoints(read(read(target, "mapping"), "vertices")),
                    read(read(target, "mapping"), "triangles"), offsets) >= .002,
                    "Rounded local control has a folded triangle", 263);
                if (vx == 0 && vy == 0) for (const double offset : offsets)
                    enforce(std::abs(offset) <= 1e-8, "Local control changed the neutral pose", 267);
                keys.append(record({{"key", key(x, y)}, {"offsets", numbers(offsets)}}));
            }
            operations.append(record({{"part", read(target, "part")}, {"keys", Json(keys)}}));
        }
        if (!operations.empty()) mechanisms.append(record({{"name", Json(name)}, {"axisX", numbers(xs)},
            {"axisY", numbers(ys)}, {"operations", Json(operations)}}));
    };
    if (contracts::ngRigString(state, "kind", "humanoid").view() == "local") {
        Slice<Point2> support;
        for (const auto target : targets)
            support.append(contracts::ngRigPoints(read(readControlMap(materialById,
                contracts::ngRigUnsigned(read(target, "part")), 277), "cloud")));
        auto localFrame = controls_detail::frame(support);
        Point2 mean{0., 0.};
        for (const auto p : support) { mean[0] += p[0] / support.size(); mean[1] += p[1] / support.size(); }
        localFrame.origin = mean;
        double minimum = std::numeric_limits<double>::infinity(), maximum = -std::numeric_limits<double>::infinity();
        for (const auto p : support) {
            const double x = localFrame.local(p)[0];
            minimum = std::min(minimum, x); maximum = std::max(maximum, x);
        }
        localFrame.width = std::max(1e-8, maximum - minimum);
        Slice<std::string> features;
        for (const auto target : targets) features.append(feature(readControlMap(materialById,
            contracts::ngRigUnsigned(read(target, "part")), 286)));
        mechanism("Local::Bend", {-1., -.5, 0., .5, 1.}, {-1., 0., 1.}, features, "", localFrame,
            [&](std::string_view, Point2 p, double x, double y, Json) {
                return controls_detail::localBendDisplacement(p, x, y, localFrame);
            });
    }
    for (const std::string side : {"R", "L"}) {
        auto white = supports("sclera", side);
        if (white.empty()) continue;
        auto eye = readControlMap(eyes, side, 300);
        auto aperture = controls_detail::profile(white, eye);
        auto upper = supports("upper", side), lowerSupport = supports("lower", side);
        auto upperContact = aperture.upper.dup(), lowerContact = aperture.lower.dup();
        std::uint64_t whiteOwner = 0;
        for (const auto target : targets) if (read(target, "side").as_string().view() == side &&
                feature(readControlMap(materialById, contracts::ngRigUnsigned(read(target, "part")), 305)) == "sclera") {
            whiteOwner = contracts::ngRigUnsigned(read(target, "part")); break;
        }
        auto downClose = controls_detail::contactProfile(
            contracts::ngRigPoints(read(readControlMap(materialById, whiteOwner, 306), "cloud")), eye, true);
        Slice<double> flatClose, expressionOffset, smileClose;
        for (std::size_t i = 0; i < downClose.size(); ++i) {
            const double down = downClose[i], t = static_cast<double>(i) / (downClose.size() - 1);
            const double flat = downClose[0] * (1 - t) + downClose[downClose.size() - 1] * t;
            flatClose.append(flat); expressionOffset.append(.3 * (down - flat)); smileClose.append(flat - .3 * (down - flat));
        }
        smileClose = controls_detail::endpointCubic(smileClose);
        bool hasReference = false;
        Point2 reference;
        if (eyes.size() == 2) { reference = readControlMap(eyes, side == "R" ? "L" : "R", 316).origin; hasReference = true; }
        else {
            auto mouthSupport = supports("mouth", "");
            if (!mouthSupport.empty()) { reference = controls_detail::frame(mouthSupport).origin; hasReference = true; }
        }
        const auto raiseInnerCorner = [&](bool innerLeft) {
            for (std::size_t i = 0; i < smileClose.size(); ++i) {
                const double t = static_cast<double>(i) / (smileClose.size() - 1);
                const double factor = innerLeft ? 1 - t : t;
                smileClose[i] += eye.width * .08 * (factor * factor * factor);
            }
        };
        if (hasReference) raiseInnerCorner(eye.local(reference)[0] < 0);
        bool upperContainsLower = false;
        std::uint64_t upperOwner = 0;
        std::size_t greatest = 0;
        for (const auto target : targets) if (read(target, "side").as_string().view() == side &&
                feature(readControlMap(materialById, contracts::ngRigUnsigned(read(target, "part")), 331)) == "upper") {
            auto cloud = contracts::ngRigPoints(read(readControlMap(materialById,
                contracts::ngRigUnsigned(read(target, "part")), 332), "cloud"));
            std::size_t score = 0;
            for (const auto p : cloud) if (std::abs(eye.local(p)[0]) <= eye.width / 2) ++score;
            if (score > greatest) { greatest = score; upperOwner = contracts::ngRigUnsigned(read(target, "part")); }
        }
        if (upperOwner) {
            Slice<Point2> above, below;
            double low = std::numeric_limits<double>::infinity(), high = -std::numeric_limits<double>::infinity();
            for (const auto original : contracts::ngRigPoints(read(readControlMap(materialById, upperOwner, 339), "cloud"))) {
                const auto p = eye.local(original);
                const double middle = (sample(aperture, aperture.upper, p[0]) + sample(aperture, aperture.lower, p[0])) / 2;
                if (p[1] <= middle) above.append(original);
                else if (std::abs(p[0]) <= eye.width / 2) { below.append(original); low = std::min(low, p[0]); high = std::max(high, p[0]); }
            }
            upperContact = controls_detail::contactProfile(above, eye, true);
            upperContainsLower = below.size() > 1 && high - low > eye.width * .25;
            if (upperContainsLower) lowerContact = controls_detail::contactProfile(below, eye, false);
        }
        Json topology{Object{}};
        if (upperOwner && readControlMap(materialById, upperOwner, 350).sourceObjectRef().if_contains("alpha_runs_32")) {
            topology = observation::ngRigEyelashTopology(
                readControlMap(materialById, upperOwner, 351), eye.origin, eye.tangent, context);
            if (!hasReference && read(topology, "medial_resolved").as_bool())
                raiseInnerCorner(read(topology, "inner_left").as_bool());
        }
        if (!lowerSupport.empty()) lowerContact = controls_detail::contactProfile(lowerSupport, eye, false);
        Slice<double> heights;
        for (std::size_t i = 0; i < aperture.axis.size(); ++i) heights.append(aperture.lower[i] - aperture.upper[i]);
        const double height = percentile(heights, .5);
        const auto blinkName = "Eye::" + side + "::Blink";
        mechanism(blinkName, {0., .25, .5, .75, 1.}, {-1., 0., 1.}, {"sclera", "upper", "lower", "corner", "fold"}, side, eye,
            [&](std::string_view kind, Point2 p, double blink, double expression, Json) {
                return controls_detail::blinkDisplacement(kind, p, blink, expression, eye, aperture,
                    flatClose, smileClose, expressionOffset, upperContact, lowerContact);
            });
        if (!mechanisms.empty() && read(mechanisms[mechanisms.size() - 1], "name").as_string().view() == blinkName)
            mechanisms[mechanisms.size() - 1].as_object()["contact_curves"] = record({
                {"tangent", numbers(aperture.axis)}, {"frame_origin", point(eye.origin)}, {"frame_tangent", point(eye.tangent)},
                {"upper_lower_edge", numbers(upperContact)}, {"lower_upper_edge", numbers(lowerContact)},
                {"white_boundary_owner", Json(whiteOwner)}, {"upper_owner", Json(upperOwner)},
                {"upper_part_contains_lower_band", Json(upperContainsLower)}, {"neutral_target", numbers(flatClose)},
                {"open_sclera_lower_reference", numbers(downClose)}, {"smile_target", numbers(smileClose)},
                {"expression_range", Json(.3)}, {"eyelash_topology", topology}});
        mechanism("Eye::" + side + "::X-Y", {-1., -.5, 0., .5, 1.}, {-1., 0., 1.}, {"iris"}, side, eye,
            [&](std::string_view, Point2, double x, double y, Json) { return controls_detail::irisDisplacement(x, y, eye, height); });
        mechanism("Eyebrow::" + side, {-1., -.5, 0., .5, 1.}, {-1., 0., 1.}, {"brow"}, side, eye,
            [&](std::string_view, Point2 p, double x, double y, Json) { return controls_detail::eyebrowDisplacement(p, x, y, eye); });
        std::uint64_t owner = 0;
        for (const auto target : targets) if (read(target, "side").as_string().view() == side &&
                feature(readControlMap(materialById, contracts::ngRigUnsigned(read(target, "part")), 391)) == "sclera") {
            owner = contracts::ngRigUnsigned(read(target, "part")); break;
        }
        for (const auto target : targets) if (read(target, "side").as_string().view() == side &&
                feature(readControlMap(materialById, contracts::ngRigUnsigned(read(target, "part")), 394)) == "iris")
            masks.append(record({{"part", read(target, "part")}, {"source", Json(owner)}}));
    }
    auto mouth = supports("mouth", "");
    if (!mouth.empty()) {
        Slice<Point2> base;
        for (const auto target : targets) {
            auto material = readControlMap(materialById, contracts::ngRigUnsigned(read(target, "part")), 401);
            if (contracts::ngRigString(material, "feature", "").view() == "mouth")
                base.append(contracts::ngRigPoints(read(material, "cloud")));
        }
        if (base.empty()) for (const auto target : targets) {
            auto material = readControlMap(materialById, contracts::ngRigUnsigned(read(target, "part")), 405);
            if (contracts::ngRigString(material, "feature", "").view() == "mouth_outline")
                base.append(contracts::ngRigPoints(read(material, "cloud")));
        }
        if (base.empty()) base = mouth;
        Frame mouthFrame;
        mouthFrame.tangent = Point2{1., 0.};
        if (eyes.size() == 2) {
            const auto r = readControlMap(eyes, "R", 411).origin, l = readControlMap(eyes, "L", 411).origin;
            mouthFrame.tangent = Point2{l[0] - r[0], l[1] - r[1]};
            const double length = std::sqrt(mouthFrame.tangent[0] * mouthFrame.tangent[0] + mouthFrame.tangent[1] * mouthFrame.tangent[1]);
            for (auto& value : mouthFrame.tangent) value /= length;
        }
        mouthFrame.normal = Point2{-mouthFrame.tangent[1], mouthFrame.tangent[0]};
        Slice<double> stations, y;
        for (const auto p : base) { const auto q = mouthFrame.local(p); stations.append(q[0]); y.append(q[1]); }
        const double low = percentile(stations, .01), high = percentile(stations, .99), middle = percentile(y, .5);
        mouthFrame.width = high - low;
        mouthFrame.origin = Point2{(low + high) / 2 * mouthFrame.tangent[0] + middle * mouthFrame.normal[0],
            (low + high) / 2 * mouthFrame.tangent[1] + middle * mouthFrame.normal[1]};
        const double height = std::max(percentile(y, 1) - percentile(y, 0), mouthFrame.width * .01);
        mechanism("Mouth::Open", {-1., -.5, 0., .5, 1.}, {-1., 0., 1.}, {"mouth"}, "", mouthFrame,
            [&](std::string_view, Point2 p, double x, double expression, Json material) {
                const compat::RetainedString name(lower(read(material, "name")));
                const bool rigid = physics::detail::physicsRegexSearch(compat::RetainedStringSlice(name),
                    compat::RetainedString::fromStaticLiteral("(tongue|teeth)"));
                return controls_detail::mouthDisplacement(p, x, expression, material, rigid, mouthFrame, height);
            });
        if (!mechanisms.empty() && read(mechanisms[mechanisms.size() - 1], "name").as_string().view() == "Mouth::Open")
            mechanisms[mechanisms.size() - 1].as_object()["source_frame"] = record({
                {"origin", point(mouthFrame.origin)}, {"tangent", point(mouthFrame.tangent)}, {"width", Json(mouthFrame.width)}});
    }
    Array drawOrder, overlappingBackings;
    IdMap<double> desiredZ;
    for (const auto target : targets) desiredZ[contracts::ngRigUnsigned(read(target, "part"))] =
        contracts::ngRigNumber(contracts::ngRigGet(target, "absolute_zsort", Json(0.)));
    for (const auto target : targets) {
        const auto faceId = contracts::ngRigUnsigned(read(target, "part"));
        auto face = readControlMap(materialById, faceId, 447);
        if (contracts::ngRigString(face, "role", "").view() != "face") continue;
        auto compositeScope = contracts::ngRigGet(target, "composite_scope", Json(Array{}));
        Slice<std::uint64_t> supported;
        double highest = -std::numeric_limits<double>::infinity();
        for (const auto candidate : targets) {
            auto material = readControlMap(materialById, contracts::ngRigUnsigned(read(candidate, "part")), 453);
            if (feature(material).empty() || contracts::ngRigGet(candidate, "composite_scope", Json(Array{})) != compositeScope) continue;
            const auto* strong = material.sourceObjectRef().if_contains("draw_order_cloud");
            if (strong == nullptr) continue;
            if (observation::ngRigAlphaCoverage(face, contracts::ngRigPoints(*strong), "alpha_runs_128", nullptr) > .5) {
                supported.append(contracts::ngRigUnsigned(read(candidate, "part")));
                highest = std::max(highest, readControlMap(desiredZ, contracts::ngRigUnsigned(read(candidate, "part")), 458));
            }
        }
        if (supported.empty()) continue;
        const double next = std::max(readControlMap(desiredZ, faceId, 462), highest + .25);
        const auto change = [&](Json node, double value, const char* reason) {
            const auto id = contracts::ngRigUnsigned(read(node, "part"));
            const double original = contracts::ngRigNumber(contracts::ngRigGet(node, "absolute_zsort", Json(0.)));
            const double relative = contracts::ngRigNumber(contracts::ngRigGet(node, "relative_zsort", Json(0.)));
            drawOrder.append(record({{"part", read(node, "part")}, {"absolute_zsort", Json(value)},
                {"relative_zsort", Json(value - original + relative)}, {"reason", Json(reason)}}));
            desiredZ[id] = value;
        };
        if (next != readControlMap(desiredZ, faceId, 471)) change(target, next, "skin behind independent facial mechanisms");
        const auto* faceCloud = face.sourceObjectRef().if_contains("draw_order_cloud");
        if (faceCloud == nullptr) continue;
        for (const auto candidate : targets) {
            auto ear = readControlMap(materialById, contracts::ngRigUnsigned(read(candidate, "part")), 475);
            if (contracts::ngRigString(ear, "role", "").view() != "ear" ||
                    contracts::ngRigGet(candidate, "composite_scope", Json(Array{})) != compositeScope) continue;
            const double coverage = observation::ngRigAlphaCoverage(ear, contracts::ngRigPoints(*faceCloud), "alpha_runs_128", nullptr);
            if (coverage <= .5) continue;
            overlappingBackings.append(record({{"ear", read(candidate, "part")}, {"face", read(target, "part")},
                {"face_alpha_coverage", Json(coverage)}}));
            const double value = std::max(readControlMap(desiredZ, contracts::ngRigUnsigned(read(candidate, "part")), 482),
                readControlMap(desiredZ, faceId, 482) + .25);
            if (value != readControlMap(desiredZ, contracts::ngRigUnsigned(read(candidate, "part")), 483))
                change(candidate, value, "ear artwork covering skin stays behind the separate skin");
        }
    }
    return record({{"schema_version", Json("rig-controls-d/1")}, {"mechanisms", Json(mechanisms)},
        {"draw_order", Json(drawOrder)}, {"overlapping_ear_backings", Json(overlappingBackings)},
        {"masks", Json(masks)}, {"source_sha256", read(state, "source_sha256")}});
}

} // namespace nijigenerate::autorig::deterministic::controls
