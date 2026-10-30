#include "rectanglebrush.hpp"
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

namespace nijigenerate::viewport::common::mesheditor::brushes::rectanglebrush {
void RectangleBrush::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(RectangleBrush),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.common.mesheditor.brushes.rectanglebrush.RectangleBrush"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString RectangleBrush::toString() {
    const auto keepAlive = shared_from_this();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t RectangleBrush::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool RectangleBrush::opEquals(std::shared_ptr<RectangleBrush> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int RectangleBrush::opCmp(std::shared_ptr<RectangleBrush>) {
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
RectangleBrush::RectangleBrush(compat::RetainedString name, float width, float height, float innerRatio) {
    _name = name;
    this->width = std::max(1.0f, width);
    this->height = std::max(1.0f, height);
    this->innerRatio = std::max(0.0f, std::min(innerRatio, 1.0f));
    prepareSourceClassInfo();
}
compat::RetainedString RectangleBrush::name() { return _name; }
bool RectangleBrush::isInside(Vec2 center, Vec2 pos) {
    const Vec2 distance{std::abs(pos.x - center.x), std::abs(pos.y - center.y)};
    return distance.x <= width / 2 && distance.y <= height;
}
float RectangleBrush::weightAt(Vec2 center, Vec2 pos) {
    const Vec2 distance{std::abs(pos.x - center.x), std::abs(pos.y - center.y)};
    const float xratio = std::min(1.0f, std::max(0.0f, 1 - distance.x / width));
    const float yratio = std::min(1.0f, std::max(0.0f, 1 - distance.y / height));
    return std::min(xratio, yratio);
}
compat::Slice<float> RectangleBrush::weightsAt(Vec2 center, Vec2Array positions) {
    const auto keepAlive = shared_from_this();
    compat::Slice<float> result;
    positions.forEachValue([&](std::size_t, Vec2 point) {
        result.append(weightAt(center, point));
        return 0;
    });
    return result;
}
void RectangleBrush::draw(Vec2 center, Mat4 transform) {
    const auto keepAlive = shared_from_this();
    auto points = ngCreateRectBuffer(Vec2{center.x - width, center.y - height},
        Vec2{center.x + width, center.y + height});
    detail::draw(points, transform);
}
bool RectangleBrush::configure() {
    const auto keepAlive = shared_from_this();
    ImGui::BeginGroup();
    detail::control("BRUSH_WIDTH", "brush_width", width, 1, 2000);
    ImGui::SameLine(0, 4);
    detail::control("BRUSH_HEIGHT", "brush_height", height, 1, 2000);
    ImGui::SameLine(0, 4);
    detail::control("BRUSH_INNER_RATIO", "brush_inner_radtio", innerRatio, 0, 1);
    ImGui::EndGroup();
    return false;
}
}
