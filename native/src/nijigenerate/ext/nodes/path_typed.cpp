#include <nodes/path_deformer.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <serde_json.hpp>
#include <serde_reader.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace nicxlive::core::nodes {
[[noreturn]] void ngThrowPathDriverAllocationError() {
    throw nijigenerate::compat::OutOfMemoryError();
}
namespace {
serde::ReadError readPendulum(ConnectedPendulumDriver& driver, serde::TypedJsonView data) {
    const auto read = [&](const char* key, auto getter, auto setter) -> serde::ReadError {
        const auto field = data.findFirst(key);
        if (field.isMissing()) return {};
        float value = (driver.*getter)();
        const auto error = serde::deserializeValue(field, value);
        (driver.*setter)(value);
        return error;
    };
    using Getter = float (ConnectedPendulumDriver::*)() const noexcept;
    using Setter = void (ConnectedPendulumDriver::*)(float) noexcept;
#define READ_SCALAR(key, method) \
    if (auto error = read(key, static_cast<Getter>(&ConnectedPendulumDriver::method), \
        static_cast<Setter>(&ConnectedPendulumDriver::method))) return error
    READ_SCALAR("damping", damping);
    READ_SCALAR("restore_constant", restoreConstant);
    READ_SCALAR("gravity", gravity);
    READ_SCALAR("input_scale", inputScale);
    READ_SCALAR("propagate_scale", propagateScale);
#undef READ_SCALAR
    return {};
}
}

void PathDeformer::serializeSelfImpl(serde::TypedJsonWriter& writer, bool recursive,
    SerializeNodeFlags flags) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Deformable::serializeSelfImpl(writer, recursive, flags);
    if (physicsOnly) {
        writer.putKey("physics_only");
        writer.putValue(physicsOnly);
    }
    writer.putKey("curve_type");
    if (curveType != CurveType::Bezier && curveType != CurveType::Spline)
        nijigenerate::compat::ngAssertUnreachable("Assertion failure", "mir/serde.d", 522);
    writer.putValue(curveType == CurveType::Bezier ? "Bezier" : "Spline");
    if (has_flag(flags, SerializeNodeFlags::Geometry)) {
        writer.putKey("vertices");
        const auto array = writer.listBegin();
        if (originalCurve) {
            const auto curveReceiver = originalCurve;
            const auto& points = curveReceiver->controlPoints();
            const auto count = points.size();
            for (std::size_t i = 0; i < count; ++i) {
                writer.elemBegin(); writer.putValue(points.xAt(i));
                writer.elemBegin(); writer.putValue(points.yAt(i));
            }
        }
        writer.listEnd(array);
    }
    if (has_flag(flags, SerializeNodeFlags::Links) && driver) {
        writer.putKey("physics");
        const auto object = writer.structBegin();
        const auto adapter = dynamic_cast<ConnectedDriverAdapter*>(driver.get());
        if (!adapter) throw serde::JsonConversionError("Path physics driver has no source serializer");
        if (const auto pendulum = dynamic_cast<ConnectedPendulumDriver*>(adapter->impl())) {
            writer.putField("type", "Pendulum");
            writer.putField("damping", pendulum->damping());
            writer.putField("restore_constant", pendulum->restoreConstant());
            writer.putField("gravity", pendulum->gravity());
            writer.putField("input_scale", pendulum->inputScale());
            writer.putField("propagate_scale", pendulum->propagateScale());
        } else if (dynamic_cast<ConnectedSpringPendulumDriver*>(adapter->impl())) {
            writer.putField("type", "SpringPendulum");
        } else throw serde::JsonConversionError("Path physics driver has no source serializer");
        writer.structEnd(object);
    }
    writer.putKey("dynamic_deformation");
    writer.putValue(dynamicDeformation);
}

serde::ReadError PathDeformer::deserializeFromFghj(serde::TypedJsonView data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    // The source deliberately discards the base read error and the vertex read errors.
    (void)Deformable::deserializeFromFghj(data);
    const auto only = data.findFirst("physics_only");
    if (!only.isMissing()) if (auto error = serde::deserializeValue(only, physicsOnly)) return error;
    const auto curve = data.findFirst("curve_type");
    if (!curve.isMissing()) {
        // Enum conversion uses deserializeScopedString, unlike an ordinary string field's DUP.
        const auto kind = curve.kind();
        const auto key = kind == serde::JsonKind::String ? curve.stringBytes() : std::string_view{};
        if (kind != serde::JsonKind::String && kind != serde::JsonKind::Null)
            (void)serde::unexpectedJsonKind(curve);
        if (key == "Bezier") curveType = CurveType::Bezier;
        else if (key == "Spline") curveType = CurveType::Spline;
        else {
            constexpr std::string_view prefix = "Unable to deserialize string '";
            constexpr std::string_view suffix = "' to CurveTypeAllowed keys:allowedKeys";
            std::string message;
            message.reserve(prefix.size() + key.size() + suffix.size());
            message.append(prefix);
            if (!key.empty()) message.append(key);
            message.append(suffix);
            throw d_runtime::ThrowableException(d_runtime::RetainedString(std::move(message)),
                d_runtime::RetainedString::fromStaticLiteral("fghj/serialization.d"), 2167);
        }
    }
    dynamicDeformation = false;
    const auto dynamic = data.findFirst("dynamic_deformation");
    if (!dynamic.isMissing()) if (auto error = serde::deserializeValue(dynamic, dynamicDeformation)) return error;
    Vec2Array points;
    auto elements = data.findFirst("vertices").byElement();
    while (!elements.empty()) {
        float x = std::numeric_limits<float>::quiet_NaN();
        float y = std::numeric_limits<float>::quiet_NaN();
        (void)serde::deserializeValue(elements.front(), x);
        elements.popFront();
        (void)serde::deserializeValue(elements.front(), y);
        elements.popFront();
        const auto index = points.size();
        points.resize(index + 1);
        points.xAt(index) = x; points.yAt(index) = y;
    }
    rebuffer(points);
    const auto physics = data.findFirst("physics");
    if (!physics.isMissing()) {
        std::string type;
        (void)serde::deserializeValue(physics.findFirst("type"), type);
        std::unique_ptr<ConnectedPhysicsDriver> next;
        if (type == "Pendulum") {
            auto pendulum = std::make_unique<ConnectedPendulumDriver>(this);
            (void)readPendulum(*pendulum, physics);
            next = std::move(pendulum);
        } else if (type == "SpringPendulum") {
            next = std::make_unique<ConnectedSpringPendulumDriver>(this);
        }
        if (next) {
            driver = std::make_unique<ConnectedDriverAdapter>(std::move(next), this);
            driverInitialized = false;
            prevRootSet = false;
            if (hasDegenerateBaseline) disablePhysicsDriver("degenerateBaseline");
            else driver->setup();
            clearCache();
        }
    }
    return {};
}
}
