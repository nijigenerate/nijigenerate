#include "optimum_detail.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include <common/resource_key_map.hpp>
#if defined(NG_DEBUG_AUTOMESH_OPT)
#include "optimum_diagnostics.hpp"
#endif
#include <cmath>
#include <functional>
#include <limits>
#include <type_traits>
#include <vector>

namespace nijigenerate::viewport::vertex::automesh::optimum::detail {
namespace {
float sign(float value) { return value > 0 ? 1.0f : value == 0 ? 0.0f : -1.0f; }
Vec2 difference(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
struct ContourIndexHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        if constexpr (sizeof(std::size_t) < sizeof(value))
            return static_cast<std::size_t>(value ^ (value >> 32));
        return static_cast<std::size_t>(value);
    }
};
using IndexFlags = nicxlive::core::common::ResourceKeyMap<std::uint64_t, bool,
    ContourIndexHash, std::equal_to<std::uint64_t>>;

void recordIndexFlag(IndexFlags& flags, std::uint64_t index,
    std::source_location source = std::source_location::current()) {
    try {
        flags[index] = true;
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}
}

Points dense(Vec2Array points) { return Points(points.toArray()); }
Vec2Array fromDense(Points points) {
    Vec2Array result(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) result.set(i, points[i]);
    return result;
}
Vec2Array adjacentUnique(Vec2Array points) {
    auto source = dense(points);
    auto range = compat::phobos::ngUniq(compat::phobos::detail::Equal{}, source);
    const auto unique = compat::phobos::ngArray(std::move(range));
    return fromDense(unique);
}
SourceReal lengthSquared(Vec2 value) {
    SourceReal result = 0;
    const float xx = value.x * value.x;
    result += static_cast<SourceReal>(xx);
    const float yy = value.y * value.y;
    result += static_cast<SourceReal>(yy);
    return result;
}
template<class ContourType> Vec2Array contoursToVec2s(ContourType contours) {
    Vec2Array result;
    IndexFlags visited;
    std::uint64_t next = 0;
    const auto coordinateDifference = [](auto a, auto b) {
        using Value = decltype(a - b);
        if constexpr (std::is_integral_v<decltype(a)> && std::is_integral_v<decltype(b)>
                && std::is_integral_v<Value> && std::is_signed_v<Value>) {
            using Bits = std::make_unsigned_t<Value>;
            return std::bit_cast<Value>(static_cast<Bits>(a) - static_cast<Bits>(b));
        } else {
            return a - b;
        }
    };
    const auto findNearest = [&]<class C>(C& contour) {
        std::uint64_t nearest = std::numeric_limits<std::uint64_t>::max();
        float minDistance = std::numeric_limits<float>::infinity();
        for (std::size_t i = 0; i < contours.size(); ++i) {
            if (visited.contains(i) || contours[i].empty()) continue;
            // Preserve both source coordinate expressions and their repeated indexed reads.
            const auto currentY = contour[contour.size() - 1].y;
            const auto candidateY = contours[i][0].y;
            const float dy = static_cast<float>(coordinateDifference(currentY, candidateY));
            const auto currentX = contour[contour.size() - 1].x;
            const auto candidateX = contours[i][0].x;
            const float dx = static_cast<float>(coordinateDifference(currentX, candidateX));
            const Vec2 difference{dy, dx};
            const float distance = static_cast<float>(std::sqrt(lengthSquared(difference)));
            if (distance < minDistance) { nearest = i; minDistance = distance; }
        }
        return nearest;
    };
#if defined(NG_DEBUG_AUTOMESH_OPT)
    for (std::size_t i = 0; i < contours.size(); ++i)
        diagnostics::println("contour: %d, %d", i, contours[i].size());
#endif
    while (next != std::numeric_limits<std::uint64_t>::max() && visited.size() < contours.size()) {
        recordIndexFlag(visited, next);
        auto contour = contours[next];
#if defined(NG_DEBUG_AUTOMESH_OPT)
        diagnostics::println("shape: %d", contour.size());
#endif
        for (const auto point : contour) result.push_back({static_cast<float>(point.x), static_cast<float>(point.y)});
#if defined(NG_DEBUG_AUTOMESH_OPT)
        diagnostics::print(" findNearest: %d(%s)", next, visited.at(next));
#endif
        next = findNearest(contour);
#if defined(NG_DEBUG_AUTOMESH_OPT)
        diagnostics::println("->%d(%s)", next, visited.contains(next) ? visited.at(next) : false);
#endif
    }
    return result;
}
// Instantiate the actual CV-backed private call from this original.
template Vec2Array contoursToVec2s<Contours>(Contours contours);
Vec2 calcMoment(Vec2Array contour) {
    const auto points = contour.toArray();
    if (points.empty()) {
        try {
            throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral(
                "Cannot reduce an empty input range w/o an explicit seed value."),
                compat::RetainedString::fromStaticLiteral("std/algorithm/iteration.d"), 4378);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
    auto moment = points.front();
    for (std::size_t i = 1; i < points.size(); ++i) moment = {moment.x + points[i].x, moment.y + points[i].y};
    const float count = static_cast<float>(contour.size());
    return {moment.x / count, moment.y / count};
}
Points scaling(Vec2Array contour, Vec2 moment, float scale, std::int32_t) {
    const auto points = contour.toArray();
    Points result;
    result.resize(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto point = points[i];
        const auto delta = difference(point, moment);
        const Vec2 scaled{delta.x * scale, delta.y * scale};
        result[i] = Vec2{scaled.x + moment.x, scaled.y + moment.y};
    }
    return result;
}
Vec2Array horizontalMirrored(Vec2Array sampled, float axis) {
    float side = 0;
    Vec2Array mirrored;
    IndexFlags used;
    const auto points = sampled.toArray();
    for (const auto point : points) {
        if (side == 0) {
            side = sign(point.x - axis);
            mirrored.push_back(point);
        } else if (sign(point.x - axis) != side) {
            const Vec2 flipped{axis * 2 - point.x, point.y};
            std::size_t nearest = 0;
            for (std::size_t i = 1; i < points.size(); ++i) {
                if (lengthSquared(difference(points[i], flipped)) < lengthSquared(difference(points[nearest], flipped)))
                    nearest = i;
            }
            if (!used.contains(nearest)) {
                mirrored.push_back({axis * 2 - points[nearest].x, points[nearest].y});
                recordIndexFlag(used, nearest);
            }
        } else mirrored.push_back(point);
    }
    return adjacentUnique(mirrored);
}
Vec2Array resampling(Vec2Array contour, double rate, bool, float, bool, float) {
    Vec2Array sampled;
    sampled.push_back(contour.at(0));
    for (std::size_t i = 1; i < contour.size(); ++i) {
        const auto previous = sampled.at(sampled.size() - 1);
        const auto current = contour.at(i % contour.size());
        if (lengthSquared(difference(current, previous)) > rate * rate) sampled.push_back(current);
    }
    return sampled;
}
Vec2 calculateNormalVector(Vec2 p1, Vec2 p2) {
    const float vx = p2.y - p1.y;
    const float vy = p1.x - p2.x;
    const float norm = std::sqrt(vx * vx + vy * vy);
    return norm == 0 ? Vec2{0, 0} : Vec2{vx / norm, vy / norm};
}
Vec2Array sampleExpandedFromThinned(Vec2Array points, float distance) {
    const auto count = points.size();
    if (count < 3) return points;
    Vec2Array expanded;
    for (std::size_t i = 0; i < count; ++i) {
        const auto previous = points.at((i + count - 1) % count);
        const auto next = points.at((i + 1) % count);
        const auto normal = calculateNormalVector(previous, next);
        expanded.push_back({points.at(i).x + normal.x * distance, points.at(i).y + normal.y * distance});
    }
    return adjacentUnique(expanded);
}
Vec2Array sampleContractedFromThinned(Vec2Array points, float distance, float factor) {
    const auto count = points.size();
    if (count < 3) return points;
    Vec2Array contracted;
    for (std::size_t i = 0; i < count; ++i) {
        const auto previous = points.at((i + count - 1) % count);
        const auto next = points.at((i + 1) % count);
        const auto normal = calculateNormalVector(previous, next);
        contracted.push_back({points.at(i).x - normal.x * (distance * factor),
            points.at(i).y - normal.y * (distance * factor)});
    }
    return adjacentUnique(contracted);
}
Vec2Array sampleCentroidContractedContour(Vec2Array points, float scale, float) {
    const auto source = points.toArray();
    Vec2 total{0, 0};
    for (const auto point : source) total = {total.x + point.x, total.y + point.y};
    const float count = static_cast<float>(points.size());
    const Vec2 center{total.x / count, total.y / count};
    Vec2Array result;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto point = points.at(i);
        result.push_back({center.x + scale * (point.x - center.x), center.y + scale * (point.y - center.y)});
    }
    return adjacentUnique(result);
}
Vec2Array sampledAtRate(Vec2Array points, float rate) {
    Vec2Array result;
    float flag = 0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto point = points.at(i);
        if (flag <= 0) {
            result.push_back(point);
            flag = static_cast<float>(static_cast<double>(flag) + 1.0);
        }
        flag -= rate;
    }
    return result;
}

// Float-array specialization of Phobos std.algorithm.iteration.sumPairwise.
// Copyright Andrei Alexandrescu 2008-. BSL-1.0, phobos-optimum-sum.txt.
double sumWidths(compat::Slice<float> data) {
    if (data.empty()) return 0;
    std::array<double, 64> store;
    std::size_t index = 0;
    const auto collapse = [&](std::size_t k) {
        const auto lastToKeep = index - static_cast<std::uint32_t>(std::countr_zero(k + 1));
        while (index > lastToKeep) { store[index - 1] += store[index]; --index; }
    };
    const auto blocks = data.size() / 16;
    for (std::size_t k = 0; k < blocks; ++k) {
        store[index] = (((static_cast<double>(data[0]) + data[1]) + (static_cast<double>(data[2]) + data[3]))
            + ((static_cast<double>(data[4]) + data[5]) + (static_cast<double>(data[6]) + data[7])))
            + (((static_cast<double>(data[8]) + data[9]) + (static_cast<double>(data[10]) + data[11]))
            + ((static_cast<double>(data[12]) + data[13]) + (static_cast<double>(data[14]) + data[15])));
        data = data.slice(16, data.size());
        collapse(k);
        ++index;
    }
    std::size_t i = 0;
    for (const float value : data) { store[index] = value; collapse(i); ++index; ++i; }
    double sum = store[index - 1];
    for (std::size_t j = index - 1; j-- > 0;) sum += store[j];
    return 0.0 + sum;
}

} // namespace nijigenerate::viewport::vertex::automesh::optimum::detail
