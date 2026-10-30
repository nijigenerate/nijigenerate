#include "anatomy.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <stdexcept>
#include "linear.hpp"
#include <common/resource_key_map.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>

namespace nijigenerate::autorig::deterministic::anatomy {
namespace {
// Only builtin byte storage; JSON access, arithmetic and exception construction stay outside.
template<class Operation> auto allocateAnatomyStorage(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
}

using core::settings::SettingsJsonArray;
using core::settings::SettingsJsonObject;
using core::settings::SettingsJsonValue;
using core::settings::SettingsString;
using String = SettingsString;
struct StringHash {
    std::size_t operator()(const String& value) const noexcept {
        return compat::ngPhobosStringHash(value.view());
    }
};
template<class Value> using StringMap = nicxlive::core::common::ResourceKeyMap<
    String, Value, StringHash, std::equal_to<String>>;
template<std::size_t N> String literal(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
std::size_t readIndex(const StringMap<std::size_t>& index, const String& role, std::size_t line) {
    const auto found = index.find(role);
    if (found == index.end()) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/anatomy.d"), line);
    return found->second;
}

SettingsJsonValue field(const SettingsJsonValue& value, std::string_view key) {
    return value.sourceIndexValue(key);
}

template<std::size_t N> void enforce(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) {
        const auto copied = allocateAnatomyStorage([&] {
            return compat::RetainedString(std::string_view(message, N - 1));
        });
        throw compat::ThrowableException(copied,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/anatomy.d"), line);
    }
}

SettingsJsonValue pointJson(contracts::Point2 point) {
    SettingsJsonArray result;
    result.resize(point.size());
    for (std::size_t i = 0; i < point.size(); ++i) result[i] = SettingsJsonValue(point[i]);
    return result;
}

} // namespace

SettingsJsonValue ngRigSolveScaffold(SettingsJsonValue evidence, SettingsJsonValue prior) {
    using contracts::ngRigNumber;
    using contracts::ngRigNumbers;
    using contracts::ngRigPoint;
    using contracts::ngRigScalar;
    using contracts::Point2;
    using detail::dot;
    using detail::length;
    using detail::subtract;

    StringMap<bool> seen;
    for (auto bone : field(prior, "bone_graph").sourceArrayValue()) {
        seen[bone.sourceIndexValue(std::size_t{2}).as_string()] = true;
        seen[bone.sourceIndexValue(std::size_t{3}).as_string()] = true;
    }
    auto roles = seen.keys();
    if (roles.size() > 1)
        std::sort(roles.data(), roles.data() + roles.size(), [](const String& left, const String& right) {
            return left.view() < right.view();
        });
    StringMap<std::size_t> index;
    compat::Slice<Point2> source;
    compat::Slice<double> weights;
    const auto transform = field(evidence, "source_to_model").sourceArrayValue();
    enforce(transform.size() == 3, "Expected a 3x3 source transform", 28);
    const auto t0 = ngRigNumbers(transform[0]);
    const auto t1 = ngRigNumbers(transform[1]);
    const auto t2 = ngRigNumbers(transform[2]);
    enforce(t0.size() == 3 && t1.size() == 3 && t2.size() == 3 &&
        t2[0] == 0. && t2[1] == 0. && t2[2] == 1., "Invalid source transform", 30);
    const double determinant = t0[0] * t1[1] - t0[1] * t1[0];
    enforce(determinant > 0 && std::abs(t0[0] * t0[0] + t1[0] * t1[0] - determinant) < 1e-6 * determinant &&
        std::abs(t0[1] * t0[1] + t1[1] * t1[1] - determinant) < 1e-6 * determinant &&
        std::abs(t0[0] * t0[1] + t1[0] * t1[1]) < 1e-6 * determinant,
        "Source transform is not a similarity", 32);
    for (std::size_t i = 0; i < roles.size(); ++i) {
        const auto role = roles[i];
        index[role] = i;
        const auto entry = field(field(evidence, "landmarks"), role.view());
        const auto provenance = field(entry, "provenance").as_string();
        enforce(provenance.view() == "measured" || provenance.view() == "prior" ||
            provenance.view() == "visual_estimate", "Invalid landmark provenance", 39);
        const auto point = ngRigPoint(field(entry, "xy"));
        const Point2 modelPoint{t0[0] * point[0] + t0[1] * point[1] + t0[2],
            t1[0] * point[0] + t1[1] * point[1] + t1[2]};
        source.append(modelPoint);
        const double weight = ngRigScalar(entry, compat::RetainedString::fromStaticLiteral("weight"), 1);
        enforce(weight > 0, "Invalid landmark weight", 45);
        weights.append(weight);
    }
    const auto originalHeadRoot = source[readIndex(index, literal("head_root"), 48)];
    const auto originalHeadTop = source[readIndex(index, literal("head_top"), 48)];
    const auto down = subtract(originalHeadRoot, originalHeadTop);
    enforce(length(down) > 0, "Degenerate head axis", 49);
    const auto originalNeck = source[readIndex(index, literal("neck_base"), 50)];
    const auto currentHeadRoot = source[readIndex(index, literal("head_root"), 50)];
    if (dot(subtract(originalNeck, currentHeadRoot), down) <= 0) {
        for (std::size_t axis = 0; axis < 2; ++axis) {
            const auto left = source[readIndex(index, literal("shoulder.L"), 52)][axis];
            const auto right = source[readIndex(index, literal("shoulder.R"), 52)][axis];
            source[readIndex(index, literal("neck_base"), 51)][axis] = (left + right) / 2;
        }
    }
    const auto policy = field(prior, "torso_axis");
    const auto start = field(policy, "start").as_string();
    const auto end = field(policy, "end").as_string();
    const auto origin = source[readIndex(index, start, 55)];
    Point2 tip{0., 0.};
    const auto hips = field(policy, "end_attachment").sourceArrayValue();
    for (auto hip : hips) for (std::size_t axis = 0; axis < 2; ++axis)
        tip[axis] += source[readIndex(index, hip.as_string(), 58)][axis] / hips.size();
    const auto direction = subtract(tip, origin);
    const double length2 = dot(direction, direction);
    enforce(length2 > 0, "Degenerate torso axis", 61);
    StringMap<double> fractions;
    double previous = 0;
    for (auto station : field(policy, "stations").sourceArrayValue()) {
        const auto observation = source[readIndex(index, station.as_string(), 65)];
        const double fraction = dot(subtract(observation, origin), direction) / length2;
        enforce(fraction - previous >= ngRigNumber(field(policy, "minimum_station_gap")),
            "Invalid torso station order", 66);
        fractions[station.as_string()] = fraction;
        previous = fraction;
    }
    enforce(1 - previous >= ngRigNumber(field(policy, "minimum_station_gap")), "Invalid torso end station", 70);
    compat::Slice<String> independent;
    for (const auto role : roles) if (role.view() != end.view() && !fractions.contains(role)) independent.append(role);
    auto basis = linear::ngRigZeroMatrix(roles.size(), independent.size());
    for (std::size_t column = 0; column < independent.size(); ++column)
        basis[readIndex(index, independent[column], 74)][column] = 1;
    for (auto hip : hips) for (std::size_t column = 0; column < independent.size(); ++column)
        basis[readIndex(index, end, 76)][column] += basis[readIndex(index, hip.as_string(), 76)][column] / hips.size();
    for (const auto [role, fraction] : fractions) {
        for (std::size_t column = 0; column < independent.size(); ++column) {
            const auto first = basis[readIndex(index, start, 78)][column];
            const auto last = basis[readIndex(index, end, 78)][column];
            basis[readIndex(index, role, 78)][column] = (1 - fraction) * first + fraction * last;
        }
    }
    linear::RigMatrix rows, targets;
    for (std::size_t i = 0; i < roles.size(); ++i) if (!fractions.contains(roles[i])) {
        const double weight = std::sqrt(weights[i]);
        auto row = basis[i].duplicate();
        for (auto& entry : row) entry *= weight;
        rows.append(row);
        targets.append(linear::RigRow{source[i][0] * weight, source[i][1] * weight});
    }
    for (auto constraint : field(prior, "joint_constraints").sourceArrayValue()) {
        linear::RigRow row;
        row.resize(independent.size(), 0.);
        const double weight = std::sqrt(ngRigNumber(field(constraint, "weight")));
        // Accumulation order follows the actual JSON object provider, never a sorted substitute.
        const auto terms = field(constraint, "terms");
        for (const auto term : terms.sourceObjectRef())
            for (std::size_t column = 0; column < independent.size(); ++column) {
                const auto coefficient = ngRigNumber(term.value());
                const auto basisValue = basis[readIndex(index, term.name, 91)][column];
                row[column] += coefficient * weight * basisValue;
            }
        rows.append(row);
        targets.append(linear::RigRow{0., 0.});
    }
    const auto solved = linear::ngRigLeastSquares(rows, targets);
    compat::Slice<Point2> fitted;
    SettingsJsonObject points, residuals;
    for (std::size_t i = 0; i < roles.size(); ++i) {
        Point2 point{0., 0.};
        for (std::size_t column = 0; column < independent.size(); ++column)
            for (std::size_t axis = 0; axis < 2; ++axis) point[axis] += basis[i][column] * solved[column][axis];
        fitted.append(point);
    }
    const auto headRoot = fitted[readIndex(index, literal("head_root"), 106)];
    const auto headTop = fitted[readIndex(index, literal("head_top"), 106)];
    auto headDown = subtract(headRoot, headTop);
    const double headLength = length(headDown);
    for (auto& value : headDown) value /= headLength;
    const auto unconstrainedNeck = fitted[readIndex(index, literal("neck_base"), 108)];
    const double neckStation = dot(subtract(unconstrainedNeck, headRoot), headDown);
    enforce(neckStation > 0, "Neck attachment must be below Head along the facial axis", 110);
    for (std::size_t axis = 0; axis < 2; ++axis)
        fitted[readIndex(index, literal("neck_base"), 111)][axis] = headRoot[axis] + neckStation * headDown[axis];
    for (const auto [role, fraction] : fractions) for (std::size_t axis = 0; axis < 2; ++axis) {
        const auto first = fitted[readIndex(index, start, 113)][axis];
        const auto last = fitted[readIndex(index, end, 113)][axis];
        fitted[readIndex(index, role, 113)][axis] = (1 - fraction) * first + fraction * last;
    }
    for (std::size_t i = 0; i < roles.size(); ++i) {
        points[SettingsString(roles[i])] = pointJson(fitted[i]);
        residuals[SettingsString(roles[i])] = length(subtract(fitted[i], source[i]));
    }
    Point2 feet;
    for (std::size_t axis = 0; axis < 2; ++axis) {
        const auto left = fitted[readIndex(index, literal("foot_tip.L"), 119)][axis];
        const auto right = fitted[readIndex(index, literal("foot_tip.R"), 119)][axis];
        feet[axis] = (left + right) / 2;
    }
    const double height = length(subtract(fitted[readIndex(index, literal("head_top"), 120)], feet));
    enforce(height > 0, "Invalid body height", 121);
    for (const auto residual : residuals) {
        const auto relativeResidual = ngRigNumber(residual.value()) / height;
        const auto maximumResidual = ngRigNumber(
            field(field(prior, "acceptance"), "joint_fit_max_relative_residual"));
        if (!(relativeResidual <= maximumResidual)) {
            // enforce evaluates its lazy message only on failure, then duplicates the counted bytes.
            const auto message = allocateAnatomyStorage([&] {
                return compat::RetainedString(
                    "Landmark residual exceeds tolerance: " + std::string(residual.name.view()));
            });
            const auto copied = allocateAnatomyStorage([&] { return compat::RetainedString(message.view()); });
            throw compat::ThrowableException(copied,
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/anatomy.d"), 123);
        }
    }
    SettingsJsonArray bones;
    for (auto definition : field(prior, "bone_graph").sourceArrayValue()) {
        const auto first = fitted[readIndex(index, definition.sourceIndexValue(std::size_t{2}).as_string(), 127)];
        const auto last = fitted[readIndex(index, definition.sourceIndexValue(std::size_t{3}).as_string(), 127)];
        enforce(length(subtract(first, last)) >= height * 1e-5, "Zero-length anatomical bone", 128);
        SettingsJsonObject bone;
        bone[literal("id")] = definition.sourceIndexValue(std::size_t{0});
        bone[literal("parent")] = definition.sourceIndexValue(std::size_t{1});
        bone[literal("head")] = SettingsJsonArray{
            SettingsJsonValue(first[0]), SettingsJsonValue(first[1]), SettingsJsonValue(0.)};
        bone[literal("tail")] = SettingsJsonArray{
            SettingsJsonValue(last[0]), SettingsJsonValue(last[1]), SettingsJsonValue(0.)};
        bone[literal("lock_to_root")] =
            definition.sourceIndexValue(std::size_t{0}).as_string().view().starts_with("Foot.");
        bone[literal("rest_roll")] = 0.;
        bones.append(SettingsJsonValue(bone));
    }
    SettingsJsonObject volumes;
    const auto sourceVolumes = field(evidence, "volumes");
    for (const auto member : sourceVolumes.sourceObjectRef()) {
        const auto name = member.name;
        const auto spec = member.value();
        auto center = ngRigPoint(field(spec, "center"));
        auto radii = ngRigPoint(field(spec, "radii"));
        center = Point2{t0[0] * center[0] + t0[1] * center[1] + t0[2],
            t1[0] * center[0] + t1[1] * center[1] + t1[2]};
        const double scale = std::sqrt(determinant);
        for (auto& value : radii) value *= scale;
        Point2 axis;
        if (name.view() == "head") {
            const auto first = fitted[readIndex(index, literal("head_root"), 138)];
            const auto last = fitted[readIndex(index, literal("head_top"), 138)];
            axis = subtract(first, last);
        } else {
            const auto first = fitted[readIndex(index, end, 139)];
            const auto last = fitted[readIndex(index, start, 139)];
            axis = subtract(first, last);
        }
        const double axisLength = length(axis);
        for (auto& value : axis) value /= axisLength;
        if (name.view() == "torso") {
            for (std::size_t i = 0; i < 2; ++i) {
                const auto first = fitted[readIndex(index, start, 142)][i];
                const auto last = fitted[readIndex(index, end, 142)][i];
                center[i] = (first + last) / 2;
            }
            radii[1] = axisLength / 2;
        }
        SettingsJsonArray frame{pointJson(Point2{axis[1], axis[0]}), pointJson(Point2{-axis[0], axis[1]})};
        SettingsJsonObject volume;
        volume[literal("center")] = pointJson(center);
        volume[literal("radii")] = pointJson(radii);
        volume[literal("frame")] = frame;
        volumes[name] = SettingsJsonValue(volume);
    }
    SettingsJsonObject result;
    result[literal("schema_version")] = literal("rig-scaffold-d/1");
    result[literal("landmarks")] = points;
    result[literal("bones")] = bones;
    result[literal("volumes")] = volumes;
    result[literal("body_height")] = height;
    result[literal("residual_model_units")] = residuals;
    result[literal("evidence_sha256")] = SettingsString(contracts::ngRigDigest(evidence));
    return result;
}

} // namespace nijigenerate::autorig::deterministic::anatomy
