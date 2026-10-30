#include "doublethreshbrush.hpp"
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

namespace nijigenerate::viewport::common::mesheditor::brushes::doublethreshbrush {
void DoubleThreshBrush::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DoubleThreshBrush),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.common.mesheditor.brushes.doublethreshbrush.DoubleThreshBrush"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString DoubleThreshBrush::toString() {
    const auto keepAlive = shared_from_this();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DoubleThreshBrush::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DoubleThreshBrush::opEquals(std::shared_ptr<DoubleThreshBrush> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DoubleThreshBrush::opCmp(std::shared_ptr<DoubleThreshBrush>) {
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
float brushDistance(Vec2 a, Vec2 b) {
    // The imported inmath distance(T) returns T.vt after real-valued length.
#if defined(_WIN32) && defined(_MSC_VER)
    using SourceReal = double;
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
DoubleThreshBrush::DoubleThreshBrush(compat::RetainedString name, float radius, float innerRadius) {
    _name = name;
    this->radius = std::max(1.0f, radius);
    this->innerRadius = std::min(innerRadius, radius);
    prepareSourceClassInfo();
}
compat::RetainedString DoubleThreshBrush::name() { return _name; }
bool DoubleThreshBrush::isInside(Vec2 center, Vec2 pos) { return brushDistance(center, pos) <= radius; }
float DoubleThreshBrush::weightAt(Vec2 center, Vec2 pos) {
    const float distance = std::abs(brushDistance(pos, center));
    if (distance <= innerRadius) return 1;
    if (radius > innerRadius)
        return std::min(1.0f, std::max(1 - (distance - innerRadius) / (radius - innerRadius), 0.0f));
    return 0;
}
compat::Slice<float> DoubleThreshBrush::weightsAt(Vec2 center, Vec2Array positions) {
    const auto keepAlive = shared_from_this();
    compat::Slice<float> result;
    positions.forEachValue([&](std::size_t, Vec2 point) {
        result.append(weightAt(center, point));
        return 0;
    });
    return result;
}
void DoubleThreshBrush::draw(Vec2 center, Mat4 transform) {
    const auto keepAlive = shared_from_this();
    const auto outer = ngCreateCircleBuffer(center, Vec2{radius, radius}, 32);
    const auto inner = ngCreateCircleBuffer(center, Vec2{innerRadius, innerRadius}, 32);
    auto points = outer.dup();
    points.append(inner);
    detail::cross(points, center, radius);
    detail::draw(points, transform);
}
bool DoubleThreshBrush::configure() {
    const auto keepAlive = shared_from_this();
    ImGui::BeginGroup();
    detail::control("BRUSH_RADIUS", "brush_radius", radius, 1, 2000);
    ImGui::SameLine(0, 4);
    detail::control("BRUSH_INNER_RADIUS", "brush_inner_radius", innerRadius, 1, 2000);
    ImGui::EndGroup();
    return false;
}
}
