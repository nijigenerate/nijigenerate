#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <common/resource_key_map.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::core::math::path {
using nijigenerate::compat::Vec2u;

namespace detail {
template<class Operation> auto allocatePrimitiveArray(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
// Used contiguous byte reshape/index specialization from selected Mir 3.22.4.
// Apache-2.0; see port/licenses/mir-ndslice-reshape-NOTICE.txt.
class MirVisitedSlice2D {
    compat::Slice<std::uint8_t> backing_;
    std::array<std::size_t, 2> shape_;
    bool hasIterator_ = false;
public:
    MirVisitedSlice2D(compat::Slice<std::uint8_t> backing,
            std::ptrdiff_t rows, std::ptrdiff_t columns, int& error)
        : backing_(backing), shape_{static_cast<std::size_t>(rows), static_cast<std::size_t>(columns)} {
        const auto oldCount = backing_.size();
        auto newCount = shape_[0] * shape_[1];
        if (oldCount == 0) { error = 1; return; }
        for (auto& length : shape_) {
            if (length != std::size_t(-1)) continue;
            newCount = std::size_t(0) - newCount;
            length = oldCount / newCount;
            newCount *= length;
            break;
        }
        if (oldCount != newCount) { error = 2; return; }
        error = 0;
        hasIterator_ = true;
    }
    std::uint8_t& at(std::size_t row, std::size_t column, bool writing = false) const {
#ifndef NDEBUG
        if (column >= shape_[1]) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("2D slice of ubyte: bounds check failed at 2 dimension"),
            compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 788);
        if (row >= shape_[0]) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("2D slice of ubyte: bounds check failed at 1 dimension"),
            compat::RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 794);
#endif
        if (!hasIterator_) compat::ngFailNullAccess(writing);
        // MIR indexes its pointer, not the former one-dimensional D array.
        return backing_.data()[column + shape_[1] * row];
    }
};

template<class E>
E& arrayAt(compat::Slice<E> values, std::size_t index, std::size_t line) {
    if (index >= values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/path.d"), line);
    return values.data()[index];
}
struct IntegerHash {
    std::size_t operator()(std::int32_t value) const noexcept {
        return static_cast<std::size_t>(value);
    }
};
}

template<class T>
concept SkeletonView2D = requires(T image, std::ptrdiff_t row, std::ptrdiff_t column) {
    { image.size() } -> std::convertible_to<std::size_t>;
    static_cast<bool>(image(row, column));
};

template<SkeletonView2D T>
compat::Slice<Vec2u> extractPath(T skeleton, std::int32_t width, std::int32_t height) {
    if (skeleton.size() == 0) return {};
    const auto count = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(height) * static_cast<std::uint32_t>(width));
    auto visitedData = detail::allocatePrimitiveArray([&] {
        return compat::Slice<std::uint8_t>(std::vector<std::uint8_t>(static_cast<std::size_t>(count), 0));
    });
    int reshapeError = 0;
    detail::MirVisitedSlice2D visited(visitedData, height, width, reshapeError);
    compat::Slice<Vec2u> longest;
    const auto encode = [width](Vec2u point) {
        return std::bit_cast<std::int32_t>(
            point.y * static_cast<std::uint32_t>(width) + point.x);
    };
    const auto component = [&](Vec2u seed) {
        compat::Slice<Vec2u> result, queue;
        std::size_t head = 0;
        queue.append(seed);
        visited.at(seed.y, seed.x, true) = 1;
        while (head < queue.size()) {
            const auto current = detail::arrayAt(queue, head, 34);
            ++head;
            result.append(current);
            for (std::int32_t dy = -1; dy <= 1; ++dy) {
                for (std::int32_t dx = -1; dx <= 1; ++dx) {
                    if (dy == 0 && dx == 0) continue;
                    const auto row = std::bit_cast<std::int32_t>(
                        current.y + static_cast<std::uint32_t>(dy));
                    const auto column = std::bit_cast<std::int32_t>(
                        current.x + static_cast<std::uint32_t>(dx));
                    if (row < 0 || row >= height || column < 0 || column >= width) continue;
                    if (skeleton(row, column) && !visited.at(row, column)) {
                        visited.at(row, column, true) = 1;
                        queue.append({static_cast<std::uint32_t>(column), static_cast<std::uint32_t>(row)});
                    }
                }
            }
        }
        return result;
    };
    using Membership = nicxlive::core::common::ResourceKeyMap<std::int32_t, bool,
        detail::IntegerHash, std::equal_to<std::int32_t>>;
    const auto neighbors = [&](Vec2u point, Membership members) {
        compat::Slice<Vec2u> result;
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                if (dy == 0 && dx == 0) continue;
                const Vec2u next{point.x + static_cast<std::uint32_t>(dx),
                                 point.y + static_cast<std::uint32_t>(dy)};
                const auto found = members.find(encode(next));
                if (found != members.end() && found->second) result.append(next);
            }
        }
        return result;
    };
    const auto diameter = [&](Vec2u seed, compat::Slice<Vec2u> points, Membership members) {
        compat::Slice<Vec2u> queue;
        auto distances = detail::allocatePrimitiveArray([&] {
            return compat::Slice<std::int32_t>(std::vector<std::int32_t>(static_cast<std::size_t>(count)));
        });
        for (auto& distance : distances) distance = -1;
        nicxlive::core::common::ResourceKeyMap<std::int32_t, Vec2u,
            detail::IntegerHash, std::equal_to<std::int32_t>> predecessor;
        std::int32_t head = 0;
        queue.append(seed);
        detail::arrayAt(distances, static_cast<std::size_t>(encode(seed)), 78) = 0;
        while (static_cast<std::size_t>(head) < queue.size()) {
            const auto current = detail::arrayAt(queue, static_cast<std::size_t>(head), 80);
            head = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(head) + 1U);
            for (const auto next : neighbors(current, members)) {
                const auto key = encode(next);
                if (detail::arrayAt(distances, static_cast<std::size_t>(key), 84) == -1) {
                    const auto currentDistance = detail::arrayAt(distances,
                        static_cast<std::size_t>(encode(current)), 85);
                    detail::arrayAt(distances, static_cast<std::size_t>(key), 85) = std::bit_cast<std::int32_t>(
                        static_cast<std::uint32_t>(currentDistance) + 1U);
                    predecessor[key] = current;
                    queue.append(next);
                }
            }
        }
        std::int32_t maximum = 0;
        auto farthest = seed;
        for (const auto point : points) {
            const auto distance = detail::arrayAt(distances, static_cast<std::size_t>(encode(point)), 94);
            if (distance > maximum) {
                maximum = distance;
                farthest = point;
            }
            tasks::TaskFiber::yield();
        }
        compat::Slice<Vec2u> reversed;
        auto current = farthest;
        for (;;) {
            reversed.append(current);
            if (current == seed) break;
            const auto previous = predecessor.findRetained(encode(current));
            if (!previous) throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/math/path.d"), 107);
            current = *previous;
        }
        std::reverse(reversed.begin(), reversed.end());
        auto path = detail::allocatePrimitiveArray([&] {
            return compat::Slice<Vec2u>(reversed.begin(), reversed.end());
        });
        return std::pair{farthest, path};
    };
    for (std::int32_t row = 0; row < height; ++row) {
        for (std::int32_t column = 0; column < width; ++column) {
            if (!skeleton(row, column) || visited.at(row, column)) continue;
            const auto points = component({static_cast<std::uint32_t>(column), static_cast<std::uint32_t>(row)});
            Membership members;
            for (const auto point : points) {
                if (point.x >= static_cast<std::uint32_t>(width)
                    || point.y >= static_cast<std::uint32_t>(height)) continue;
                members[encode(point)] = true;
            }
            tasks::TaskFiber::yield();
            compat::Slice<Vec2u> endpoints;
            for (const auto point : points) {
                if (neighbors(point, members).size() == 1) endpoints.append(point);
                tasks::TaskFiber::yield();
            }
            const auto seed = endpoints.empty() ? detail::arrayAt(points, 0, 137)
                : detail::arrayAt(endpoints, 0, 137);
            const auto first = diameter(seed, points, members);
            auto second = diameter(first.first, points, members);
            if (second.second.size() > longest.size()) longest = second.second;
        }
    }
    return longest;
}
}
