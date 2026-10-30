#include "contours.hpp"
#include "detail/contours_sort.hpp"
#include <nijigenerate/compat/numeric.hpp>
#include <bit>
#include <algorithm>
#include <cmath>
#include <vector>
#include <source_location>

namespace nijigenerate::core::cv::contours {
namespace detail {
Contour reserveContour(std::size_t capacity) {
    try {
        std::vector<Vec2i> backing;
        backing.reserve(capacity);
        return Contour(std::move(backing));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
std::int32_t subtract(std::int32_t left, std::int32_t right) {
    return std::bit_cast<std::int32_t>(std::uint32_t(left) - std::uint32_t(right));
}
std::int32_t add(std::int32_t left, std::int32_t right) {
    return std::bit_cast<std::int32_t>(std::uint32_t(left) + std::uint32_t(right));
}
std::int32_t multiply(std::int32_t left, std::int32_t right) {
    return std::bit_cast<std::int32_t>(std::uint32_t(left) * std::uint32_t(right));
}
}
using detail::add;
using detail::subtract;
using detail::multiply;

double distanceToSegment(Vec2i point, Vec2i first, Vec2i second) {
    const double a = subtract(point.x, first.x);
    const double b = subtract(point.y, first.y);
    const double c = subtract(second.x, first.x);
    const double d = subtract(second.y, first.y);
    const double dot = a * c + b * d;
    const double lengthSquared = c * c + d * d;
    const double parameter = lengthSquared != 0 ? dot / lengthSquared : -1;
    double x, y;
    if (parameter < 0) {
        x = first.x;
        y = first.y;
    } else if (parameter > 1) {
        x = second.x;
        y = second.y;
    } else {
        x = first.x + parameter * c;
        y = first.y + parameter * d;
    }
    return std::sqrt((point.x - x) * (point.x - x) + (point.y - y) * (point.y - y));
}

bool pointInPolygon(Vec2i point, Contour polygon) {
    std::int32_t crossings = 0;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const auto first = polygon[index];
        const auto second = polygon[(index + 1) % polygon.size()];
        if ((first.y > point.y) != (second.y > point.y)) {
            const double x = first.x + (static_cast<double>(subtract(point.y, first.y))
                / subtract(second.y, first.y)) * subtract(second.x, first.x);
            if (point.x < x) crossings = add(crossings, 1);
        }
    }
    return (crossings & 1) == 1;
}

Contour ramerDouglasPeucker(Contour points, double epsilon) {
    if (points.size() < 3) return points.duplicate();
    double maximumDistance = 0;
    std::int32_t index = 0;
    for (std::size_t candidate = 1; candidate < points.size() - 1; ++candidate) {
        const auto distance = distanceToSegment(detail::sourceAt(points, candidate, 279),
            detail::sourceAt(points, 0, 279), detail::sourceAt(points, points.size() - 1, 279));
        if (distance > maximumDistance) {
            maximumDistance = distance;
            index = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(candidate));
        }
    }
    if (maximumDistance > epsilon) {
        const auto left = ramerDouglasPeucker(detail::sourceSlice(points, 0,
            static_cast<std::size_t>(add(index, 1)), 286), epsilon);
        const auto right = ramerDouglasPeucker(detail::sourceSlice(points,
            static_cast<std::size_t>(index), points.size(), 287), epsilon);
        const auto suffix = detail::sourceSlice(right, 1, right.size(), 288);
        Contour result;
        result.resize(left.size() + suffix.size());
        for (std::size_t i = 0; i < left.size(); ++i) result.data()[i] = left.data()[i];
        for (std::size_t i = 0; i < suffix.size(); ++i) result.data()[left.size() + i] = suffix.data()[i];
        return result;
    }
    return {points[0], points[points.size() - 1]};
}

void buildHierarchy(Contours contours, compat::Slice<ContourHierarchy>& hierarchy) {
    hierarchy = {};
    hierarchy.resize(contours.size());
    for (auto& entry : hierarchy) entry = {-1, -1, -1, -1};
    compat::Slice<BoundingBox> boxes;
    boxes.resize(contours.size());
    for (std::size_t index = 0; index < contours.size(); ++index) {
        const auto contour = contours[index];
        auto box = BoundingBox{detail::sourceAt(contour, 0, 228).x,
            detail::sourceAt(contour, 0, 229).y, detail::sourceAt(contour, 0, 228).x,
            detail::sourceAt(contour, 0, 229).y};
        for (const auto point : contour) {
            box.minX = std::min(box.minX, point.x);
            box.maxX = std::max(box.maxX, point.x);
            box.minY = std::min(box.minY, point.y);
            box.maxY = std::max(box.maxY, point.y);
        }
        boxes[index] = box;
    }
    for (std::size_t index = 0; index < contours.size(); ++index) {
        const auto representative = contours[index][0];
        std::int32_t parent = -1;
        for (std::size_t candidate = 0; candidate < contours.size(); ++candidate) {
            if (index == candidate) continue;
            const auto box = boxes[candidate];
            if (representative.x < box.minX || representative.x > box.maxX
                || representative.y < box.minY || representative.y > box.maxY) continue;
            if (pointInPolygon(representative, contours[candidate])) {
                if (parent == -1 || contours[candidate].size() < contours[parent].size())
                    parent = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(candidate));
            }
        }
        hierarchy[index].parent = parent;
        if (parent != -1) {
            const auto child = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(index));
            if (hierarchy[parent].child == -1) hierarchy[parent].child = child;
            else {
                auto sibling = hierarchy[parent].child;
                while (hierarchy[sibling].next != -1) sibling = hierarchy[sibling].next;
                hierarchy[sibling].next = child;
                hierarchy[index].prev = sibling;
            }
        }
    }
}

Contour approximateContour(Contour contour, ApproximationMethod method) {
    if (method == ApproximationMethod::NONE) return contour;
    if (method == ApproximationMethod::SIMPLE) {
        if (contour.size() < 3) return contour.duplicate();
        Contour result = detail::reserveContour(contour.size());
        result.append(contour[0]);
        for (std::size_t index = 1; index < contour.size() - 1; ++index) {
            const auto previous = contour[index - 1];
            const auto current = contour[index];
            const auto next = contour[index + 1];
            if (multiply(subtract(current.x, previous.x), subtract(next.y, current.y))
                != multiply(subtract(current.y, previous.y), subtract(next.x, current.x)))
                result.append(current);
        }
        result.append(contour[contour.size() - 1]);
        return result;
    }
    if (method == ApproximationMethod::TC89_L1 || method == ApproximationMethod::TC89_KCOS)
        return ramerDouglasPeucker(contour, 2.0);
    return contour;
}

void fillContourInterior(Labels& labels, Contour contour, std::int32_t width, std::int32_t height) {
    auto minimumX = detail::sourceAt(contour, 0, 157).x;
    auto maximumX = detail::sourceAt(contour, 0, 157).x;
    auto minimumY = detail::sourceAt(contour, 0, 158).y;
    auto maximumY = detail::sourceAt(contour, 0, 158).y;
    for (const auto point : contour) {
        minimumX = std::min(minimumX, point.x);
        maximumX = std::max(maximumX, point.x);
        minimumY = std::min(minimumY, point.y);
        maximumY = std::max(maximumY, point.y);
    }
    for (auto y = minimumY; y <= maximumY; y = add(y, 1)) {
        compat::Slice<double> intersections;
        for (std::size_t index = 0; index < contour.size(); ++index) {
            const auto first = contour[index];
            const auto second = contour[(index + 1) % contour.size()];
            if ((first.y <= y && second.y > y) || (first.y > y && second.y <= y)) {
                const double x = first.x + (static_cast<double>(subtract(y, first.y))
                    / subtract(second.y, first.y)) * subtract(second.x, first.x);
                intersections.append(x);
            }
        }
        if (intersections.size() >= 2) {
            detail::intersection_sort::sort({intersections.data(), intersections.size()});
            for (std::size_t index = 0; index < intersections.size() - 1; index += 2) {
                const auto first = compat::ngFloatToInt32(std::ceil(intersections[index]));
                const auto last = compat::ngFloatToInt32(std::floor(intersections[index + 1]));
                for (auto x = first; x <= last; x = add(x, 1)) {
                    if (inBounds(x, y, width, height) && detail::labelAt(labels, y, x, 188) == 0)
                        detail::labelAt(labels, y, x, 189) = -1;
                }
            }
        }
    }
}
}
