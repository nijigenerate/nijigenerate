#include <param/parameter.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <algorithm>
#include <cstddef>

namespace nicxlive::core::param {
namespace {
constexpr char parameterSource[] = "nijilive/core/param/package.d";
template<class Condition, std::size_t N>
void checkAxis(Condition&& condition, const char (&message)[N], std::size_t line) {
#ifndef NDEBUG
    if (!condition()) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::AssertError>(
        nijigenerate::compat::RetainedString::fromStaticLiteral(message),
        nijigenerate::compat::RetainedString::fromStaticLiteral(parameterSource), line);
#else
    (void)condition; (void)message; (void)line;
#endif
}
auto& axisAt(Parameter& parameter, std::uint32_t axis, std::size_t line) {
    if (axis >= parameter.axisPoints.size())
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::ArrayIndexError>(
            axis, parameter.axisPoints.size(),
            nijigenerate::compat::RetainedString::fromStaticLiteral(parameterSource), line);
    return parameter.axisPoints[axis];
}
[[noreturn]] void removeBoundsFailure(std::size_t index, std::size_t length) {
#ifndef NDEBUG
    (void)index; (void)length;
    throw nijigenerate::compat::ngStaticError<nijigenerate::compat::AssertError>(
        nijigenerate::compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
        nijigenerate::compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#else
    // removeStable first drops index items, then drops one more from that slice.
    const bool firstDrop = index > length;
    throw nijigenerate::compat::ngStaticError<nijigenerate::compat::ArraySliceError>(
        firstDrop ? index : 1, firstDrop ? length : 0, firstDrop ? length : 0,
        nijigenerate::compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
#endif
}
}

void Parameter::insertAxisPoint(std::uint32_t axis, float offset) {
    const auto retained = weak_from_this().lock();
    checkAxis([&] { return offset > 0 && offset < 1; }, "offset out of bounds", 495);
    checkAxis([&] { return isVec2 ? axis <= 1 : axis == 0; }, "bad axis", isVec2 ? 497 : 499);
    auto& points = axisAt(*this, axis, 503);
    std::uint32_t index = 1;
    for (; index < points.size(); ++index) if (points[index] > offset) break;
    if (index > points.size()) {
        // std.array grows the descriptor before validating its first moved slice.
        const auto oldLength = points.size();
        points.resize(oldLength + 1);
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::ArraySliceError>(
            index, oldLength, points.size(),
            nijigenerate::compat::RetainedString::fromStaticLiteral("std/array.d"), 1463);
    }
    points.insertValue(index, offset);
    const auto currentBindings = bindings;
    for (const auto binding : currentBindings)
        nijigenerate::compat::ngRequireObject(binding)->insertKeypoints(axis, index);
}
void Parameter::deleteAxisPoint(std::uint32_t axis, std::uint32_t index) {
    const auto retained = weak_from_this().lock();
    checkAxis([&] { return isVec2 ? axis <= 1 : axis == 0; }, "bad axis", isVec2 ? 522 : 524);
    checkAxis([&] { return index > 0; }, "cannot delete axis point at 0", 526);
    checkAxis([&] { return index < axisAt(*this, axis, 527).size() - 1; },
        "cannot delete axis point at 1", 527);
    auto& points = axisAt(*this, axis, 530);
    if (index >= points.size()) removeBoundsFailure(index, points.size());
    auto shortened = points;
    shortened.erase(shortened.begin() + index);
    points = shortened;
    const auto currentBindings = bindings;
    for (const auto binding : currentBindings)
        nijigenerate::compat::ngRequireObject(binding)->deleteKeypoints(axis, index);
}
void Parameter::reverseAxis(std::uint32_t axis) {
    const auto retained = weak_from_this().lock();
    auto points = axisAt(*this, axis, 542);
    if (!points.empty()) std::reverse(points.begin(), points.end());
    auto values = axisAt(*this, axis, 543);
    for (auto& value : values) value = 1 - value;
    const auto currentBindings = bindings;
    for (const auto binding : currentBindings)
        nijigenerate::compat::ngRequireObject(binding)->reverseAxis(axis);
}
}
