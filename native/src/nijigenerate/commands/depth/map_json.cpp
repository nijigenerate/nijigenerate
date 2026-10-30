#include "map_json.hpp"
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <emmintrin.h>
#include <algorithm>
#include <bit>
namespace nijigenerate::commands::depth::map {
namespace detail {
namespace {
Object& sourceObjectRef(Json& value) {
    return value.sourceObjectRef();
}
}
void enforce(bool condition, std::string_view text) {
    if (!condition) throw DepthMapValueError(std::string(text));
}
float jsonFloat(Json value, compat::RetainedString name, float fallback) {
    if (value.is_null()) return fallback;
    if (value.is_double()) return finiteFloat(float(value.as_double()), name);
    if (value.is_int64()) return finiteFloat(float(value.as_int64()), name);
    if (value.is_uint64()) return finiteFloat(float(value.as_uint64()), name);
    const auto argument = core::translation::ngFormatArgument(name);
    throw depthMapEnforcementFailure(compat::RetainedString(core::translation::ngFormatTyped(
        "%s must be a number", std::span(&argument, 1))),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 154);
    compat::ngAssertUnreachable("Assertion failure", "source/nijigenerate/commands/depth/map.d", 156);
}
std::size_t jsonSize(Json value, compat::RetainedString name, std::size_t fallback) {
    if (value.is_null()) return fallback;
    if (value.is_int64()) {
        if (!(value.as_int64() >= 0)) {
            const auto argument = core::translation::ngFormatArgument(name);
            throw depthMapEnforcementFailure(compat::RetainedString(core::translation::ngFormatTyped(
                "%s must be >= 0", std::span(&argument, 1))),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 163);
        }
        return std::size_t(value.as_int64());
    }
    if (value.is_uint64()) return std::size_t(value.as_uint64());
    if (value.is_double()) {
        const double number = value.as_double();
        if (!(number >= 0 && number == double(_mm_cvttsd_si64(_mm_set_sd(number))))) {
            const auto argument = core::translation::ngFormatArgument(name);
            throw depthMapEnforcementFailure(compat::RetainedString(core::translation::ngFormatTyped(
                "%s must be a non-negative integer", std::span(&argument, 1))),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 168);
        }
        return std::size_t(number);
    }
    const auto argument = core::translation::ngFormatArgument(name);
    throw depthMapEnforcementFailure(compat::RetainedString(core::translation::ngFormatTyped(
        "%s must be a non-negative integer", std::span(&argument, 1))),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 177);
    compat::ngAssertUnreachable("Assertion failure", "source/nijigenerate/commands/depth/map.d", 179);
}
Vec2 jsonVec2(Json value, compat::RetainedString name, Vec2 fallback) {
    if (value.is_null()) return fallback;
    if (!(value.is_array() && value.sourceArrayValue().size() == 2)) {
        const auto argument = core::translation::ngFormatArgument(name);
        throw depthMapEnforcementFailure(compat::RetainedString(core::translation::ngFormatTyped(
            "%s must be [x, y]", std::span(&argument, 1))),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 184);
    }
    return {jsonFloat(value.sourceArrayValue()[0], std::string(name.view()) + "[0]"),
        jsonFloat(value.sourceArrayValue()[1], std::string(name.view()) + "[1]")};
}
Json vec2ToJson(Vec2 value) {
    Array result;
    result.append(Json(double(value.x))); result.append(Json(double(value.y)));
    return Json(result);
}
ext::nodes::ExDepthOp depthOpFromJson(Json value) {
    using namespace ext::nodes;
    if (!value.is_object()) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("operation must be an object"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 225);
    auto& object = sourceObjectRef(value);
    if (!object.if_contains("type")) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("operation.type is required"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 226);
    if (!value.sourceIndexValue("type").is_string()) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("operation.type must be a string"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 227);
    const auto typeValue = value.sourceIndexValue("type");
    const auto type = typeValue.as_string().view();
    const auto get = [&](std::string_view key, Json fallback = nullptr) {
        const auto found = sourceObjectRef(value).if_contains(key); return found ? *found : fallback;
    };
    ExDepthOp op;
    if (type == "attached-point" || type == "AttachedPoint") {
        op.type = ExDepthOpType::AttachedPoint;
        op.index = jsonSize(get("index"), "index");
        op.amount = jsonFloat(get("amount", 0.0), "amount");
        return op;
    }
    if (type == "ring" || type == "Ring") {
        op.type = ExDepthOpType::Ring;
        op.p0 = jsonVec2(get("p0"), "p0"); op.p1 = jsonVec2(get("p1"), "p1");
        op.amount = jsonFloat(get("amount", 0.0), "amount");
        op.width = std::max(0.5f, jsonFloat(get("width", 1.0), "width"));
        op.hardness = std::max(0.1f, jsonFloat(get("hardness", 1.0), "hardness"));
        op.p0Angle = jsonFloat(get("p0Angle", 180.0), "p0Angle");
        op.p1Angle = jsonFloat(get("p1Angle", 0.0), "p1Angle");
        return op;
    }
    if (type == "plane" || type == "Plane") {
        op.type = ExDepthOpType::Plane; op.center = jsonVec2(get("center"), "center");
        op.radiusX = std::max(1.0f, jsonFloat(get("radiusX", 1.0), "radiusX"));
        op.radiusY = std::max(1.0f, jsonFloat(get("radiusY", 1.0), "radiusY"));
        op.angle = jsonFloat(get("angle", 0.0), "angle");
        op.targetDepth = jsonFloat(get("targetDepth", get("amount", 0.0)), "targetDepth");
        op.flattenStrength = jsonFloat(get("flattenStrength", 1.0), "flattenStrength");
        return op;
    }
    throw depthMapEnforcementFailure(compat::RetainedString("unknown depth operation type: " + std::string(type)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 260);
    compat::ngAssertUnreachable("Assertion failure", "source/nijigenerate/commands/depth/map.d", 262);
}
compat::Slice<ext::nodes::ExDepthOp> depthOpsFromJson(Json value) {
    if (!value.is_array()) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("operations must be an array"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 266);
    compat::Slice<ext::nodes::ExDepthOp> result;
    for (auto entry : value.sourceArrayValue()) result.append(depthOpFromJson(entry));
    return result;
}
Json depthOpsToJson(compat::Slice<ext::nodes::ExDepthOp> ops) {
    Array result;
    for (std::size_t i = 0; i < ops.size(); ++i) {
        auto object = ngDepthOpToJson(ops[i]);
        object.sourceObjectRef()["indexInList"] = Json(std::bit_cast<std::int64_t>(i));
        result.append(object);
    }
    return Json(result);
}
Json depthsToJson(compat::Slice<float> depths) {
    if (depths.isNull()) return Json(nullptr);
    Array result;
    for (float depth : depths) result.append(Json(double(depth)));
    return Json(result);
}
}
core::settings::SettingsJsonValue ngDepthOpToJson(ext::nodes::ExDepthOp op) {
    using namespace detail;
    using namespace ext::nodes;
    Object result;
    result[compat::RetainedString::fromStaticLiteral("type")] =
        Json(compat::RetainedString::fromStaticView(op.typeName()));
    switch (op.type) {
    case ExDepthOpType::AttachedPoint:
        result[compat::RetainedString::fromStaticLiteral("index")] = Json(std::bit_cast<std::int64_t>(op.index));
        result[compat::RetainedString::fromStaticLiteral("amount")] = Json(double(op.amount));
        break;
    case ExDepthOpType::Ring:
        result[compat::RetainedString::fromStaticLiteral("p0")] = vec2ToJson(op.p0);
        result[compat::RetainedString::fromStaticLiteral("p1")] = vec2ToJson(op.p1);
        result[compat::RetainedString::fromStaticLiteral("amount")] = Json(double(op.amount));
        result[compat::RetainedString::fromStaticLiteral("width")] = Json(double(op.width));
        result[compat::RetainedString::fromStaticLiteral("hardness")] = Json(double(op.hardness));
        result[compat::RetainedString::fromStaticLiteral("p0Angle")] = Json(double(op.p0Angle));
        result[compat::RetainedString::fromStaticLiteral("p1Angle")] = Json(double(op.p1Angle));
        break;
    case ExDepthOpType::Plane:
        result[compat::RetainedString::fromStaticLiteral("center")] = vec2ToJson(op.center);
        result[compat::RetainedString::fromStaticLiteral("radiusX")] = Json(double(op.radiusX));
        result[compat::RetainedString::fromStaticLiteral("radiusY")] = Json(double(op.radiusY));
        result[compat::RetainedString::fromStaticLiteral("angle")] = Json(double(op.angle));
        result[compat::RetainedString::fromStaticLiteral("targetDepth")] = Json(double(op.targetDepth));
        result[compat::RetainedString::fromStaticLiteral("flattenStrength")] = Json(double(op.flattenStrength));
        break;
    default: throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 198);
    }
    return Json(result);
}
}
