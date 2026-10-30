#include "circlebrush.hpp"
#include "brush_internal.hpp"
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <string>
#include <utility>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <source_location>
#include <new>
#include <stdexcept>

// Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.

namespace nijigenerate::viewport::common::mesheditor::brushes::circlebrush {
void CircleBrush::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(CircleBrush),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.common.mesheditor.brushes.circlebrush.CircleBrush"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString CircleBrush::toString() {
    const auto keepAlive = shared_from_this();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t CircleBrush::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool CircleBrush::opEquals(std::shared_ptr<CircleBrush> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int CircleBrush::opCmp(std::shared_ptr<CircleBrush>) {
    const auto keepAlive = shared_from_this();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(name.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
namespace {
float circleDistance(Vec2 a, Vec2 b) {
    // inmath.length returns real, but the UFCS distance(T) function returns
    // T.vt. Retain that float return boundary before comparison or division.
#if defined(_WIN32) && defined(_MSC_VER)
    using SourceReal = double; // LDC Windows MSVC target's D real.
#else
    using SourceReal = long double;
#endif
    const float x = a.x - b.x, y = a.y - b.y;
    const float xx = x * x, yy = y * y;
    SourceReal squared = 0;
    squared += static_cast<SourceReal>(xx);
    squared += static_cast<SourceReal>(yy);
    return static_cast<float>(std::sqrt(squared));
}
}
CircleBrush::CircleBrush(compat::RetainedString name, float radius) : _name(name), radius(radius) {
    prepareSourceClassInfo();
}
compat::RetainedString CircleBrush::name() { return _name; }
bool CircleBrush::isInside(Vec2 center, Vec2 pos) { return circleDistance(center, pos) <= radius; }
float CircleBrush::weightAt(Vec2 center, Vec2 pos) {
    const float distance = 1 - std::abs(circleDistance(pos, center)) / radius;
    return std::min(1.0f, std::max(distance, 0.0f));
}
compat::Slice<float> CircleBrush::weightsAt(Vec2 center, Vec2Array positions) {
    const auto keepAlive = shared_from_this();
    compat::Slice<float> result;
    positions.forEachValue([&](std::size_t, Vec2 point) {
        result.append(weightAt(center, point));
        return 0;
    });
    return result;
}
void CircleBrush::draw(Vec2 center, Mat4 transform) {
    const auto keepAlive = shared_from_this();
    auto points = ngCreateCircleBuffer(center, Vec2{radius, radius}, 32);
    detail::cross(points, center, radius);
    detail::draw(points, transform);
}
bool CircleBrush::configure() {
    const auto keepAlive = shared_from_this();
    ImGui::BeginGroup();
    detail::control("BRUSH_RADIUS", "brush_radius", radius, 1, 2000);
    ImGui::EndGroup();
    return false;
}
}
