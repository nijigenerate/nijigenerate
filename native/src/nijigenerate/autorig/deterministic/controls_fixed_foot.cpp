#include "controls_fixed_foot.hpp"
#include "controls.hpp"
#include "controls_dependencies.hpp"
#include "templates.hpp"
#include "geometry.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <common/resource_key_map.hpp>
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <boost/multiprecision/cpp_bin_float.hpp>
#endif
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <numbers>

namespace nijigenerate::autorig::deterministic::controls_detail {
namespace {
struct IdHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        constexpr auto shift = sizeof(std::size_t) < sizeof(value) ? sizeof(std::size_t) * 8 : 0;
        return static_cast<std::size_t>(value ^ (shift ? value >> shift : 0));
    }
};
using FixedFootSeen = nicxlive::core::common::ResourceKeyMap<std::uint64_t, bool,
    IdHash, std::equal_to<std::uint64_t>>;

template<std::size_t N>
void enforce(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) throw compat::ThrowableException(contracts::detail::copyContractMessage(std::string_view(message,N-1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/controls.d"), line);
}

double fixedFootAngle(double magnitude, double key) {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    using SourceReal = boost::multiprecision::cpp_bin_float_double_extended;
    const SourceReal pi = boost::multiprecision::ldexp(SourceReal(std::uint64_t{0xc90fdaa22168c235}), -62);
    const SourceReal product = SourceReal(magnitude * key) * pi;
    const SourceReal angle = product / SourceReal(180);
    return angle.convert_to<double>();
#else
    return magnitude * key * std::numbers::pi_v<long double> / 180;
#endif
}
}

FixedFootTargetInput prepareFixedFootTarget(core::settings::SettingsJsonValue target,
    core::settings::SettingsJsonValue landmarks) {
    FixedFootTargetInput prepared;
    prepared.side = target.sourceIndexValue("side").as_string();
    const auto side = std::string(prepared.side.view());
    prepared.joints = {contracts::ngRigPoint(landmarks.sourceIndexValue("hip." + side)),
        contracts::ngRigPoint(landmarks.sourceIndexValue("knee." + side)),
        contracts::ngRigPoint(landmarks.sourceIndexValue("ankle." + side))};
    prepared.axis = contracts::Point2{prepared.joints[2][0] - prepared.joints[0][0],
        prepared.joints[2][1] - prepared.joints[0][1]};
    const double length = std::sqrt(prepared.axis[0] * prepared.axis[0] +
        prepared.axis[1] * prepared.axis[1]);
    enforce(length > 0, "Degenerate fixed-foot axis", 520);
    for (auto& value : prepared.axis) value /= length;
    for (std::size_t i = 0; i < prepared.joints.size(); ++i) {
        const auto point = prepared.joints[i];
        prepared.stations[i] = (point[0] - prepared.joints[0][0]) * prepared.axis[0] +
            (point[1] - prepared.joints[0][1]) * prepared.axis[1];
    }
    enforce(prepared.stations[0] < prepared.stations[1] && prepared.stations[1] < prepared.stations[2],
        "Nonmonotone fixed-foot stations", 523);
    prepared.xs = contracts::ngRigNumbers(target.sourceIndexValue("xs"));
    prepared.ys = contracts::ngRigNumbers(target.sourceIndexValue("ys"));
    prepared.matrix = contracts::ngRigNumbers(target.sourceIndexValue("parent_to_root"));
    const double m00 = prepared.matrix[0];
    const double m11 = prepared.matrix[4];
    const double m01 = prepared.matrix[1];
    const double m10 = prepared.matrix[3];
    prepared.determinant = m00 * m11 - m01 * m10;
    enforce(std::abs(prepared.determinant) > 1e-12, "Singular fixed-foot carrier frame", 527);
    return prepared;
}

core::settings::SettingsJsonValue fixedFootKey(core::settings::SettingsJsonValue target,
    std::size_t index, double angle, std::array<contracts::Point2, 3> joints,
    contracts::Point2 pelvis, contracts::Point2 axis, contracts::Point3 stations,
    compat::Slice<double> xs, compat::Slice<double> ys, compat::Slice<double> matrix,
    double determinant, compat::RetainedStringSlice side) {
    using core::settings::SettingsJsonArray;
    using core::settings::SettingsJsonObject;
    using core::settings::SettingsJsonValue;
    std::array<contracts::Point2, 3> delta;
    for (std::size_t i = 0; i < joints.size(); ++i) {
        const auto point = joints[i];
        const double x = point[0] - pelvis[0], y = point[1] - pelvis[1];
        delta[i] = contracts::Point2{
            std::cos(angle) * x - std::sin(angle) * y + pelvis[0] - point[0],
            std::sin(angle) * x + std::cos(angle) * y + pelvis[1] - point[1]};
    }
    delta[2] = contracts::Point2{0., 0.};
    compat::Slice<contracts::Point2> posed, rest;
    compat::Slice<double> offsets;
    for (const double y : ys) for (const double x : xs) {
        rest.append(contracts::Point2{x, y});
        const contracts::Point2 world{matrix[0] * x + matrix[1] * y + matrix[2],
            matrix[3] * x + matrix[4] * y + matrix[5]};
        const double station = (world[0] - joints[0][0]) * axis[0] +
            (world[1] - joints[0][1]) * axis[1];
        const std::size_t segment = station <= stations[1] ? 0 : 1;
        const double weight = smoothStep((station - stations[segment]) /
            (stations[segment + 1] - stations[segment]));
        contracts::Point2 displacement{
            delta[segment][0] * (1 - weight) + delta[segment + 1][0] * weight,
            delta[segment][1] * (1 - weight) + delta[segment + 1][1] * weight};
        if (target.sourceIndexValue("bones").as_array().size() == 1 &&
                target.sourceIndexValue("bones").sourceIndexValue(std::size_t{0}).as_string().view() ==
                    std::string("Foot.") + std::string(side.view()))
            displacement = contracts::Point2{0., 0.};
        contracts::Point2 local{(matrix[4] * displacement[0] - matrix[1] * displacement[1]) / determinant,
            (-matrix[3] * displacement[0] + matrix[0] * displacement[1]) / determinant};
        for (auto& value : local) value = std::round(value * 1e4) / 1e4;
        offsets.append(local[0]);
        offsets.append(local[1]);
        posed.append(contracts::Point2{x + local[0], y + local[1]});
    }
    auto validation = geometry::ngRigValidateGrid(posed, xs.size(), .05, rest);
    SettingsJsonObject operation;
    operation["grid"] = target.sourceIndexValue("grid");
    operation["parameter"] = "Body::Roll";
    operation["key"] = SettingsJsonValue(SettingsJsonArray{
        SettingsJsonValue(index), SettingsJsonValue(std::size_t{0})});
    SettingsJsonArray values;
    for (const double offset : offsets) values.append(SettingsJsonValue(offset));
    operation["values"] = SettingsJsonValue(values);
    operation["validation"] = validation;
    return SettingsJsonValue(operation);
}

} // namespace nijigenerate::autorig::deterministic::controls_detail

namespace nijigenerate::autorig::deterministic::controls {

core::settings::SettingsJsonValue ngRigCompileFixedFootCorrections(core::settings::SettingsJsonValue state,
    core::settings::SettingsJsonValue program, std::shared_ptr<framework::AutoRigTaskContext> context) {
    using core::settings::SettingsJsonArray;
    using core::settings::SettingsJsonObject;
    using core::settings::SettingsJsonValue;
    SettingsJsonArray operations;
    if (contracts::ngRigString(state, "kind", "humanoid").view() != "humanoid") {
        SettingsJsonObject report;
        report["operations"] = SettingsJsonValue(operations);
        return SettingsJsonValue(report);
    }
    auto landmarks = program.sourceIndexValue("scaffold").sourceIndexValue("landmarks");
    const auto pelvis = contracts::ngRigPoint(landmarks.sourceIndexValue("pelvis"));
    const double magnitude = contracts::ngRigNumber(
        templates::ngRigHumanoidPrior().sourceIndexValue("drivers").sourceIndexValue("body_roll_degrees"));
    controls_detail::FixedFootSeen seen;
    const auto targets = state.sourceIndexValue("targets").as_array();
    for (const auto target : targets) {
        if (!target.sourceIndexValue("owner").as_string().view().starts_with("leg:") ||
                seen.contains(contracts::ngRigUnsigned(target.sourceIndexValue("grid")))) continue;
        seen[contracts::ngRigUnsigned(target.sourceIndexValue("grid"))] = true;
        auto prepared = controls_detail::prepareFixedFootTarget(target, landmarks);
        const double keys[5]{-1., -.5, 0., .5, 1.};
        for (std::size_t index = 0; index < 5; ++index) {
            contracts::ngRigCheckpoint(context);
            const double angle = controls_detail::fixedFootAngle(magnitude, keys[index]);
            operations.append(controls_detail::fixedFootKey(target, index, angle, prepared.joints,
                pelvis, prepared.axis, prepared.stations, prepared.xs, prepared.ys, prepared.matrix,
                prepared.determinant, prepared.side));
        }
    }
    SettingsJsonObject report;
    report["operations"] = SettingsJsonValue(operations);
    report["method"] = "shared longitudinal cubic Hermite with fixed ankle";
    report["program_sha256"] = program.sourceIndexValue("content_sha256");
    auto result = SettingsJsonValue(report);
    result.as_object()["content_sha256"] = core::settings::SettingsString(contracts::ngRigDigest(result));
    return result;
}

} // namespace nijigenerate::autorig::deterministic::controls
