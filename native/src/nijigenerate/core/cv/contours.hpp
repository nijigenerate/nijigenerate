#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/core/tasks.hpp>
#include "pixel_operations.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <math/types.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <tuple>
#include <utility>

namespace nijigenerate::core::cv::contours {

using Vec2i = nicxlive::core::math::Vec2i;
using Contour = compat::Slice<Vec2i>;
using Contours = compat::Slice<Contour>;
using Labels = compat::Slice<compat::Slice<std::int32_t>>;

enum class RetrievalMode : std::int32_t { EXTERNAL, LIST, CCOMP, TREE };
enum class ApproximationMethod : std::int32_t { NONE, SIMPLE, TC89_L1, TC89_KCOS };

struct ContourHierarchy {
    std::int32_t next{}, prev{}, child{}, parent{};
};
struct BoundingBox {
    std::int32_t minX{}, minY{}, maxX{}, maxY{};
};

} // namespace nijigenerate::core::cv::contours

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::core::cv::contours::RetrievalMode> {
    using Value = ::nijigenerate::core::cv::contours::RetrievalMode;
    inline static constexpr std::string_view name = "RetrievalMode";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::EXTERNAL, "EXTERNAL"},
        FormatEnumMember<Value>{Value::LIST, "LIST"},
        FormatEnumMember<Value>{Value::CCOMP, "CCOMP"},
        FormatEnumMember<Value>{Value::TREE, "TREE"}};
};

template<> struct FormatEnumTraits<::nijigenerate::core::cv::contours::ApproximationMethod> {
    using Value = ::nijigenerate::core::cv::contours::ApproximationMethod;
    inline static constexpr std::string_view name = "ApproximationMethod";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::NONE, "NONE"},
        FormatEnumMember<Value>{Value::SIMPLE, "SIMPLE"},
        FormatEnumMember<Value>{Value::TC89_L1, "TC89_L1"},
        FormatEnumMember<Value>{Value::TC89_KCOS, "TC89_KCOS"}};
};

template<> struct FormatAggregateTraits<::nijigenerate::core::cv::contours::ContourHierarchy> {
    using Value = ::nijigenerate::core::cv::contours::ContourHierarchy;
    inline static constexpr std::string_view typeName = "ContourHierarchy";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("next", offsetof(Value, next), &Value::next),
            ngFormatAggregateMember("prev", offsetof(Value, prev), &Value::prev),
            ngFormatAggregateMember("child", offsetof(Value, child), &Value::child),
            ngFormatAggregateMember("parent", offsetof(Value, parent), &Value::parent)};
    }
};

template<> struct FormatAggregateTraits<::nijigenerate::core::cv::contours::BoundingBox> {
    using Value = ::nijigenerate::core::cv::contours::BoundingBox;
    inline static constexpr std::string_view typeName = "BoundingBox";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("minX", offsetof(Value, minX), &Value::minX),
            ngFormatAggregateMember("minY", offsetof(Value, minY), &Value::minY),
            ngFormatAggregateMember("maxX", offsetof(Value, maxX), &Value::maxX),
            ngFormatAggregateMember("maxY", offsetof(Value, maxY), &Value::maxY)};
    }
};

}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::cv::contours::RetrievalMode> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.cv.contours.RetrievalMode)";
    inline static constexpr char unqualifiedArgument[] = "(RetrievalMode)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.cv.contours.RetrievalMode";
    inline static constexpr char unqualifiedElement[] = "RetrievalMode";
};
template<> struct SourceTypeSpellings<::nijigenerate::core::cv::contours::ApproximationMethod> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.cv.contours.ApproximationMethod)";
    inline static constexpr char unqualifiedArgument[] = "(ApproximationMethod)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.cv.contours.ApproximationMethod";
    inline static constexpr char unqualifiedElement[] = "ApproximationMethod";
};
template<> struct SourceTypeSpellings<::nijigenerate::core::cv::contours::ContourHierarchy> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.cv.contours.ContourHierarchy)";
    inline static constexpr char unqualifiedArgument[] = "(ContourHierarchy)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.cv.contours.ContourHierarchy";
    inline static constexpr char unqualifiedElement[] = "ContourHierarchy";
};
template<> struct SourceTypeSpellings<::nijigenerate::core::cv::contours::BoundingBox> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.cv.contours.BoundingBox)";
    inline static constexpr char unqualifiedArgument[] = "(BoundingBox)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.cv.contours.BoundingBox";
    inline static constexpr char unqualifiedElement[] = "BoundingBox";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::cv::contours {

inline constexpr bool inBounds(std::int32_t x, std::int32_t y,
                               std::int32_t width, std::int32_t height) {
    return x >= 0 && x < width && y >= 0 && y < height;
}
inline constexpr std::array<std::array<std::int32_t, 2>, 8> DIRECTIONS{{
    {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}}};
inline constexpr std::array<std::array<std::int32_t, 3>, 3> DIR_MAP{{
    {3, 4, 5}, {2, -1, 6}, {1, 0, 7}}};

namespace detail {
template<class Shape> struct SourceShapeArray : std::false_type {};
template<class T, std::size_t Size>
struct SourceShapeArray<std::array<T, Size>> : std::true_type {};
template<class T> struct SourceShapeArray<compat::Slice<T>> : std::true_type {};
template<class T> struct SourceShapeArray<compat::ConstSlice<T>> : std::true_type {};

template<class Shape> decltype(auto) sourceShapeAt(Shape&& shape,
        std::size_t index, std::size_t line) {
    if constexpr (SourceShapeArray<std::remove_cvref_t<Shape>>::value) {
        if (index >= shape.size())
            throw compat::ngStaticError<compat::ArrayIndexError>(index, shape.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/cv/contours.d"), line);
    }
    // A defining non-array shape keeps its own authored indexing operation.
    return std::forward<Shape>(shape)[index];
}

template<class T> T& sourceAt(compat::Slice<T>& values, std::size_t index, std::size_t line) {
    if (index >= values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/cv/contours.d"), line);
    return values.data()[index];
}
template<class T> const T& sourceAt(const compat::Slice<T>& values,
        std::size_t index, std::size_t line) {
    if (index >= values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/cv/contours.d"), line);
    return values.data()[index];
}
inline std::int32_t& labelAt(Labels& labels, std::size_t row, std::size_t column,
        std::size_t line) {
    return sourceAt(sourceAt(labels, row, line), column, line);
}
template<class T> compat::Slice<T> sourceSlice(compat::Slice<T> values,
        std::size_t first, std::size_t last, std::size_t line) {
    if (first > last || last > values.size())
        throw compat::ngStaticError<compat::ArraySliceError>(first, last, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/cv/contours.d"), line);
    return values.slice(first, last);
}
std::int32_t add(std::int32_t left, std::int32_t right);
std::int32_t subtract(std::int32_t left, std::int32_t right);
std::int32_t multiply(std::int32_t left, std::int32_t right);
Contour reserveContour(std::size_t capacity);
}

template<class T>
Contour suzukiAbeContour(const T image, Labels& labels, std::int32_t label,
    std::int32_t startX, std::int32_t startY, std::int32_t width, std::int32_t height) {
    const Vec2i beginning{startX, startY};
    auto current = beginning;
    Vec2i previous{startX, detail::subtract(startY, 1)};
    Contour contour = detail::reserveContour(256);
    contour.append(beginning);
    detail::labelAt(labels, static_cast<std::size_t>(startY), static_cast<std::size_t>(startX), 92) = label;
    bool firstIteration = true;
    while (true) {
        const auto dx = detail::subtract(previous.x, current.x);
        const auto dy = detail::subtract(previous.y, current.y);
        const auto table = dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1
            ? DIR_MAP[dy + 1][dx + 1] : -1;
        const auto start = table != -1 ? (table + 1) % 8 : 0;
        bool found = false;
        Vec2i next;
        auto nextDirection = start;
        for (std::size_t search = 0; search < 8; ++search) {
            const auto direction = (start + search) % 8;
            const auto x = detail::add(current.x, DIRECTIONS[direction][0]);
            const auto y = detail::add(current.y, DIRECTIONS[direction][1]);
            if (!inBounds(x, y, width, height)) continue;
            if (ContourPixelOperations<std::remove_cvref_t<decltype(image(y, x))>>::nonzero(
                    image(y, x)) && (detail::labelAt(labels, y, x, 119) == 0 ||
                    detail::labelAt(labels, y, x, 119) == label
                || (x == beginning.x && y == beginning.y && !firstIteration))) {
                next = {x, y};
                nextDirection = static_cast<std::int32_t>(direction);
                found = true;
                break;
            }
        }
        if (!found) break;
        previous = {detail::add(current.x, DIRECTIONS[(nextDirection + 7) % 8][0]),
                    detail::add(current.y, DIRECTIONS[(nextDirection + 7) % 8][1])};
        current = next;
        if (!(current == beginning)) detail::labelAt(labels, current.y, current.x, 139) = label;
        contour.append(current);
        if (!firstIteration && current == beginning) break;
        firstIteration = false;
    }
    return contour;
}
void fillContourInterior(Labels& labels, Contour contour, std::int32_t width, std::int32_t height);
bool pointInPolygon(Vec2i point, Contour polygon);
void buildHierarchy(Contours contours, compat::Slice<ContourHierarchy>& hierarchy);
Contour ramerDouglasPeucker(Contour points, double epsilon);
double distanceToSegment(Vec2i point, Vec2i first, Vec2i second);
Contour approximateContour(Contour contour, ApproximationMethod method);
template<class T>
void findContours(const T image, Contours& contours, compat::Slice<ContourHierarchy>& hierarchy,
    RetrievalMode mode = RetrievalMode::LIST,
    ApproximationMethod method = ApproximationMethod::SIMPLE) {
    // D out parameters are reset before the first shape access.
    contours = {};
    hierarchy = {};
    const auto height = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(
        detail::sourceShapeAt(image.shape(), 0, 351)));
    const auto width = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(
        detail::sourceShapeAt(image.shape(), 1, 352)));
    Labels labels;
    labels.resize(static_cast<std::size_t>(height));
    for (auto& row : labels) row.resize(static_cast<std::size_t>(width));
    std::int32_t label = 1;
    Contours localContours;
    compat::Slice<ContourHierarchy> hierarchyList;
    for (std::int32_t y = 0; y < height; y = detail::add(y, 1)) {
        for (std::int32_t x = 0; x < width; x = detail::add(x, 1)) {
            if (ContourPixelOperations<std::remove_cvref_t<decltype(image(y, x))>>::nonzero(
                    image(y, x)) && detail::labelAt(labels, y, x, 367) == 0) {
                const std::int32_t left = detail::subtract(x, 1) >= 0
                    ? ContourPixelOperations<std::remove_cvref_t<decltype(
                        image(y, detail::subtract(x, 1)))>>::assignInt(
                            image(y, detail::subtract(x, 1))) : 0;
                if (left == 0) {
                    auto contour = suzukiAbeContour(image, labels, label, x, y, width, height);
                    fillContourInterior(labels, contour, width, height);
                    contour = approximateContour(contour, method);
                    localContours.append(contour);
                    label = detail::add(label, 1);
                } else detail::labelAt(labels, y, x, 377) = -1;
            }
            tasks::TaskFiber::yield();
        }
    }
    contours = localContours;
    if (mode == RetrievalMode::TREE || mode == RetrievalMode::CCOMP)
        buildHierarchy(contours, hierarchyList);
    else {
        hierarchyList.resize(contours.size());
        for (auto& entry : hierarchyList) entry = {-1, -1, -1, -1};
    }
    if (mode == RetrievalMode::EXTERNAL) {
        Contours external;
        compat::Slice<ContourHierarchy> externalHierarchy;
        for (std::size_t index = 0; index < contours.size(); ++index) {
            if (hierarchyList[index].parent == -1) {
                external.append(contours[index]);
                externalHierarchy.append(hierarchyList[index]);
            }
        }
        contours = external;
        hierarchyList = externalHierarchy;
    }
    hierarchy = hierarchyList;
}

}
