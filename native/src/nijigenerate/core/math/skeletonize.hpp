#pragma once

#include <concepts>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <algorithm>

namespace nijigenerate::core::math::skeletonize {

template<class T>
concept MutableSkeletonView2D = requires(T image, std::ptrdiff_t row, std::ptrdiff_t column) {
    { image.shape()[0] } -> std::convertible_to<std::uint64_t>;
    { image.shape()[1] } -> std::convertible_to<std::uint64_t>;
    static_cast<bool>(image(row, column));
    static_cast<std::uint8_t>(image(row, column));
    image(row, column) = 0;
    requires (std::same_as<std::remove_cvref_t<decltype(image(row, column))>, std::uint8_t>
              || std::same_as<std::remove_cvref_t<decltype(image(row, column))>, bool>);
};

namespace detail {
inline std::int32_t addIndex(std::int32_t left, std::int32_t right) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(left) +
        static_cast<std::uint32_t>(right));
}

template<class T>
int countNeighbors(T image, std::int32_t row, std::int32_t column) {
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dy == 0 && dx == 0) continue;
            if (image(addIndex(row, dy), addIndex(column, dx))) ++count;
        }
    }
    return count;
}

template<class T>
int countTransitions(T image, std::int32_t row, std::int32_t column) {
    const auto above = addIndex(row, -1);
    const auto below = addIndex(row, 1);
    const auto left = addIndex(column, -1);
    const auto right = addIndex(column, 1);
    const std::array<std::uint8_t, 8> neighbors{
        static_cast<std::uint8_t>(image(above, column)),
        static_cast<std::uint8_t>(image(above, right)),
        static_cast<std::uint8_t>(image(row, right)),
        static_cast<std::uint8_t>(image(below, right)),
        static_cast<std::uint8_t>(image(below, column)),
        static_cast<std::uint8_t>(image(below, left)),
        static_cast<std::uint8_t>(image(row, left)),
        static_cast<std::uint8_t>(image(above, left))};
    int transitions = 0;
    for (std::size_t index = 0; index < neighbors.size(); ++index) {
        if (!neighbors[index] && neighbors[(index + 1) % neighbors.size()]) ++transitions;
    }
    return transitions;
}
}

// Keep the actual view descriptor in the suspended frame; borrowed views still
// require their caller's actual backing owner throughout the call.
template<MutableSkeletonView2D T>
void skeletonizeImage(T image) {
    const auto height = static_cast<std::uint64_t>(image.shape()[0]);
    const auto width = static_cast<std::uint64_t>(image.shape()[1]);
    using compat::Vec2u;
    const auto rescan = [&] {
        compat::Slice<Vec2u> result;
        for (std::int32_t row = 1; static_cast<std::uint64_t>(row) < height - 1;
                row = detail::addIndex(row, 1)) {
            for (std::int32_t column = 1; static_cast<std::uint64_t>(column) < width - 1;
                    column = detail::addIndex(column, 1)) {
                if (image(row, column)) result.append({static_cast<std::uint32_t>(column),
                                                      static_cast<std::uint32_t>(row)});
            }
        }
        return result;
    };
    const auto apply = [&](compat::Slice<Vec2u> deletions) {
        compat::Slice<Vec2u> sorted;
        if (deletions.empty()) return sorted;
        for (const auto point : deletions) {
            const auto row = static_cast<std::int32_t>(point.y);
            const auto column = static_cast<std::int32_t>(point.x);
            image(row, column) = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const auto nextRow = detail::addIndex(row, dy);
                    const auto nextColumn = detail::addIndex(column, dx);
                    if (nextRow < 1 || nextRow >= height - 1
                        || nextColumn < 1 || nextColumn >= width - 1) continue;
                    sorted.append({static_cast<std::uint32_t>(nextColumn),
                                   static_cast<std::uint32_t>(nextRow)});
                }
            }
        }
        if (!sorted.empty()) {
            std::sort(sorted.begin(), sorted.end(), [](Vec2u left, Vec2u right) {
                return left.y < right.y || (left.y == right.y && left.x < right.x);
            });
        }
        // Source uniq.array constructs a distinct array, leaving the sorted backing alive.
        compat::Slice<Vec2u> result;
        for (const auto point : sorted) {
            if (result.empty() || result[result.size() - 1] != point) result.append(point);
        }
        return result;
    };
    const auto select = [&](compat::Slice<Vec2u> candidates, bool second) {
        compat::Slice<Vec2u> result;
        for (const auto point : candidates) {
            const auto row = static_cast<std::int32_t>(point.y);
            const auto column = static_cast<std::int32_t>(point.x);
            if (!image(row, column)) continue;
            const auto neighbors = detail::countNeighbors(image, row, column);
            if (neighbors < 2 || neighbors > 6) continue;
            if (detail::countTransitions(image, row, column) != 1) continue;
            if (!second) {
                if (image(detail::addIndex(row, -1), column) && image(row, detail::addIndex(column, 1))
                    && image(detail::addIndex(row, 1), column)) continue;
                if (image(row, detail::addIndex(column, 1)) && image(detail::addIndex(row, 1), column)
                    && image(row, detail::addIndex(column, -1))) continue;
            } else {
                if (image(detail::addIndex(row, -1), column) && image(row, detail::addIndex(column, 1))
                    && image(row, detail::addIndex(column, -1))) continue;
                if (image(detail::addIndex(row, -1), column) && image(detail::addIndex(row, 1), column)
                    && image(row, detail::addIndex(column, -1))) continue;
            }
            result.append(point);
        }
        return result;
    };
    auto candidates = rescan();
    bool changed = true;
    while (changed) {
        const auto firstDeletions = select(candidates, false);
        const auto firstCandidates = apply(firstDeletions);
        const auto secondDeletions = select(firstCandidates, true);
        candidates = apply(secondDeletions);
        changed = !firstDeletions.empty() || !secondDeletions.empty();
        if (candidates.empty()) {
            candidates = rescan();
            if (candidates.empty()) break;
        }
        tasks::TaskFiber::yield();
    }
}

}
