#include "controls.hpp"
#include "cheek_options.hpp"
#include "detail/head_template_data.hpp"
#include "detail/head_surface_ridge.hpp"
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
#include <map>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

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
double smootherStep(double value) {
    const double t = std::clamp(value, 0., 1.);
    return t*t*t*(t*(6*t - 15) + 10);
}

// Suppress staircase noise from the alpha contour without moving the chin tip
// or averaging across the two sides of an asymmetric face.
void smoothSilhouette(SkinProfile& profile, double radius) {
    for (auto* edge : {&profile.left, &profile.right}) {
        const auto source = edge->duplicate();
        for (std::size_t i = 1; i + 1 < source.size(); ++i) {
            const auto first = std::lower_bound(profile.y.begin(), profile.y.end(), profile.y[i] - radius);
            const auto last = std::upper_bound(first, profile.y.end(), profile.y[i] + radius);
            double sum = 0, weight = 0;
            for (auto row = first; row != last; ++row) {
                const double k = square(std::max(0., 1. - square((*row-profile.y[i])/radius)));
                sum += k * source[static_cast<std::size_t>(row-profile.y.begin())];
                weight += k;
            }
            const double taper = smootherStep(std::min(profile.y[i]-profile.y[0],
                profile.y[profile.y.size()-1]-profile.y[i])/radius);
            (*edge)[i] = std::lerp(source[i], sum/weight, taper);
        }
    }
}

// Measure the inward turn from cheek to jaw in the neutral skin silhouette.
// A rounded jaw can have a broad turn, so require a localized peak as well
// as agreement at two scales.
std::optional<double> jawAngleRow(const SkinProfile& profile, int side, double step) {
    const auto& edge = side < 0 ? profile.left : profile.right;
    const double chin = profile.y[profile.y.size()-1];
    const double span = chin-profile.temple;
    const double window = std::max(4.*step,.08*span);
    const auto turn = [&](double y, double w) {
        const double before = curve(profile.y,edge,y-w);
        const double at = curve(profile.y,edge,y);
        const double after = curve(profile.y,edge,y+w);
        return -side*(std::atan2(after-at,w)-std::atan2(at-before,w));
    };
    const double start = std::max(profile.eye+window,profile.temple+2.*window);
    const double end = chin-2.*window;
    std::optional<double> best;
    double bestTurn = .20;
    for (const double y : profile.y) {
        if (y < start || y > end) continue;
        const double local = turn(y,window);
        if (local <= bestTurn || turn(y,2.*window) <= local ||
            local-std::max(turn(y-window,window),turn(y+window,window)) < .055) continue;
        best = y;
        bestTurn = local;
    }
    return best;
}

} // namespace

static Json compileCheekCorrections(Json state, Json program,
    const std::shared_ptr<framework::AutoRigTaskContext>& context,
    bool measureReference) {
    Array operations, observations;
    if (contracts::ngRigString(state, compat::RetainedString::fromStaticLiteral("kind"),
            compat::RetainedString::fromStaticLiteral("humanoid")).view() != "humanoid")
        return record({{"applicable", Json(false)}, {"operations", Json(operations)}});
    const double jawRidgeBlend = contracts::ngRigNumber(contracts::ngRigGet(
        contracts::ngRigGet(state, compat::RetainedString::fromStaticLiteral("options"), Json(Object{})),
        compat::RetainedString::fromStaticLiteral(jawRidgeBlendOption), Json(defaultJawRidgeBlend)));
    enforce(jawRidgeBlend >= 0. && jawRidgeBlend <= 1., "jaw_ridge_blend must be between 0 and 1", __LINE__);
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
    auto geometry = contracts::ngRigGet(state, compat::RetainedString::fromStaticLiteral("control_geometry"),
        read(state, "controls"));
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
        const auto feature = contracts::ngRigString(material, compat::RetainedString::fromStaticLiteral("feature"),
            compat::RetainedString::fromStaticLiteral(""));
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
    const auto fittedSurface=program.sourceObjectRef().if_contains("head_fitted_surface");
    std::vector<Point3> fittedMesh;
    std::array<std::vector<Point3>,2> lowerSections;
    std::array<std::array<Point3,4>,2> fittedJoints{};
    if (fittedSurface) {
        const auto points = [](Json value) {
            std::vector<Point3> result;
            for (const auto& p : value.sourceArrayValue()) {
                const auto coordinates=contracts::ngRigNumbers(p);
                if (coordinates.size()!=3) return std::vector<Point3>{};
                result.push_back({coordinates[0],coordinates[1],coordinates[2]});
            }
            return result;
        };
        const auto joint = [&](const char* key) {
            const auto p=contracts::ngRigNumbers(read(*fittedSurface,key));
            return Point3{p[0],p[1],p[2]};
        };
        fittedMesh=points(read(*fittedSurface,"points"));
        if (const auto left=fittedSurface->sourceObjectRef().if_contains("left_lower_section")) {
            lowerSections={points(*left),points(read(*fittedSurface,"right_lower_section"))};
        }
        fittedJoints={{{joint("left_upper"),joint("left_ear"),joint("left_jaw"),joint("chin")},
            {joint("right_upper"),joint("right_ear"),joint("right_jaw"),joint("chin")}}};
        if (fittedMesh.size()==head_surface::rowCount*head_surface_reference::columns)
            profile.temple=profile.y[0];
    }
    smoothSilhouette(profile, std::max(2*step, (profile.mouth-profile.eye)/6));
    auto skinTarget = readControlMap(targets, skin, 704);
    auto skinWorld = contracts::ngRigPoints(read(skinTarget, "world"));
    auto skinLocal = contracts::ngRigPoints(read(read(skinTarget, "mapping"), "vertices"));
    auto skinTriangles = read(read(skinTarget, "mapping"), "triangles");
    struct ContourSample {
        Point2 section;
        std::array<std::size_t, 3> vertices{};
        Point3 weights{};
        bool mapped = false;
    };
    std::array<std::vector<ContourSample>, 2> contourSamples;
    for (std::size_t sideIndex = 0; sideIndex < contourSamples.size(); ++sideIndex) {
        const auto& edge = sideIndex == 0 ? profile.left : profile.right;
        for (std::size_t i = 0; i < profile.y.size(); ++i)
            contourSamples[sideIndex].push_back({{edge[i], profile.y[i]}});
    }
    const double chin = profile.y[profile.y.size()-1];
    const std::array jawRows{jawAngleRow(profile,-1,step),jawAngleRow(profile,1,step)};
    const Point2 chinTip{std::midpoint(profile.left[profile.left.size()-1],
        profile.right[profile.right.size()-1]), chin};
    const auto toRoot = [&](Point2 p) {
        return Point2{profile.frame.origin[0]+p[0]*profile.frame.tangent[0]+p[1]*profile.frame.normal[0],
                      profile.frame.origin[1]+p[0]*profile.frame.tangent[1]+p[1]*profile.frame.normal[1]};
    };
    // Feature Parts retain their own bindings. Keep skin fixed under the
    // interior features, but let it follow the ridge where a feature's
    // artwork reaches the cheek boundary.
    Slice<Point2> interiorAnchors;
    for (const auto anchor : anchors) {
        const auto p = profile.frame.local(anchor);
        const double middle = profile.midline(p[1]);
        const auto& sideEdge = p[0] < middle ? profile.left : profile.right;
        const double width = std::abs(curve(profile.y,sideEdge,p[1])-middle);
        if (std::abs(p[0]-middle) <= .62*width)
            interiorAnchors.append(anchor);
    }
    anchors = interiorAnchors;
    anchors.append(toRoot(chinTip));
    anchors.append(toRoot({profile.left[profile.left.size()-1],chin}));
    anchors.append(toRoot({profile.right[profile.right.size()-1],chin}));
    anchors.append(toRoot({curve(profile.y,profile.left,profile.temple),profile.temple}));
    anchors.append(toRoot({curve(profile.y,profile.right,profile.temple),profile.temple}));
    const auto originalCount = skinWorld.size();
    if (measureReference) {
        // Evaluate the original correction with its original contour sampling.
        // The temporary triangles are used only for that evaluation. The
        // reference is transferred to the authored Part in the second pass.
        using Triangle = std::array<std::size_t,3>;
        std::vector<Triangle> samplingTriangles;
        for (const auto& triangle : skinTriangles.as_array()) {
            const auto ids = contracts::ngRigNumbers(triangle);
            samplingTriangles.push_back({sourceControlIndex(ids[0]),sourceControlIndex(ids[1]),sourceControlIndex(ids[2])});
        }
        skinWorld = skinWorld.duplicate();
        skinLocal = skinLocal.duplicate();
        const auto splitSamplingTriangle = [&](Point2 section) {
            const Point2 world = toRoot(section);
            for (std::size_t i=0;i<samplingTriangles.size();++i) {
                const auto triangle = samplingTriangles[i];
                const auto weights = controls_detail::barycentric(world,skinWorld[triangle[0]],
                    skinWorld[triangle[1]],skinWorld[triangle[2]]);
                // An edge shared by two triangles needs a different split.
                if (!(weights[0]>1e-5 && weights[1]>1e-5 && weights[2]>1e-5)) continue;
                Point2 local{0.,0.};
                for (std::size_t j=0;j<3;++j)
                    for (std::size_t axis=0;axis<2;++axis) {
                        local[axis] += weights[j]*skinLocal[triangle[j]][axis];
                    }
                const auto vertex = skinWorld.size();
                if (vertex >= std::numeric_limits<std::uint16_t>::max()) return;
                skinWorld.append(world);
                skinLocal.append(local);
                samplingTriangles[i] = {triangle[0],triangle[1],vertex};
                samplingTriangles.push_back({triangle[1],triangle[2],vertex});
                samplingTriangles.push_back({triangle[2],triangle[0],vertex});
                break;
            }
        };
        if (read(read(skinTarget,"mapping"),"uv").sourceArrayValue().size() == originalCount) {
            for (const auto& outline : contourSamples)
                for (const auto& sample : outline)
                    if (sample.section[1]>profile.temple && sample.section[1]<chin)
                        splitSamplingTriangle(sample.section);
            for (std::size_t i=0; i<contourSamples.size(); ++i)
                for (const double y : {profile.temple,chin,jawRows[i].value_or(std::midpoint(profile.temple,chin))}) {
                    const auto& curveSide = i == 0 ? profile.left : profile.right;
                    splitSamplingTriangle({curve(profile.y,curveSide,y),y});
                }
            splitSamplingTriangle(chinTip);
        }
        Array samplingIndices;
        for (const auto& t : samplingTriangles)
            samplingIndices.append(Json(Array{Json(t[0]),Json(t[1]),Json(t[2])}));
        skinTriangles = Json(samplingIndices);
    }
    Slice<bool> pinned; pinned.resize(skinWorld.size(), false);
    struct AnchorConstraint {
        std::array<std::size_t, 3> vertices;
        Point3 weights;
    };
    Slice<AnchorConstraint> anchorConstraints;
    for (const auto triangle : skinTriangles.as_array()) {
        auto indices = contracts::ngRigNumbers(triangle);
        const auto a = skinLocal[sourceControlIndex(indices[0])], b = skinLocal[sourceControlIndex(indices[1])],
            c = skinLocal[sourceControlIndex(indices[2])];
        if (std::abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) < 1e-10) continue;
        const std::array vertices{sourceControlIndex(indices[0]), sourceControlIndex(indices[1]),
            sourceControlIndex(indices[2])};
        const auto first = profile.frame.local(skinWorld[vertices[0]]);
        const auto second = profile.frame.local(skinWorld[vertices[1]]);
        const auto third = profile.frame.local(skinWorld[vertices[2]]);
        const double lower = std::min({first[1],second[1],third[1]});
        const double upper = std::max({first[1],second[1],third[1]});
        auto row = std::lower_bound(profile.y.begin(),profile.y.end(),lower-1e-7);
        for (; row != profile.y.end() && *row <= upper+1e-7; ++row) {
            for (auto& edge : contourSamples) {
                auto& sample = edge[static_cast<std::size_t>(row-profile.y.begin())];
                if (sample.mapped) continue;
                const auto weights = controls_detail::barycentric(sample.section,first,second,third);
                if (!(weights[0]>=-1e-7 && weights[1]>=-1e-7 && weights[2]>=-1e-7)) continue;
                sample.vertices = vertices;
                sample.weights = weights;
                sample.mapped = true;
            }
        }
        for (const auto anchor : anchors) {
            const auto weights = controls_detail::barycentric(anchor, skinWorld[sourceControlIndex(indices[0])],
                skinWorld[sourceControlIndex(indices[1])], skinWorld[sourceControlIndex(indices[2])]);
            if (weights[0] >= -1e-7 && weights[1] >= -1e-7 && weights[2] >= -1e-7) {
                anchorConstraints.append(AnchorConstraint{{sourceControlIndex(indices[0]),
                    sourceControlIndex(indices[1]), sourceControlIndex(indices[2])}, weights});
                for (std::size_t j = 0; j < 3; ++j)
                    if (weights[j] > 1 - 1e-7) pinned[sourceControlIndex(indices[j])] = true;
            }
        }
    }
    for (std::size_t i = 0; i < skinWorld.size(); ++i) {
        const auto p = profile.frame.local(skinWorld[i]);
        const double middle = profile.midline(p[1]);
        const double radius = (curve(profile.y, profile.right, p[1]) -
            curve(profile.y, profile.left, p[1])) / 2.;
        if (p[1] <= profile.temple || std::abs(p[0] - middle) <= .22 * radius)
            pinned[i] = true;
    }
    // Use the painted neck boundary in the same root coordinates as the face.
    std::array<std::map<long, double>, 2> neckEdges;
    for (const auto& [id, material] : materials) {
        if (!targets.contains(id) || read(material,"role").as_string().view() != "neck") continue;
        const auto neckContour = observation::ngRigAlphaContour(material, context);
        for (const auto p : contracts::ngRigPoints(read(neckContour,"points"))) {
            const long row = std::lround(p[1]/step);
            for (std::size_t sideIndex=0; sideIndex<neckEdges.size(); ++sideIndex) {
                auto [it, inserted] = neckEdges[sideIndex].try_emplace(row,p[0]);
                if (!inserted) it->second = sideIndex == 0 ? std::min(it->second,p[0]) : std::max(it->second,p[0]);
            }
        }
    }
    struct EarRoot {
        Json target;
        Slice<Point2> attachment;
        double score = std::numeric_limits<double>::infinity();
        std::uint64_t id = 0;
        double step = 0.;
    };
    std::array<EarRoot, 2> earsBySide;
    const double faceHeight = chin-profile.temple;
    for (const auto& [id, material] : materials) {
        if (!targets.contains(id) || read(material,"role").as_string().view() != "ear") continue;
        const auto bounds = contracts::ngRigNumbers(read(material,"bounds"));
        const auto center = profile.frame.local({std::midpoint(bounds[0],bounds[2]),
                                                 std::midpoint(bounds[1],bounds[3])});
        const double height = bounds[3]-bounds[1];
        // An anatomical ear overlaps the eye-to-jaw band; ears above the
        // brow (including separate decorative inner layers) do not.
        if (center[1] < profile.eye-.12*faceHeight ||
            center[1] > profile.mouth+.10*faceHeight ||
            height < .05*faceHeight || height > .55*faceHeight) continue;
        const int side = center[0] < profile.midline(center[1]) ? -1 : 1;
        auto& selected = earsBySide[side < 0 ? 0 : 1];
        const double score = std::abs(center[1]-std::lerp(profile.eye,profile.mouth,.32));
        if (score >= selected.score) continue;
        const auto contour = observation::ngRigAlphaContour(material,context);
        const auto points = contracts::ngRigPoints(read(contour,"points"));
        Slice<Point2> attachment;
        for (const auto point : points) {
            if (point[1] < std::lerp(bounds[1],bounds[3],.56)) continue;
            if (side*(profile.frame.local(point)[0]-center[0]) > 0.) continue;
            attachment.append(point);
        }
        if (!attachment.empty()) selected = {targets[id],attachment,score,id,contracts::ngRigNumber(read(contour,"pixel_step"))};
    }
    struct HairTarget {
        Json target;
        Slice<Point2> vertices;
        std::array<bool,2> adjacent{};
        Slice<Point2> noseEdge;
        double noseClearance = 0.;
    };
    std::vector<HairTarget> hairTargets;
    std::vector<Json> skinOutlines;
    struct SkinFollower {
        std::uint64_t id;
        std::vector<ContourSample> samples;
    };
    std::vector<SkinFollower> coincidentSkin;
    const auto skinMaterial = readControlMap(materials, skin, 704);
    const double faceWidth = curve(profile.y,profile.right,profile.eye)-
        curve(profile.y,profile.left,profile.eye);
    const double hairBand = .20*(curve(profile.y,profile.right,profile.eye)-
        curve(profile.y,profile.left,profile.eye));
    std::optional<Point2> nose;
    double noseHalfHeight=0.;
    for (const auto& [id, material] : materials) {
        if (!targets.contains(id) || !material.sourceObjectRef().if_contains("feature") ||
            read(material,"feature").as_string().view()!="nose") continue;
        const auto bounds=contracts::ngRigNumbers(read(material,"bounds"));
        nose=profile.frame.local({std::midpoint(bounds[0],bounds[2]),std::midpoint(bounds[1],bounds[3])});
        noseHalfHeight=(bounds[3]-bounds[1])*.5;
        break;
    }
    for (const auto& [id, material] : materials) {
        const auto role = read(material,"role").as_string().view();
        if (!targets.contains(id) || id==skin) continue;
        const bool sameGrid = read(targets[id],"grid")==read(skinTarget,"grid");
        if (sameGrid && role == "face" &&
            read(material,"texture_size") == read(skinMaterial,"texture_size") &&
            read(material,"source_root_mapping") == read(skinMaterial,"source_root_mapping") &&
            read(material,"alpha_runs_32") == read(skinMaterial,"alpha_runs_32") &&
            read(material,"alpha_runs_128") == read(skinMaterial,"alpha_runs_128") &&
            read(targets[id],"origin") == read(skinTarget,"origin") &&
            read(targets[id],"root_to_local_direction") == read(skinTarget,"root_to_local_direction")) {
            SkinFollower follower{id, {}};
            for (const auto world : contracts::ngRigPoints(read(targets[id], "world"))) {
                ContourSample sample{world};
                for (const auto& triangle : skinTriangles.as_array()) {
                    const auto ids = contracts::ngRigNumbers(triangle);
                    const std::array vertices{sourceControlIndex(ids[0]), sourceControlIndex(ids[1]),
                        sourceControlIndex(ids[2])};
                    const auto weights = controls_detail::barycentric(world, skinWorld[vertices[0]],
                        skinWorld[vertices[1]], skinWorld[vertices[2]]);
                    if (!(weights[0] >= -1e-7 && weights[1] >= -1e-7 && weights[2] >= -1e-7)) continue;
                    sample.vertices = vertices;
                    sample.weights = weights;
                    sample.mapped = true;
                    break;
                }
                follower.samples.push_back(sample);
            }
            coincidentSkin.push_back(std::move(follower));
            continue;
        }
        if (sameGrid && role!="hair_front" && role!="hair_side") {
            const auto size = contracts::ngRigNumbers(read(material,"texture_size"));
            const auto runs = read(material,"alpha_runs_32").as_array();
            std::size_t painted = 0;
            for (std::size_t i=1;i<runs.size();i+=2) painted += contracts::ngRigUnsigned(runs[i]);
            if (painted < .15*size[0]*size[1]) {
                const auto points = contracts::ngRigPoints(read(
                    observation::ngRigAlphaContour(material,context),"points"));
                std::size_t matching = 0;
                double first = chin, last = profile.temple;
                for (const auto p : points) {
                    const auto q = profile.frame.local(p);
                    if (q[1]<profile.temple || q[1]>chin) continue;
                    const double distance = std::min(std::abs(q[0]-curve(profile.y,profile.left,q[1])),
                        std::abs(q[0]-curve(profile.y,profile.right,q[1])));
                    if (distance > .035*faceWidth) continue;
                    ++matching;
                    first = std::min(first,q[1]);
                    last = std::max(last,q[1]);
                }
                if (matching > .55*points.size() && last-first > .4*(chin-profile.temple))
                    skinOutlines.push_back(targets[id]);
            }
        }
        if (role!="hair_front" && role!="hair_side") continue;
        const auto target=readControlMap(targets,id,710);
        if (target.sourceObjectRef().if_contains("hair_shell_offset")) continue;
        const auto contour = contracts::ngRigPoints(read(observation::ngRigAlphaContour(material,context),"points"));
        HairTarget hair{target,contracts::ngRigPoints(read(target,"world"))};
        for (std::size_t i=0; i<hair.adjacent.size(); ++i) {
            const auto& edge = i == 0 ? profile.left : profile.right;
            const auto adjacent = [&](Point2 p) {
                const auto q = profile.frame.local(p);
                if (q[1]>=chin) return false;
                const double gap = (i == 0 ? -1. : 1.)*(q[0]-curve(profile.y,edge,q[1]));
                return gap >= 0. || std::abs(gap) < hairBand;
            };
            hair.adjacent[i] = std::ranges::any_of(contour,adjacent);
        }
        if (nose && role=="hair_front") {
            const double width=.09*faceWidth;
            double tip=profile.temple;
            for (const auto p : contour) {
                const auto q=profile.frame.local(p);
                if (std::abs(q[0]-(*nose)[0])<width) tip=std::max(tip,q[1]);
            }
            const double gap=(*nose)[1]-noseHalfHeight-tip;
            if (gap>0. && gap<.16*faceWidth) {
                hair.noseClearance=gap;
                for (const auto p : contour) {
                    const auto q=profile.frame.local(p);
                    if (std::abs(q[0]-(*nose)[0])<2.*width && q[1]>tip-.14*faceWidth)
                        hair.noseEdge.append(p);
                }
            }
        }
        if (hair.adjacent[0] || hair.adjacent[1] || !hair.noseEdge.empty())
            hairTargets.push_back(std::move(hair));
    }
    auto matrix = contracts::ngRigNumbers(read(skinTarget, "parent_to_root"));
    const double m00 = matrix[0];
    const double m11 = matrix[4];
    const double m01 = matrix[1];
    const double m10 = matrix[3];
    const double determinant = m00 * m11 - m01 * m10;
    auto xs = contracts::ngRigNumbers(read(skinTarget, "xs")), ys = contracts::ngRigNumbers(read(skinTarget, "ys"));
    struct YawRidge {
        int side;
        Point2 temple, chin, seam, upper, lower;
    };
    std::map<std::size_t,YawRidge> yawRidges;
    std::map<std::size_t,Point2> earSourcesByYaw;
    const auto fittedLandmarks=program.sourceObjectRef().if_contains("head_anatomical_landmarks");
    std::vector<Json> skinKeys;
    for (const auto& key : keys)
        if (read(key,"grid") == read(skinTarget,"grid")) skinKeys.push_back(key);
    const auto neutralPitch = [](const Json& key) {
        return std::abs(contracts::ngRigPoint(read(key,"value"))[1]) <= 1e-6;
    };
    std::stable_partition(skinKeys.begin(),skinKeys.end(),neutralPitch);
    for (const auto& key : skinKeys) {
        contracts::ngRigCheckpoint(context);
        const auto projection = contracts::ngRigNumbers(read(key, "projection"));
        enforce(projection.size() == 8, "Missing native XYZ projection", 728);
        const Point3 depthAxis{projection[2] * projection[5] - projection[4] * projection[3],
            projection[4] * projection[1] - projection[0] * projection[5], projection[0] * projection[3] - projection[2] * projection[1]};
        const double norm = std::sqrt(square(depthAxis[0]) + square(depthAxis[1]) + square(depthAxis[2]));
        enforce(norm > 1e-12, "Degenerate viewing-depth axis", 731);
        const double slope = (depthAxis[0] * profile.frame.tangent[0] + depthAxis[1] * profile.frame.tangent[1]) / norm;
        const auto value = contracts::ngRigPoint(read(key, "value"));
        const auto axisKey = contracts::ngRigNumbers(read(key, "key"));
        const std::array poseKey{sourceControlIndex(axisKey[0]), sourceControlIndex(axisKey[1])};
        const auto yaw = poseKey[0];
        const auto base = yawRidges.find(yaw);
        const bool pitch = !neutralPitch(key);
        const int side = base != yawRidges.end() ? base->second.side :
            std::abs(value[0]) <= 1e-6 || std::abs(slope) <= 1e-6 ? 0 : slope > 0 ? 1 : -1;
        const auto values = contracts::ngRigNumbers(read(key, "offsets"));
        Slice<Point2> offsets;
        for (std::size_t i = 0; i < values.size(); i += 2) offsets.append(Point2{values[i], values[i + 1]});
        const auto projected = [&](Point2 section) {
            return controls_detail::projected(section, profile, matrix, determinant, offsets, xs, ys);
        };
        const bool dynamic = contracts::ngRigGet(skinTarget,
            compat::RetainedString::fromStaticLiteral("grid_dynamic"), Json(false)).as_bool();
        const auto invertGrid = [&](Point2 world, Point2 wanted) {
            const auto original = projected(profile.frame.local(world));
            // A static Grid applies a translation sampled at the rest point.
            // Its inverse subtracts that translation from the desired position.
            if (!dynamic) return Point2{wanted[0]-original[0],wanted[1]-original[1]};
            auto candidate = world;
            for (unsigned iteration=0;iteration<20;++iteration) {
                const auto value=projected(profile.frame.local(candidate));
                const Point2 error{wanted[0]-value[0],wanted[1]-value[1]};
                if (std::hypot(error[0],error[1])<1e-5)
                    return Point2{candidate[0]-world[0],candidate[1]-world[1]};
                constexpr double h=.01;
                const auto dx=projected(profile.frame.local({candidate[0]+h,candidate[1]}));
                const auto dy=projected(profile.frame.local({candidate[0],candidate[1]+h}));
                const double a=(dx[0]-value[0])/h,b=(dy[0]-value[0])/h;
                const double c=(dx[1]-value[1])/h,d=(dy[1]-value[1])/h,det=a*d-b*c;
                enforce(std::isfinite(det)&&std::abs(det)>1e-8,"Cheek parent projection is singular",750);
                const Point2 step{(d*error[0]-b*error[1])/det,(a*error[1]-c*error[0])/det};
                double scale=1.;
                for (;scale>1./1024;scale*=.5) {
                    const auto next=projected(profile.frame.local({candidate[0]+step[0]*scale,candidate[1]+step[1]*scale}));
                    if (std::hypot(wanted[0]-next[0],wanted[1]-next[1])<std::hypot(error[0],error[1])) break;
                }
                candidate[0]+=step[0]*scale;candidate[1]+=step[1]*scale;
            }
            enforce(false,"Cheek residual inverse projection did not converge",750);
            return Point2{0.,0.};
        };
        const auto unprojectResidual = [&](Point2 world, Point2 residual) {
            if (residual[0] == 0. && residual[1] == 0.) return residual;
            const auto original = projected(profile.frame.local(world));
            return invertGrid(world,{original[0]+residual[0],original[1]+residual[1]});
        };
        Slice<bool> allowedSkin;
        allowedSkin.resize(skinWorld.size(),false);
        const double span = chin-profile.temple;
        const auto& edge = side < 0 ? profile.left : profile.right;
        const Point2 restTemple{curve(profile.y,edge,profile.temple),profile.temple};
        // Only the endpoints come from the inherited pose. The intervening
        // contour is built from the artwork, in its own temple-to-chin frame.
        const auto composedPoint = [&](Point2 section) {
            for (const auto triangle : skinTriangles.as_array()) {
                const auto ids = contracts::ngRigNumbers(triangle);
                const std::array v{sourceControlIndex(ids[0]),sourceControlIndex(ids[1]),sourceControlIndex(ids[2])};
                const auto w = controls_detail::barycentric(section,profile.frame.local(skinWorld[v[0]]),
                    profile.frame.local(skinWorld[v[1]]),profile.frame.local(skinWorld[v[2]]));
                if (!(w[0]>=-1e-7 && w[1]>=-1e-7 && w[2]>=-1e-7)) continue;
                Point2 result{0.,0.};
                for (std::size_t i=0; i<3; ++i) {
                    const auto p = projected(profile.frame.local(skinWorld[v[i]]));
                    result[0] += w[i]*p[0]; result[1] += w[i]*p[1];
                }
                return result;
            }
            return projected(section);
        };
        const auto posedTemple = composedPoint(restTemple);
        const Point2 restChin{curve(profile.y,edge,chin),chin};
        const auto posedChin = composedPoint(fittedMesh.empty() ? restChin : chinTip);
        const double poseDX = posedChin[0]-posedTemple[0], poseDY = posedChin[1]-posedTemple[1];
        const double poseLength = std::max(1e-8,std::hypot(poseDX,poseDY));
        const Point2 normal{poseDY/poseLength,-poseDX/poseLength};
        // Carry the neutral artwork's signed curvature into the two fixed
        // endpoints. Use its natural width and avoid adding a made-up bulge.
        const double restDX = restChin[0]-restTemple[0];
        const double restLength = std::hypot(restDX,span);
        const auto artworkTarget = [&](double y) {
            const double h = std::clamp((y-profile.temple)/span,0.,1.);
            const double x = curve(profile.y,edge,y);
            const double bulge = x-std::lerp(restTemple[0],restChin[0],h);
            const double relief = bulge*span/restLength*poseLength/restLength;
            return Point2{std::lerp(posedTemple[0],posedChin[0],h)+relief*normal[0],
                          std::lerp(posedTemple[1],posedChin[1],h)+relief*normal[1]};
        };
        // Blend through the adjoining painted spans so that the fixed
        // endpoints do not acquire a new tangent discontinuity.
        const auto provisionalContour = [&](Point2 section, Point2 current) {
            const double h = (section[1]-profile.temple)/span;
            const double join = smootherStep(h/.32)*smootherStep((1.-h)/.27);
            const auto wanted = artworkTarget(section[1]);
            return Point2{std::lerp(current[0],wanted[0],join),
                          std::lerp(current[1],wanted[1],join)};
        };
        const auto sideIndex = side < 0 ? 0 : 1;
        const auto& neckEdge = neckEdges[sideIndex];
        const auto edgeAt = [&](double y) { return curve(profile.y,edge,y); };
        const auto jawRow = jawRows[sideIndex];
        const auto& modelJaw = side<0 ? head_template::leftJaw : head_template::rightJaw;
        const auto& modelEar = side<0 ? head_template::leftEar : head_template::rightEar;
        const double templateJawRow=std::lerp(profile.temple,chin,(modelJaw[1]-.38)/(1.-.38));
        const double seamRow = jawRow.value_or(templateJawRow);
        const Point2 sourceM{edgeAt(seamRow),seamRow};
        const Point2 posedM = composedPoint(sourceM);
        std::optional<Point2> earContact;
        std::optional<Point2> neckContact;
        Json earEvidence;
        bool horizontalEar = false;
        bool horizontalNeck = false;
        double earDistance = INFINITY;
        double neckDistance = INFINITY;
        // Choose the nearest first hit along the horizontal line through M.
        // If the two painted silhouettes miss this row, allow a translation
        // to the closest available contact, with height change penalized.
        const auto consider = [&](Point2 p, bool horizontal, std::optional<Point2>& contact,
                                  bool& hasHorizontal, double& distance) {
            const double score = std::hypot(p[0]-posedM[0],4.*(p[1]-posedM[1]));
            if ((!horizontal && hasHorizontal) || (horizontal == hasHorizontal && score >= distance))
                return false;
            contact = p;
            hasHorizontal = horizontal;
            distance = score;
            return true;
        };
        if (!pitch && side && jawRow) {
            std::optional<Point2> previous;
            for (const auto& [row,x] : neckEdge) {
                const Point2 p{x,row*step};
                consider(p,p[1]==posedM[1],neckContact,horizontalNeck,neckDistance);
                if (previous && p[1]-(*previous)[1] <= 4.*step &&
                    (*previous)[1] <= posedM[1] && posedM[1] < p[1]) {
                    const auto t = (posedM[1]-(*previous)[1])/(p[1]-(*previous)[1]);
                    consider({std::lerp((*previous)[0],p[0],t),posedM[1]},true,
                        neckContact,horizontalNeck,neckDistance);
                }
                previous = p;
            }
        }
        const auto& ear = earsBySide[side < 0 ? 0 : 1];
        const auto lockedEar = earSourcesByYaw.find(yaw);
        if (side && ear.id && (!pitch || lockedEar != earSourcesByYaw.end())) {
            for (const auto earKey : keys) {
                if (read(earKey,"grid") != read(ear.target,"grid") || read(earKey,"key") != read(key,"key")) continue;
                const auto axesX = contracts::ngRigNumbers(read(ear.target,"xs"));
                const auto axesY = contracts::ngRigNumbers(read(ear.target,"ys"));
                const auto frame = contracts::ngRigNumbers(read(ear.target,"parent_to_root"));
                const double det = frame[0]*frame[4]-frame[1]*frame[3];
                const auto baked = contracts::ngRigNumbers(read(earKey,"offsets"));
                Slice<Point2> earOffsets;
                for (std::size_t i=0;i<baked.size();i+=2) earOffsets.append(Point2{baked[i],baked[i+1]});
                const auto earWorld = contracts::ngRigPoints(read(ear.target,"world"));
                Slice<Point2> posedEar;
                for (const auto p : earWorld) {
                    const auto offset = controls_detail::gridSample(earOffsets,axesX,axesY,
                        controls_detail::gridLocal(p,frame,det));
                    posedEar.append(Point2{p[0]+frame[0]*offset[0]+frame[1]*offset[1],
                                           p[1]+frame[3]*offset[0]+frame[4]*offset[1]});
                }
                const auto triangles = read(read(ear.target,"mapping"),"triangles").as_array();
                struct EarBoundaryPoint {
                    Point2 rest;
                    Point2 posed;
                };
                std::map<std::array<long,2>, EarBoundaryPoint> boundary;
                const double earStep = ear.step;
                const auto poseAttachment = [&](Point2 p) -> std::optional<Point2> {
                    for (const auto& triangle : triangles) {
                        const auto indices = contracts::ngRigNumbers(triangle);
                        const std::array vertex{sourceControlIndex(indices[0]),sourceControlIndex(indices[1]),
                                                sourceControlIndex(indices[2])};
                        const auto w = controls_detail::barycentric(p,earWorld[vertex[0]],
                            earWorld[vertex[1]],earWorld[vertex[2]]);
                        if (!(w[0]>=-1e-7 && w[1]>=-1e-7 && w[2]>=-1e-7)) continue;
                        Point2 attachment{0.,0.};
                        for (std::size_t j=0;j<3;++j)
                            for (std::size_t axis=0;axis<2;++axis)
                                attachment[axis] += w[j]*posedEar[vertex[j]][axis];
                        return attachment;
                    }
                    return {};
                };
                if (pitch) {
                    earContact = poseAttachment(lockedEar->second);
                    if (earContact) earEvidence = record({{"source",point(lockedEar->second)}});
                    break;
                }
                for (const auto p : ear.attachment) {
                    const auto attachment = poseAttachment(p);
                    if (!attachment) continue;
                    const auto restCell = [&](std::size_t axis) {
                        return std::lround((p[axis]-ear.attachment[0][axis])/earStep);
                    };
                    boundary.try_emplace({restCell(0),restCell(1)},EarBoundaryPoint{p,*attachment});
                    if (consider(*attachment,(*attachment)[1]==posedM[1],earContact,horizontalEar,earDistance))
                        earEvidence = record({{"source",point(p)}});
                }
                for (const auto& [cell, a] : boundary) {
                    for (const auto& [dx,dy] : std::array<std::array<int,2>,4>{{{1,0},{0,1},{1,1},{-1,1}}}) {
                        const auto next = boundary.find({cell[0]+dx,cell[1]+dy});
                        if (next == boundary.end()) continue;
                        const auto& b = next->second;
                        if ((a.posed[1] > posedM[1]) == (b.posed[1] > posedM[1])) continue;
                        const double t = (posedM[1]-a.posed[1])/(b.posed[1]-a.posed[1]);
                        const Point2 hit{std::lerp(a.posed[0],b.posed[0],t),posedM[1]};
                        if (consider(hit,true,earContact,horizontalEar,earDistance))
                            earEvidence = record({{"source",point({std::lerp(a.rest[0],b.rest[0],t),
                                std::lerp(a.rest[1],b.rest[1],t)})}});
                    }
                }
                break;
            }
        }
        if (earContact && !pitch)
            earSourcesByYaw.emplace(yaw,contracts::ngRigPoint(read(earEvidence,"source")));
        enforce(pitch || !side || !jawRow || earContact || neckContact,
            "Detected jaw angle has no ear or neck contact",751);
        std::optional<Point2> seam;
        Json neckJunction;
        if (!pitch && side) {
            const bool useEar = earContact && (!neckContact ||
                (horizontalEar != horizontalNeck ? horizontalEar : earDistance <= neckDistance));
            seam = useEar ? earContact : neckContact;
            if (!seam) seam = provisionalContour(sourceM,posedM);
            neckJunction = record({{"section_y",Json(seamRow)},{"target",point(*seam)},
                {"source_m",point(toRoot(sourceM))},{"posed_m",point(posedM)},
                {"source_kind",Json(jawRow ? "jaw_angle" : "midpoint")},
                {"contact_kind",Json(useEar ? "ear" : neckContact ? "neck" : "midpoint")},
                {"horizontal_contact",Json(useEar ? horizontalEar : horizontalNeck)}});
            if (earContact) {
                neckJunction.sourceIndexAssignSlot("ear_attachment") = point(*earContact);
                neckJunction.sourceIndexAssignSlot("ear") = Json(ear.id);
                neckJunction.sourceIndexAssignSlot("ear_contact_source") = read(earEvidence,"source");
            }
            if (neckContact) neckJunction.sourceIndexAssignSlot("neck_edge") = point(*neckContact);
        }
        // The two quadratic arcs share a tangent at the ear/neck junction.
        // Its direction lies between the two chords, so both arcs turn in the
        // same direction. Limit handles to keep progress along each chord.
        Point2 upperHandle{}, lowerHandle{};
        if (pitch && base != yawRidges.end()) {
            const auto& reference = base->second;
            const double x = reference.chin[0]-reference.temple[0];
            const double y = reference.chin[1]-reference.temple[1];
            const double lengthSquared = x*x+y*y;
            // The endpoint chord determines the rotation and the uniform scale.
            const double a = (poseDX*x+poseDY*y)/lengthSquared;
            const double b = (poseDY*x-poseDX*y)/lengthSquared;
            const auto rotate = [&](Point2 point) {
                const double dx = point[0]-reference.temple[0];
                const double dy = point[1]-reference.temple[1];
                return Point2{posedTemple[0]+a*dx-b*dy,posedTemple[1]+b*dx+a*dy};
            };
            seam = rotate(reference.seam);
            upperHandle = rotate(reference.upper);
            lowerHandle = rotate(reference.lower);
            neckJunction = record({{"section_y",Json(seamRow)},{"target",point(*seam)},
                {"source_m",point(toRoot(sourceM))},{"posed_m",point(posedM)},
                {"source_kind",Json(jawRow ? "jaw_angle" : "midpoint")},
                {"contact_kind",Json("yaw_ridge")}});
        } else if (seam) {
            const Point2 upper{(*seam)[0]-posedTemple[0],(*seam)[1]-posedTemple[1]};
            const Point2 lower{posedChin[0]-(*seam)[0],posedChin[1]-(*seam)[1]};
            const double upperLength = std::hypot(upper[0],upper[1]);
            const double lowerLength = std::hypot(lower[0],lower[1]);
            const Point2 bisector{upper[0]/std::max(upperLength,1e-8)+lower[0]/std::max(lowerLength,1e-8),
                                  upper[1]/std::max(upperLength,1e-8)+lower[1]/std::max(lowerLength,1e-8)};
            const double norm = std::hypot(bisector[0],bisector[1]);
            if (upperLength < 1e-8 || lowerLength < 1e-8 || norm < 1e-6) {
                seam.reset();
            } else {
                const double upperSpan = seamRow-profile.temple;
                const double lowerSpan = chin-seamRow;
                const double speed = .48*std::min(upperLength/upperSpan,lowerLength/lowerSpan)/norm;
                for (std::size_t axis=0;axis<2;++axis) {
                    upperHandle[axis] = (*seam)[axis]-speed*upperSpan*bisector[axis];
                    lowerHandle[axis] = (*seam)[axis]+speed*lowerSpan*bisector[axis];
                }
                yawRidges.emplace(yaw,YawRidge{side,posedTemple,posedChin,*seam,upperHandle,lowerHandle});
            }
        }
        const auto quadratic = [](Point2 start, Point2 handle, Point2 end, double t) {
            const double u = 1.-t;
            return Point2{u*u*start[0]+2.*u*t*handle[0]+t*t*end[0],
                          u*u*start[1]+2.*u*t*handle[1]+t*t*end[1]};
        };
        // Project the anatomical joints on the fitted template, then join
        // the four points in order without introducing intermediate extrema.
        const std::span<const double> poseProjection{projection.begin(),projection.size()};
        std::vector<Point2> rawHead;
        if (side) for (const auto p : fittedMesh) rawHead.push_back(head_surface::project(p,poseProjection));
        const head_surface::ProjectedSkin redFace(rawHead,side);
        const auto registeredRidge=side && !fittedMesh.empty() ? head_surface::projectedRidge(
            fittedMesh,fittedJoints[sideIndex],
            lowerSections[sideIndex],rawHead,poseProjection,side,posedTemple,posedChin,2.*step,earContact) :
            std::optional<head_surface::Ridge>{};
        const bool useHeadTemplate=side && (fittedLandmarks || registeredRidge);
        const double legacyEarRow=std::lerp(profile.temple,seamRow,
            std::clamp((modelEar[1]-.38)/(modelJaw[1]-.38),.22,.81));
        const auto fittedEar=fittedJoints[sideIndex][1];
        const auto earSource = earContact ? contracts::ngRigPoint(read(earEvidence,"source")) :
            Point2{fittedEar[0],fittedEar[1]};
        const double templateEarRow=registeredRidge || earContact ? std::clamp(
            profile.frame.local(earSource)[1],profile.temple+.10*span,
            std::max(profile.temple+.10*span,seamRow-step)) : legacyEarRow;
        const std::array<double,4> sourceRows{profile.temple,templateEarRow,seamRow,chin};
        std::array<Point2,4> anchors{};
        std::array<double,4> tangent{}, axialTangent{};
        if (registeredRidge) {
            anchors=registeredRidge->anchors;
            neckJunction=record({{"contact_kind",Json("standard_surface_fit")},
                {"upper",point(anchors[0])},{"ear_attachment",point(anchors[1])},
                {"jaw_angle",point(anchors[2])},{"original_jaw",point(registeredRidge->originalJaw)},
                {"chin",point(anchors[3])}});
            if (earContact) neckJunction.sourceIndexAssignSlot("ear_contact_source") = point(earSource);
            Array guide;
            for (std::size_t segment=0; segment<3; ++segment) {
                Array samples;
                for (int i=0; i<=32; ++i) samples.append(point(registeredRidge->at(segment,i/32.)));
                guide.append(Json(samples));
            }
            neckJunction.sourceIndexAssignSlot("guide") = Json(guide);
            if (!registeredRidge->lower.empty())
                neckJunction.sourceIndexAssignSlot("lower_depth_end")=point(registeredRidge->depthEnd);
        } else if (useHeadTemplate) {
            const auto project3D = [&](Point3 p) {
                return Point2{projection[0]*p[0]+projection[2]*p[1]+projection[4]*p[2]+projection[6],
                    projection[1]*p[0]+projection[3]*p[1]+projection[5]*p[2]+projection[7]};
            };
            const auto fittedPoint = [&](const char* key) {
                const auto p=contracts::ngRigNumbers(read(*fittedLandmarks,key));
                return project3D({p[0],p[1],p[2]});
            };
            anchors={posedTemple,fittedPoint(side<0 ? "left_ear" : "right_ear"),
                fittedPoint(side<0 ? "left_jaw" : "right_jaw"),posedChin};
            if (earContact) anchors[1] = *earContact;
            // Artwork contacts can be above or below their source rows.
            // Parameterize those contacts along the posed temple-chin axis.
            const Point2 axial{poseDX/poseLength,poseDY/poseLength};
            std::array<double,4> along{}, across{};
            for (std::size_t i=0;i<anchors.size();++i) {
                const double x=anchors[i][0]-posedTemple[0], y=anchors[i][1]-posedTemple[1];
                along[i]=x*axial[0]+y*axial[1];
                across[i]=x*normal[0]+y*normal[1];
            }
            const auto slopes = [&](const auto& values, auto& derivatives) {
                const auto secant = [&](std::size_t i) {
                    return (values[i+1]-values[i])/(sourceRows[i+1]-sourceRows[i]);
                };
                derivatives[0]=secant(0);
                derivatives[3]=secant(2);
                for (std::size_t i=1;i<3;++i) {
                    const double a=secant(i-1), b=secant(i);
                    derivatives[i]=a*b>0. ? 2.*a*b/(a+b) : 0.;
                }
            };
            slopes(across,tangent);
            slopes(along,axialTangent);

            // The skin ends at the painted temple. Use the fitted crown as
            // the virtual continuation above it to set the upper tangent.
            const auto crown=fittedPoint("crown");
            const Point2 towardTemple{posedTemple[0]-crown[0],posedTemple[1]-crown[1]};
            const double crownAdvance=towardTemple[0]*axial[0]+towardTemple[1]*axial[1];
            if (crownAdvance>step) {
                const double sectionWidth=sourceRows[1]-sourceRows[0];
                const double secant=across[1]/sectionWidth;
                const double transverse=towardTemple[0]*normal[0]+towardTemple[1]*normal[1];
                const double slope=transverse/crownAdvance*axialTangent[0];
                tangent[0]=std::clamp(slope,std::min(0.,2.*secant),std::max(0.,2.*secant));
            }
            neckJunction=record({{"contact_kind",Json(fittedMesh.empty() ? "anatomical_ridge" :
                "standard_surface_fallback")},{"crown",point(crown)},
                {"temple",point(anchors[0])},{"ear_attachment",point(anchors[1])},
                {"jaw_angle",point(anchors[2])},{"chin",point(anchors[3])}});
        }
        const auto fittedContour = [&](double y) {
            std::size_t segment=0;
            while (segment<2 && y>sourceRows[segment+1]) ++segment;
            const double width=sourceRows[segment+1]-sourceRows[segment];
            const double t=std::clamp((y-sourceRows[segment])/width,0.,1.), u=1.-t;
            if (registeredRidge) return registeredRidge->at(segment,t);
            const Point2 axial{poseDX/poseLength,poseDY/poseLength};
            const auto component = [&](std::size_t i, Point2 axis) {
                return (anchors[i][0]-posedTemple[0])*axis[0]+(anchors[i][1]-posedTemple[1])*axis[1];
            };
            const double along=std::lerp(component(segment,axial),component(segment+1,axial),t*t*(3.-2.*t))+
                width*t*u*(u*axialTangent[segment]-t*axialTangent[segment+1]);
            const double across=std::lerp(component(segment,normal),component(segment+1,normal),t*t*(3.-2.*t))+
                width*t*u*(u*tangent[segment]-t*tangent[segment+1]);
            return Point2{posedTemple[0]+along*axial[0]+across*normal[0],
                posedTemple[1]+along*axial[1]+across*normal[1]};
        };
        const auto targetContour = [&](Point2 section, Point2 current) {
            if (useHeadTemplate) return fittedContour(section[1]);
            if (!seam) return provisionalContour(section,current);
            if (section[1] <= seamRow)
                return quadratic(posedTemple,upperHandle,*seam,
                    std::clamp((section[1]-profile.temple)/(seamRow-profile.temple),0.,1.));
            return quadratic(*seam,lowerHandle,posedChin,
                std::clamp((section[1]-seamRow)/(chin-seamRow),0.,1.));
        };
        const auto anchoredEar = earContact && useHeadTemplate ? earContact :
            registeredRidge ? std::optional{registeredRidge->anchors[1]} : std::nullopt;
        if (seam && !useHeadTemplate) {
            neckJunction.sourceIndexAssignSlot("temple") = point(posedTemple);
            neckJunction.sourceIndexAssignSlot("upper_handle") = point(upperHandle);
            neckJunction.sourceIndexAssignSlot("lower_handle") = point(lowerHandle);
            neckJunction.sourceIndexAssignSlot("chin") = point(posedChin);
        }
        auto outline = contourSamples[side < 0 ? 0 : 1];
        auto edgeRows = profile.y.duplicate();
        const auto insertContourRow = [&](double rowY) {
            const Point2 section{edgeAt(rowY),rowY};
            ContourSample sample{section};
            for (const auto& triangle : skinTriangles.as_array()) {
                const auto indices = contracts::ngRigNumbers(triangle);
                for (std::size_t j=0;j<3;++j) sample.vertices[j] = sourceControlIndex(indices[j]);
                sample.weights = controls_detail::barycentric(section,
                    profile.frame.local(skinWorld[sample.vertices[0]]),
                    profile.frame.local(skinWorld[sample.vertices[1]]),
                    profile.frame.local(skinWorld[sample.vertices[2]]));
                if (sample.weights[0]>=-1e-7 && sample.weights[1]>=-1e-7 && sample.weights[2]>=-1e-7) {
                    sample.mapped = true;
                    break;
                }
            }
            const auto row = std::ranges::lower_bound(outline,rowY,{},[](const auto& sample) {
                return sample.section[1];
            });
            if (row != outline.end() && row->section[1] == rowY) *row = sample;
            else outline.insert(row,sample);
        };
        if (seam || useHeadTemplate) insertContourRow(seamRow);
        if (useHeadTemplate) insertContourRow(templateEarRow);
        if (seam || useHeadTemplate) {
            edgeRows = {};
            for (const auto& sample : outline) edgeRows.append(sample.section[1]);
        }
        Slice<Point2> edgeResidual;
        Array ridgeSamples;
        if (side) {
            const double strength = useHeadTemplate ? smootherStep(std::abs(value[0])/.5) :
                seam ? 1. : smootherStep(std::abs(slope)/.34);
            // Match the lower ridge blend at E without a step in the upper ridge.
            const double earTransition = .2*(templateEarRow-profile.temple);
            const auto inheritedPoint = [&](const ContourSample& sample) {
                if (!sample.mapped) return projected(sample.section);
                Point2 current{0.,0.};
                for (std::size_t i=0; i<3; ++i) {
                    const auto q = projected(profile.frame.local(skinWorld[sample.vertices[i]]));
                    for (std::size_t axis=0; axis<2; ++axis) current[axis] += sample.weights[i]*q[axis];
                }
                return current;
            };
            std::vector<Point2> inherited;
            inherited.reserve(outline.size());
            for (const auto& sample : outline) inherited.push_back(inheritedPoint(sample));
            const bool fixedEar = anchoredEar.has_value();
            Point2 earShift{0.,0.};
            if (fixedEar) {
                const auto at = std::ranges::find(outline,templateEarRow,
                    [](const auto& sample) { return sample.section[1]; });
                const auto current = inherited[static_cast<std::size_t>(at-outline.begin())];
                earShift = {(*anchoredEar)[0]-current[0],(*anchoredEar)[1]-current[1]};
            }
            for (std::size_t i=0; i<outline.size(); ++i) {
                const auto& sample = outline[i];
                const double y = sample.section[1];
                if (y <= profile.temple || y >= chin) {
                    edgeResidual.append(Point2{0.,0.});
                    continue;
                }
                const auto current = inherited[i];
                auto wanted = targetContour(sample.section,
                    useHeadTemplate ? projected(sample.section) : current);
                if (fixedEar && y==templateEarRow) wanted = *anchoredEar;
                if (fixedEar && y>templateEarRow) {
                    // Both ends of the blend share E. Ease the attachment
                    // displacement out along the original outline toward J.
                    const double weight = smootherStep((chin-y)/(chin-templateEarRow));
                    for (std::size_t axis=0; axis<2; ++axis)
                        wanted[axis] = std::lerp(current[axis]+weight*earShift[axis],
                            wanted[axis],jawRidgeBlend);
                }
                Point2 residual{strength*(wanted[0]-current[0]),strength*(wanted[1]-current[1])};
                if (!useHeadTemplate) {
                    // Samples run from the temple toward the chin. Rotating
                    // the posed tangent toward the visible side gives the
                    // outward normal, including on the inclined jaw.
                    const auto a = inherited[i>2 ? i-2 : 0];
                    const auto b = inherited[std::min(i+2,inherited.size()-1)];
                    const double dx = b[0]-a[0], dy = b[1]-a[1];
                    const double length = std::hypot(dx,dy);
                    const double outward = length > 1e-8 ?
                        side*(dy*residual[0]-dx*residual[1])/length : 0.;
                    // Ease into the correction to avoid a jump in its
                    // tangential component where the normal component is zero.
                    const double weight = smootherStep(outward/step);
                    for (auto& delta : residual) delta *= weight;
                }
                if (!redFace.edges.empty()) {
                    const double safe=redFace.clearance({current[0]+residual[0],current[1]+residual[1]});
                    const double weight=smootherStep(safe/(3.*step));
                    for (auto& delta : residual) delta *= weight;
                    if (redFace.clearance({current[0]+residual[0],current[1]+residual[1]})<.01)
                        residual={0.,0.};
                }
                // Samples on the inherited outline and the fitted ridge share
                // the source rows of J, M and E even after projection.
                const double blend = fixedEar ? 1. : std::lerp(jawRidgeBlend, 1.,
                    smootherStep((templateEarRow-y)/earTransition));
                for (auto& delta : residual) delta *= blend;
                const auto displaced = [&] { return Point2{current[0]+residual[0],current[1]+residual[1]}; };
                if (blend != 1. && !redFace.edges.empty() &&
                    redFace.clearance(displaced()) < .01)
                    residual = {0.,0.};
                if (fixedEar && y==templateEarRow && redFace.clearance(*anchoredEar) >= .01)
                    residual = earShift;
                edgeResidual.append(residual);
            }
            if (useHeadTemplate) {
                // The projected 3D section and the painted outline have
                // different sample spacing. Fair their join in pose space
                // before imposing any constraints on the Part mesh.
                const double band=.05*span;
                std::vector<Point2> guide;
                for (std::size_t i=0;i<outline.size();++i)
                    guide.push_back({inherited[i][0]+edgeResidual[i][0],inherited[i][1]+edgeResidual[i][1]});
                const auto smooth = [&](double y) {
                    Point2 sum{0.,0.};
                    double total=0.;
                    const auto first=std::ranges::lower_bound(outline,y-band,{},[](const auto& sample) {
                        return sample.section[1];
                    });
                    for (auto it=first; it!=outline.end() && it->section[1]<y+band; ++it) {
                        const auto index=static_cast<std::size_t>(it-outline.begin());
                        const double weight=smootherStep(1.-std::abs(it->section[1]-y)/band);
                        for (std::size_t axis=0;axis<2;++axis) sum[axis]+=weight*guide[index][axis];
                        total+=weight;
                    }
                    return Point2{sum[0]/total,sum[1]/total};
                };
                Point2 earAdjustment{0.,0.};
                if (fixedEar && redFace.clearance(*anchoredEar)>=.01) {
                    const auto fairEar=smooth(templateEarRow);
                    for (std::size_t axis=0;axis<2;++axis)
                        earAdjustment[axis]=(*anchoredEar)[axis]-fairEar[axis];
                }
                for (std::size_t i=0;i<outline.size();++i) {
                    const double y=outline[i].section[1];
                    if (y<=profile.temple || y>=chin) continue;
                    auto target=smooth(y);
                    const double pinWeight=smootherStep(1.-std::abs(y-templateEarRow)/(2.*band));
                    for (std::size_t axis=0;axis<2;++axis) target[axis]+=pinWeight*earAdjustment[axis];
                    const double taper=smootherStep(std::min(y-profile.temple,chin-y)/band);
                    for (std::size_t axis=0;axis<2;++axis)
                        target[axis]=std::lerp(inherited[i][axis],target[axis],taper);
                    if (!redFace.edges.empty()) {
                        const double clearance=std::min(0.,redFace.clearance(inherited[i]));
                        target[0]+=side*std::max(0.,clearance-redFace.clearance(target));
                    }
                    if (fixedEar && y==templateEarRow && redFace.clearance(*anchoredEar)>=.01)
                        target=*anchoredEar;
                    edgeResidual[i]={target[0]-inherited[i][0],target[1]-inherited[i][1]};
                }
            }
            for (std::size_t i=0;i<outline.size();++i) {
                const auto& sample=outline[i];
                const auto y=sample.section[1];
                if (!sample.mapped || y<=profile.temple || y>=chin) continue;
                const auto current=inherited[i], residual=edgeResidual[i];
                const Point2 target{current[0]+residual[0],current[1]+residual[1]};
                ridgeSamples.append(record({{"target",point(target)},{"before",point(current)},
                    {"section_y",Json(y)},{"ear_junction",Json(useHeadTemplate ? y==templateEarRow : seam && y==seamRow)},
                    {"weights",array(Slice<double>{sample.weights[0],sample.weights[1],sample.weights[2]})},
                    {"vertices",array(Slice<std::size_t>{sample.vertices[0],sample.vertices[1],sample.vertices[2]})}}));
            }
        }
        Slice<double> residualX, residualY;
        for (const auto delta : edgeResidual) { residualX.append(delta[0]); residualY.append(delta[1]); }
        const auto contourRepair = [&](Point2 world) {
            const auto p = profile.frame.local(world);
            if (!side || p[1] <= profile.temple || p[1] >= chin) return Point2{0.,0.};
            const double mid = profile.midline(p[1]);
            const double radius = side*(curve(profile.y,edge,p[1])-mid);
            if (radius <= 1e-8) return Point2{0.,0.};
            const double lateral = smootherStep((side*(p[0]-mid)/radius-.5)/.5);
            return Point2{lateral*curve(edgeRows,residualX,p[1]),lateral*curve(edgeRows,residualY,p[1])};
        };
        // Outline triangles can have a vertex outside the painted bounds.
        // Include those vertices in the fit; exact interpolated anchors still
        // protect the chin tip and feature positions.
        if (side) for (const auto& sample : outline) {
            if (!sample.mapped || sample.section[1] <= profile.temple || sample.section[1] >= chin) continue;
            for (const auto vertex : sample.vertices) {
                const auto section = profile.frame.local(skinWorld[vertex]);
                const double middle = profile.midline(section[1]);
                if (!pinned[vertex] && side*(section[0]-middle) >
                    .5*side*(curve(profile.y,edge,section[1])-middle))
                    allowedSkin[vertex] = true;
            }
        }
        Slice<Point2> skinField;
        for (std::size_t i = 0; i < skinWorld.size(); ++i) {
            const auto world = skinWorld[i];
            auto result = !pinned[i] ? contourRepair(world) : Point2{0.,0.};
            if (result[0] != 0. || result[1] != 0.) allowedSkin[i] = true;
            result = unprojectResidual(world,result);
            const auto skinInverse = contracts::ngRigNumbers(read(skinTarget, "root_to_local_direction"));
            const Point2 rounded{std::rint((skinInverse[0] * result[0] + skinInverse[1] * result[1]) * 1e5) / 1e5,
                std::rint((skinInverse[2] * result[0] + skinInverse[3] * result[1]) * 1e5) / 1e5};
            const double skinDeterminant = skinInverse[0] * skinInverse[3] - skinInverse[1] * skinInverse[2];
            skinField.append(Point2{(skinInverse[3] * rounded[0] - skinInverse[1] * rounded[1]) / skinDeterminant,
                (skinInverse[0] * rounded[1] - skinInverse[2] * rounded[0]) / skinDeterminant});
        }
        // Preserve the interpolated feature point, not every corner of its
        // containing triangle. Coarse real meshes otherwise lose most of their
        // cheek boundary simply because a mouth anchor shares that triangle.
        Slice<Slice<double>> anchorBasis;
        for (const auto constraint : anchorConstraints) {
            Slice<double> row; row.resize(skinWorld.size(), 0.);
            for (std::size_t j = 0; j < 3; ++j) {
                const auto vertex = constraint.vertices[j];
                if (allowedSkin[vertex] && !pinned[vertex]) row[vertex] = constraint.weights[j];
            }
            // Reorthogonalization handles coincident and nearly coincident
            // feature anchors without an ill-conditioned iterative projection.
            for (unsigned pass = 0; pass < 2; ++pass) for (const auto basis : anchorBasis) {
                double dot = 0;
                for (std::size_t i = 0; i < row.size(); ++i) dot += row[i] * basis[i];
                for (std::size_t i = 0; i < row.size(); ++i) row[i] -= dot * basis[i];
            }
            double norm = 0;
            for (const double value : row) norm += value * value;
            norm = std::sqrt(norm);
            if (norm < 1e-10) continue;
            for (auto& value : row) value /= norm;
            anchorBasis.append(row);
        }
        const auto preserveAnchors = [&](Slice<Point2> field) {
            for (const auto& basis : anchorBasis) {
                Point2 error{0., 0.};
                for (std::size_t i = 0; i < field.size(); ++i)
                    for (std::size_t axis = 0; axis < 2; ++axis) error[axis] += basis[i] * field[i][axis];
                for (std::size_t i = 0; i < field.size(); ++i)
                    for (std::size_t axis = 0; axis < 2; ++axis) field[i][axis] -= basis[i] * error[axis];
            }
        };
        const ContourSample* junctionSample = nullptr;
        Slice<double> junctionDirection;
        Point2 junctionDelta{0.,0.};
        const bool fixedEar = anchoredEar.has_value();
        if (!dynamic && (fixedEar || (seam && !registeredRidge))) {
            const auto at = std::ranges::find_if(outline,[&](const auto& sample) {
                return sample.mapped && sample.section[1] == (fixedEar ? templateEarRow : seamRow);
            });
            if (at != outline.end()) {
                junctionSample = &*at;
                junctionDirection.resize(skinWorld.size(), 0.);
                for (std::size_t j=0;j<3;++j) {
                    const auto vertex = at->vertices[j];
                    if (allowedSkin[vertex] && !pinned[vertex])
                        junctionDirection[vertex] = at->weights[j];
                }
                for (const auto& basis : anchorBasis) {
                    double component = 0.;
                    for (std::size_t i=0;i<junctionDirection.size();++i)
                        component += junctionDirection[i]*basis[i];
                    for (std::size_t i=0;i<junctionDirection.size();++i)
                        junctionDirection[i] -= component*basis[i];
                }
                double norm = 0.;
                for (const auto value : junctionDirection) norm += value*value;
                if (norm > 1e-10) {
                    for (auto& value : junctionDirection) value /= norm;
                    junctionDelta = edgeResidual[static_cast<std::size_t>(at-outline.begin())];
                } else {
                    junctionDirection = {};
                }
            }
        }
        const auto constrainJunction = [&](Slice<Point2> field, Point2 wanted) {
            if (junctionDirection.empty()) return;
            for (std::size_t j=0;j<3;++j)
                for (std::size_t axis=0;axis<2;++axis)
                    wanted[axis] -= junctionSample->weights[j]*field[junctionSample->vertices[j]][axis];
            for (std::size_t i=0;i<field.size();++i)
                for (std::size_t axis=0;axis<2;++axis)
                    field[i][axis] += junctionDirection[i]*wanted[axis];
        };
        preserveAnchors(skinField);
        constrainJunction(skinField,junctionDelta);
        // Fit the painted contour inside the triangles. A mesh vertex can
        // sit well outside the painted outline, so assigning the offset at
        // the vertex's own height alone misses the recess between vertices.
        if (side) {
            struct ContourConstraint {
                const ContourSample* sample;
                Point2 displacement;
            };
            std::vector<ContourConstraint> constraints;
            const auto appendConstraint = [&](const ContourSample& sample, Point2 residual) {
                const auto gridPoint = projected(sample.section);
                constraints.push_back({&sample,invertGrid(toRoot(sample.section),
                    {gridPoint[0]+residual[0],gridPoint[1]+residual[1]})});
            };
            for (std::size_t i=0; i<outline.size(); ++i) {
                const auto& sample = outline[i];
                if (!sample.mapped || sample.section[1] <= profile.temple || sample.section[1] >= chin) continue;
                // The triangle sample is already in rendered coordinates.
                // Translate its residual to the Grid sampling point before inversion.
                appendConstraint(sample, edgeResidual[i]);
            }
            const auto initial = skinField.duplicate();
            const auto rhs = [&](Slice<Point2> field, bool target) {
                Slice<Point2> result;
                result.resize(field.size(),Point2{0.,0.});
                for (const auto& constraint : constraints) {
                    const auto& sample = *constraint.sample;
                    Point2 residual = target ? constraint.displacement : Point2{0.,0.};
                    for (std::size_t j=0;j<3;++j)
                        for (std::size_t axis=0;axis<2;++axis)
                            residual[axis] -= sample.weights[j]*field[sample.vertices[j]][axis];
                    for (std::size_t j=0;j<3;++j) {
                        const auto vertex = sample.vertices[j];
                        if (!allowedSkin[vertex] || pinned[vertex]) continue;
                        for (std::size_t axis=0;axis<2;++axis)
                            result[vertex][axis] += sample.weights[j]*residual[axis];
                    }
                }
                constexpr double regularization = .12;
                for (std::size_t i=0;i<field.size();++i)
                    for (std::size_t axis=0;axis<2;++axis)
                        result[i][axis] -= regularization*(field[i][axis]-(target ? initial[i][axis] : 0.));
                preserveAnchors(result);
                constrainJunction(result,{0.,0.});
                return result;
            };
            const auto dot = [](Slice<Point2> a, Slice<Point2> b) {
                double value = 0.;
                for (std::size_t i=0;i<a.size();++i)
                    value += a[i][0]*b[i][0]+a[i][1]*b[i][1];
                return value;
            };
            auto residual = rhs(skinField,true);
            auto direction = residual.duplicate();
            double norm = dot(residual,residual);
            for (unsigned pass=0;pass<80 && norm>1e-12;++pass) {
                auto negativeProduct = rhs(direction,false);
                const double denominator = -dot(direction,negativeProduct);
                if (denominator <= 1e-12) break;
                const double gain = norm/denominator;
                for (std::size_t i=0;i<skinField.size();++i)
                    for (std::size_t axis=0;axis<2;++axis) {
                        skinField[i][axis] += gain*direction[i][axis];
                        residual[i][axis] += gain*negativeProduct[i][axis];
                    }
                const double nextNorm = dot(residual,residual);
                for (std::size_t i=0;i<direction.size();++i)
                    for (std::size_t axis=0;axis<2;++axis)
                        direction[i][axis] = residual[i][axis]+nextNorm/norm*direction[i][axis];
                norm = nextNorm;
            }

        }
        if (measureReference && side) {
            for (std::size_t i=0;i<outline.size();++i) {
                const auto& sample = outline[i];
                if (!sample.mapped || sample.section[1]<=profile.temple || sample.section[1]>=chin) continue;
                for (std::size_t j=0;j<3;++j) {
                    const auto vertex = sample.vertices[j];
                    if (vertex < originalCount || sample.weights[j] < 1.-1e-7 ||
                        !allowedSkin[vertex] || pinned[vertex]) continue;
                    skinField[vertex] = unprojectResidual(skinWorld[vertex],edgeResidual[i]);
                }
            }
            constrainJunction(skinField,junctionDelta);
        }
        // Inversion and barycentric fitting can pull a contour sample back
        // through the original projected skin. Remove the contributing
        // residuals before saving, repeating until no changed sample enters.
        if (measureReference && !redFace.edges.empty()) {
            std::vector<bool> remove(skinWorld.size());
            bool updated;
            do {
                updated=false;
                std::vector<Point2> positions;
                positions.reserve(skinWorld.size());
                for (std::size_t i=0;i<skinWorld.size();++i) {
                    const auto p=skinWorld[i];
                    const auto delta=allowedSkin[i] && !pinned[i] ? skinField[i] : Point2{0.,0.};
                    const auto prior=projected(profile.frame.local(p));
                    positions.push_back(dynamic ? projected(profile.frame.local({p[0]+delta[0],p[1]+delta[1]})) :
                        Point2{prior[0]+delta[0],prior[1]+delta[1]});
                }
                for (std::size_t i=0;i<outline.size();++i) {
                    const auto& sample=outline[i];
                    if (!sample.mapped || sample.section[1]<=profile.temple || sample.section[1]>=chin) continue;
                    Point2 moved{0.,0.}, original{0.,0.};
                    for (std::size_t j=0;j<3;++j) {
                        const auto vertex=sample.vertices[j];
                        const auto prior=projected(profile.frame.local(skinWorld[vertex]));
                        for (std::size_t axis=0;axis<2;++axis) {
                            moved[axis]+=sample.weights[j]*positions[vertex][axis];
                            original[axis]+=sample.weights[j]*prior[axis];
                        }
                    }
                    if (std::hypot(moved[0]-original[0],moved[1]-original[1])<1e-6 ||
                        redFace.clearance(moved)>.01) continue;
                    for (const auto vertex : sample.vertices) {
                        if (remove[vertex] || !allowedSkin[vertex] || pinned[vertex] ||
                            (skinField[vertex][0]==0. && skinField[vertex][1]==0.)) continue;
                        remove[vertex]=true;
                        updated=true;
                    }
                }
                for (std::size_t i=0;i<remove.size();++i) if (remove[i]) skinField[i]={0.,0.};
            } while (updated);
        }
        double anchorError = 0;
        for (const auto constraint : anchorConstraints) {
            Point2 error{0., 0.};
            for (std::size_t j = 0; j < 3; ++j)
                for (std::size_t axis = 0; axis < 2; ++axis)
                    error[axis] += constraint.weights[j] * skinField[constraint.vertices[j]][axis];
            anchorError = std::max(anchorError, std::hypot(error[0], error[1]));
        }

        const auto id = skin;
        const auto& target = skinTarget;
        const auto& world = skinWorld;
        const auto& allowed = allowedSkin;
        const auto inverse = contracts::ngRigNumbers(read(target, "root_to_local_direction"));
        Slice<double> displacement;
        double maximum = 0;
        for (std::size_t i = 0; i < world.size(); ++i) {
            const auto field = skinField[i];
            Point2 localDelta{inverse[0] * field[0] + inverse[1] * field[1], inverse[2] * field[0] + inverse[3] * field[1]};
            if (!allowed[i] || pinned[i]) localDelta = Point2{0., 0.};
            for (auto& delta : localDelta) {
                enforce(std::isfinite(delta), "Nonfinite cheek residual", 818);
                delta = std::rint(delta * 1e5) / 1e5; maximum = std::max(maximum, std::abs(delta)); displacement.append(delta);
            }
        }
        const double inverseDet = inverse[0]*inverse[3]-inverse[1]*inverse[2];
        Slice<Point2> posed;
        for (std::size_t i=0;i<world.size();++i) {
            const Point2 delta{
                (inverse[3]*displacement[2*i]-inverse[1]*displacement[2*i+1])/inverseDet,
                (inverse[0]*displacement[2*i+1]-inverse[2]*displacement[2*i])/inverseDet};
            const auto parent = projected(profile.frame.local(world[i]));
            posed.append(dynamic ? projected(profile.frame.local(
                {world[i][0]+delta[0],world[i][1]+delta[1]})) :
                Point2{parent[0]+delta[0],parent[1]+delta[1]});
        }
        auto operation = record({{"part",read(target,"part")},{"key",read(key,"key")},
            {"values",array(displacement)},{"requested_cheek_values",array(displacement)},
            {"ridge_samples",Json(ridgeSamples)},{"neck_junction",neckJunction},
            {"allowed_vertices",array(allowed)},{"near_side",Json(side)}});
        double squaredFitError = 0., maximumFitError = 0.;
        for (const auto& sample : ridgeSamples) {
            const auto vertices = contracts::ngRigNumbers(read(sample,"vertices"));
            const auto weights = contracts::ngRigNumbers(read(sample,"weights"));
            const auto wanted = contracts::ngRigPoint(read(sample,"target"));
            Point2 actual{0.,0.};
            for (std::size_t j=0;j<3;++j)
                for (std::size_t axis=0;axis<2;++axis)
                    actual[axis] += weights[j]*posed[sourceControlIndex(vertices[j])][axis];
            const double error = std::hypot(actual[0]-wanted[0],actual[1]-wanted[1]);
            squaredFitError += error*error;
            maximumFitError = std::max(maximumFitError,error);
        }
        if (!ridgeSamples.empty()) {
            operation.sourceIndexAssignSlot("ridge_rms_error") = Json(std::sqrt(squaredFitError/ridgeSamples.size()));
            operation.sourceIndexAssignSlot("ridge_max_error") = Json(maximumFitError);
        }
        if (!measureReference) operations.append(operation);
        if (!measureReference) for (const auto& follower : coincidentSkin) {
            Slice<double> values;
            for (const auto& sample : follower.samples) {
                Point2 delta{0., 0.};
                if (sample.mapped)
                    for (std::size_t j=0; j<3; ++j)
                        for (std::size_t axis=0; axis<2; ++axis)
                            delta[axis] += sample.weights[j]*displacement[2*sample.vertices[j]+axis];
                values.append(delta[0]);
                values.append(delta[1]);
            }
            operations.append(record({{"part",Json(follower.id)},{"key",read(key,"key")},
                {"values",array(values)},{"kind",Json("coincident_skin")}}));
        }
        if (measureReference) for (const auto& line : skinOutlines) {
            const auto local = contracts::ngRigNumbers(read(line,"root_to_local_direction"));
            Slice<double> values;
            for (const auto p : contracts::ngRigPoints(read(line,"world"))) {
                const auto q = profile.frame.local(p);
                Point2 delta{0.,0.};
                if (side && q[1]>profile.temple && q[1]<chin &&
                    std::abs(q[0]-edgeAt(q[1])) < .12*faceWidth) {
                    const double strength = 1.-smootherStep(
                        (std::abs(q[0]-edgeAt(q[1]))-.035*faceWidth)/(.085*faceWidth));
                    delta = unprojectResidual(p,{strength*curve(edgeRows,residualX,q[1]),
                        strength*curve(edgeRows,residualY,q[1])});
                }
                values.append(local[0]*delta[0]+local[1]*delta[1]);
                values.append(local[2]*delta[0]+local[3]*delta[1]);
            }
            operations.append(record({{"part",read(line,"part")},{"key",read(key,"key")},
                {"values",array(values)},{"kind",Json("skin_outline")}}));
        }
        std::vector<Point2> posedOutline;
        if (measureReference && side && !hairTargets.empty()) {
            for (std::size_t i : {std::size_t(0),std::size_t(1)}) {
                const auto append = [&](const ContourSample& sample) {
                    if (!sample.mapped) return;
                    Point2 p{0.,0.};
                    for (std::size_t j=0;j<3;++j)
                        for (std::size_t axis=0;axis<2;++axis)
                            p[axis] += sample.weights[j]*posed[sample.vertices[j]][axis];
                    posedOutline.push_back(profile.frame.local(p));
                };
                if (i==0) for (const auto& sample : contourSamples[i]) append(sample);
                else for (const auto& sample : contourSamples[i] | std::views::reverse) append(sample);
            }
        }
        const auto ridgeAtHeight = [&](double y, int exterior) -> std::optional<double> {
            std::optional<double> outer;
            for (std::size_t i=0;i<posedOutline.size();++i) {
                const auto& a = posedOutline[i];
                const auto& b = posedOutline[(i+1)%posedOutline.size()];
                if ((a[1]>y)==(b[1]>y)) continue;
                const double x = std::lerp(a[0],b[0],(y-a[1])/(b[1]-a[1]));
                if (!outer || exterior*(x-*outer)>0.) outer = x;
            }
            return outer;
        };
        if (measureReference) for (const auto& hair : hairTargets) {
            const double turnWeight = smootherStep((std::abs(value[0])-.5)/.5);
            if (!side || (turnWeight == 0. && hair.noseEdge.empty())) {
                Slice<double> zeros;
                zeros.resize(2*hair.vertices.size(),0.);
                operations.append(record({{"part",read(hair.target,"part")},{"key",read(key,"key")},
                    {"values",array(zeros)},{"kind",Json("hair_ridge")},{"band",Json(hairBand)}}));
                continue;
            }
            for (const auto& hairKey : keys) {
                if (read(hairKey,"grid")!=read(hair.target,"grid") || read(hairKey,"key")!=read(key,"key"))
                    continue;
                const auto hx = contracts::ngRigNumbers(read(hair.target,"xs"));
                const auto hy = contracts::ngRigNumbers(read(hair.target,"ys"));
                const auto frame = contracts::ngRigNumbers(read(hair.target,"parent_to_root"));
                const double det = frame[0]*frame[4]-frame[1]*frame[3];
                const auto baked = contracts::ngRigNumbers(read(hairKey,"offsets"));
                const auto local = contracts::ngRigNumbers(read(hair.target,"root_to_local_direction"));
                Slice<Point2> hairOffsets;
                for (std::size_t i=0;i<baked.size();i+=2)
                    hairOffsets.append(Point2{baked[i],baked[i+1]});
                const auto hairAt = [&](Point2 world) {
                    const auto q = controls_detail::gridSample(hairOffsets,hx,hy,
                        controls_detail::gridLocal(world,frame,det));
                    return Point2{world[0]+frame[0]*q[0]+frame[1]*q[1],
                                  world[1]+frame[3]*q[0]+frame[4]*q[1]};
                };
                const bool hairDynamic = contracts::ngRigGet(hair.target,
                    compat::RetainedString::fromStaticLiteral("grid_dynamic"),Json(false)).as_bool();
                double lift=0.;
                if (side && !hair.noseEdge.empty()) {
                    const auto anchor=profile.frame.local(composedPoint(*nose));
                    const double limit=anchor[1]-noseHalfHeight-
                        std::max(hair.noseClearance,.035*faceWidth);
                    for (const auto p : hair.noseEdge) {
                        const auto q=profile.frame.local(hairAt(p));
                        const double lateralWeight=1.-smootherStep((std::abs(q[0]-anchor[0])-.055*faceWidth)/(.055*faceWidth));
                        lift=std::max(lift,lateralWeight*(q[1]-limit));
                    }
                }
                Slice<double> values;
                for (const auto world : hair.vertices) {
                    const auto p = profile.frame.local(world);
                    const double center = profile.midline(p[1]);
                    const double halfWidth = side*(edgeAt(p[1])-center);
                    const double across = halfWidth > 1e-8 ?
                        smootherStep(side*(p[0]-center)/halfWidth) : 0.;
                    Point2 correction{0.,0.};
                    if (turnWeight>0. && hair.adjacent[sideIndex] && p[1]>profile.temple && p[1]<chin) {
                        const double edgeX = edgeAt(p[1]);
                        const double gap = side*(p[0]-edgeX);
                        // Widen the falloff to include the displacement needed
                        // to carry the exterior strand past the posed ridge.
                        const auto delta = Point2{curve(edgeRows,residualX,p[1]),curve(edgeRows,residualY,p[1])};
                        const Point2 neutralEdge{edgeX,p[1]};
                        const auto originalRidge = composedPoint(neutralEdge);
                        const auto originalHair = hairAt(world);
                        const double desiredGap = gap >= 0. ? std::min(gap,hairBand)*.2 : gap*.2;
                        const double posedGap = side*((originalHair[0]-originalRidge[0])*profile.frame.tangent[0]+
                            (originalHair[1]-originalRidge[1])*profile.frame.tangent[1]);
                        const double recovery = gap >= 0. ? std::max(0.,desiredGap-posedGap) : 0.;
                        const double band = hairBand+recovery+std::hypot(delta[0],delta[1]);
                        const double along = smootherStep((p[1]-profile.temple)/(.12*span))*
                            smootherStep((chin-p[1])/(.12*span));
                        const double weight = smootherStep(1.-std::abs(gap)/band)*along;
                        for (std::size_t axis=0;axis<2;++axis)
                            correction[axis] = weight*(delta[axis]+side*recovery*profile.frame.tangent[axis]);
                    }
                    const auto originalHair = hairAt(world);
                    if (turnWeight>0. && p[1]<chin && across > 0.) {
                        const auto candidate = profile.frame.local(
                            {originalHair[0]+correction[0],originalHair[1]+correction[1]});
                        const double gap = side*(p[0]-edgeAt(p[1]));
                        const auto ridge = ridgeAtHeight(candidate[1],side);
                        if (ridge && hair.adjacent[sideIndex] && gap >= -hairBand) {
                            const double clearance = side*(candidate[0]-*ridge);
                            const double weight = smootherStep(1.+gap/hairBand);
                            const double missing = weight*std::max(0.,.2*std::min(gap,hairBand)-clearance);
                            for (std::size_t axis=0;axis<2;++axis)
                                correction[axis] += side*missing*profile.frame.tangent[axis];
                        }
                    }
                    for (auto& delta : correction) delta *= across*turnWeight;
                    if (lift>0.) {
                        const double tip=(*nose)[1]-noseHalfHeight-hair.noseClearance;
                        const double weight=smootherStep(1.+(p[1]-tip)/(.20*faceWidth));
                        for (std::size_t axis=0;axis<2;++axis)
                            correction[axis] -= lift*weight*profile.frame.normal[axis];
                    }
                    if (hairDynamic && (correction[0]!=0. || correction[1]!=0.)) {
                        Point2 candidateWorld{world[0]+correction[0],world[1]+correction[1]};
                        for (unsigned iteration=0;iteration<12;++iteration) {
                            const auto actual = hairAt(candidateWorld);
                            for (std::size_t axis=0;axis<2;++axis)
                                candidateWorld[axis] += originalHair[axis]+correction[axis]-actual[axis];
                        }
                        correction = {candidateWorld[0]-world[0],candidateWorld[1]-world[1]};
                    }
                    values.append(local[0]*correction[0]+local[1]*correction[1]);
                    values.append(local[2]*correction[0]+local[3]*correction[1]);
                }
                operations.append(record({{"part",read(hair.target,"part")},{"key",read(key,"key")},
                    {"values",array(values)},{"kind",Json("hair_ridge")},{"band",Json(hairBand)}}));
                break;
            }
        }
        observations.append(record({{"part",read(target,"part")},{"key",read(key,"key")},{"near_side",Json(side)},
            {"near_depth_slope",Json(slope)},{"maximum_local_residual",Json(maximum)},
            {"feature_anchor_error",Json(anchorError)}}));
    }
    auto report = record({{"applicable", Json(true)}, {"mechanism", Json("alpha_near_cheek_inverse_grid")},
        {jawRidgeBlendOption, Json(jawRidgeBlend)},
        {"skin", Json(skin)}, {"operations", Json(operations)}, {"observations", Json(observations)}, {"skin_protected_vertices", array(pinned)},
        {"profile", record({{"origin", point(profile.frame.origin)}, {"tangent", point(profile.frame.tangent)},
            {"y", array(profile.y)}, {"left", array(profile.left)}, {"right", array(profile.right)}, {"temple_y", Json(profile.temple)},
            {"eye_y", Json(profile.eye)}, {"mouth_y", Json(profile.mouth)}})},
        {"program_sha256", read(program, "content_sha256")}, {"depth_angle_program_sha256", read(state, "depth_angle_program_sha256")},
        {"source_uv_program_sha256", read(state, "source_uv_program_sha256")}, {"source_sha256", read(state, "source_sha256")},
        {"visually_reviewed", Json(false)}});
    return report;
}

Json ngRigCompileCheekCorrections(Json state, Json program, std::shared_ptr<framework::AutoRigTaskContext> context) {
    const auto original = compileCheekCorrections(state, program, context, true);
    if (!read(original,"applicable").as_bool()) return original;
    auto report = compileCheekCorrections(state, program, context, false);
    auto operations = read(report,"operations").sourceArrayValue().duplicate();
    for (const auto& operation : read(original,"operations").sourceArrayValue())
        operations.append(operation);
    report.sourceIndexAssignSlot("operations") = operations;
    report.sourceIndexAssignSlot("content_sha256") = core::settings::SettingsString(contracts::ngRigDigest(report));
    return report;
}

} // namespace nijigenerate::autorig::deterministic::controls
