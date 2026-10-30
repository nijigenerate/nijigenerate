#include "surface.hpp"
#include "geometry.hpp"
#include "depth.hpp"
#include "templates.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <common/string_key_map.hpp>
#include <cmath>
#include <functional>
#include <type_traits>
#include <utility>
namespace nijigenerate::autorig::deterministic::surface {
using namespace contracts;
using namespace geometry;
using namespace depth;
using namespace templates;
using Json = core::settings::SettingsJsonValue;
using Object = core::settings::SettingsJsonObject;
namespace {
using Text = compat::RetainedString;
[[noreturn]] void fail(Text message, std::size_t line) {
    throw compat::ThrowableException(std::move(message),
        Text::fromStaticLiteral("source/nijigenerate/autorig/deterministic/surface.d"),line);
}
template<std::size_t N> void require(bool ok, const char (&message)[N], std::size_t line) {
    if (!ok) fail(contracts::detail::copyContractMessage(std::string_view(message, N - 1)),line);
}
template<class Message> requires std::is_invocable_v<Message>
void require(bool ok, Message&& message, std::size_t line) {
    if (!ok) {
        const auto evaluated = std::invoke(std::forward<Message>(message));
        fail(contracts::detail::copyContractMessage(std::string_view(evaluated)),line);
    }
}
Object& object(Json& json) { return json.sourceObjectRef(); }
Json member(Json json, std::string_view key) { return json.sourceIndexValue(key); }
core::settings::SettingsString string(Json json) {
    return json.as_string();
}
compat::Slice<Json> array(Json json) {
    return json.as_array();
}
bool boolean(Json json) {
    return json.as_bool();
}
template<std::size_t N> Json get(Json json, const char (&key)[N], Json fallback = {}) {
    return ngRigGet(json, Text::fromStaticLiteral(key), fallback);
}
Json numbers(compat::Slice<double> values) {
    core::settings::SettingsJsonArray result; for (double value : values) result.append(Json(value)); return result;
}
}
Json ngRigEvaluateTemplate(Json request) {
    auto definition = ngRigTemplate(string(member(request, "template")));
    auto bounds = ngRigNumbers(member(request, "bounds"));
    require(bounds.size() == 4, "Expected four source bounds",15);
    double width = bounds[2] - bounds[0]; require(width > 0 && bounds[3] > bounds[1], "Empty surface bounds",17);
    auto supplied = get(request, "parameters", Object{}); Object parameters;
    for (auto tunable : array(member(definition, "tunables"))) {
        auto id = string(member(tunable, "id"));
        auto candidate = ngRigGet(supplied, id, member(tunable, "default")); double number = ngRigNumber(candidate);
        require(number >= ngRigNumber(member(tunable, "min")) && number <= ngRigNumber(member(tunable, "max")),
            [&] {
                return contracts::detail::allocateContractStorage([&] {
                    return "Template parameter exceeds declared range: " + std::string(id.view());
                });
            },24);
        parameters[id] = number;
    }
    for (const auto entry : object(supplied))
        require(parameters.if_contains(entry.key()) != nullptr, [&] {
                return contracts::detail::allocateContractStorage([&] {
                    return "Unknown template parameter: " + std::string(entry.key());
                });
            },28);
    compat::Slice<double> xs, ys;
    for (auto column : array(member(member(definition, "guide_grid"), "columns"))) xs.append(ngRigNumber(member(column, "u")));
    for (auto row : array(member(member(definition, "guide_grid"), "rows"))) ys.append(ngRigNumber(member(row, "v")));
    auto uv = ngRigUVGrid(xs, ys); auto evidence = get(request, "landmarks", Object{});
    bool sourceFit = boolean(get(request, "sourceFit", Json(true))); compat::Slice<Point2> source, target;
    nicxlive::core::common::StringKeyMap<bool> known;
    for (auto landmark : array(member(definition, "landmarks"))) {
        auto id = string(member(landmark, "id")); known.setRetained(id.retainedString(),true);
        auto suppliedPoint = object(evidence).if_contains(id.view());
        require(!sourceFit || !boolean(member(landmark, "required")) || suppliedPoint,
            [&] {
                return contracts::detail::allocateContractStorage([&] {
                    return "Missing required source landmark: " + std::string(id.view());
                });
            },40);
        if (suppliedPoint) { source.append(ngRigPoint(member(landmark, "uv"))); target.append(ngRigPoint(*suppliedPoint)); }
    }
    for (const auto entry : object(evidence))
        require(known.contains(entry.key()), [&] {
                return contracts::detail::allocateContractStorage([&] {
                    return "Unknown source landmark: " + std::string(entry.key());
                });
            },44);
    compat::FixedArray<double, 4> box{bounds[0], bounds[1], bounds[2], bounds[3]};
    auto guide = ngRigFitGuide(uv, box, source, target); auto validation = ngRigValidateGrid(guide, xs.size());
    compat::Slice<double> host; if (auto value = object(request).if_contains("hostDepth")) host = ngRigNumbers(*value);
    auto depthValues = ngRigEvaluateDepth(uv, member(member(definition, "geometry"), "operators"), parameters, host);
    compat::Slice<Point3> xyz;
    for (std::size_t i = 0; i < guide.size(); ++i) {
        auto p = guide[i];
        const auto absoluteDepth=std::abs(depthValues[i]);
        const auto maximumDepth=ngRigNumber(member(member(definition, "validation"), "max_depth_width"));
        require(absoluteDepth <= maximumDepth, "Template depth exceeds bound",53);
        xyz.append(Point3{p[0], p[1], depthValues[i] * width});
    }
    Point3 pose{0., 0., 0.}, pivot{(bounds[0] + bounds[2]) / 2, (bounds[1] + bounds[3]) / 2, 0.};
    if (auto suppliedPose = object(request).if_contains("pose")) {
        auto values = ngRigNumbers(*suppliedPose); require(values.size() == 3, "Expected yaw/pitch/roll",58);
        for (std::size_t i = 0; i < 3; ++i) pose[i] = values[i];
    }
    if (auto suppliedPivot = object(request).if_contains("pivot")) {
        auto values = ngRigNumbers(*suppliedPivot); require(values.size() == 3, "Expected a 3D pivot",61);
        for (std::size_t i = 0; i < 3; ++i) pivot[i] = values[i];
    }
    auto rotated = ngRigRotate(xyz, pivot, pose); compat::Slice<Point2> projected;
    for (auto p : rotated) projected.append(Point2{p[0], p[1]});
    projected = ngRigLocalCorrections(projected, uv, pose, {0., 0., 0.}, member(definition, "correction_rules"), parameters, width);
    auto posedValidation = ngRigValidateGrid(projected, xs.size()); Object result;
    result[Text::fromStaticLiteral("schema_version")] =
        core::settings::SettingsString(Text::fromStaticLiteral("rig-template-surface-d/1"));
    result[Text::fromStaticLiteral("template")] = member(definition, "id");
    result[Text::fromStaticLiteral("version")] = member(definition, "version");
    result[Text::fromStaticLiteral("source_fit")] = sourceFit;
    result[Text::fromStaticLiteral("xs")] = numbers(xs);
    result[Text::fromStaticLiteral("ys")] = numbers(ys);
    result[Text::fromStaticLiteral("uv")] = ngRigPointsJson(uv);
    result[Text::fromStaticLiteral("neutral")] = ngRigPointsJson(guide);
    result[Text::fromStaticLiteral("depth")] = numbers(depthValues);
    result[Text::fromStaticLiteral("projected")] = ngRigPointsJson(projected);
    result[Text::fromStaticLiteral("neutral_validation")] = validation;
    result[Text::fromStaticLiteral("posed_validation")] = posedValidation;
    result[Text::fromStaticLiteral("request_sha256")] = core::settings::SettingsString(ngRigDigest(request));
    return result;
}
}
