#pragma once
// Required size_t-order path from Phobos std.algorithm.sorting (BSL-1.0).
// Copyright The D Language Foundation 2007-. See native/port/licenses/phobos-psd-file.txt.
#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>
#include "assert_error.hpp"
#include "retained_string.hpp"

namespace nijigenerate::compat::phobos_index_sort {
inline void guideSortCheck(bool condition, const char* message, std::size_t sourceLine) {
    if (!condition) throw ngStaticError<AssertError>(message,
        RetainedString::fromStaticLiteral("std/algorithm/sorting.d"), sourceLine);
}

template<class Less> void guideSortThree(std::span<std::size_t> r, Less less) {
    if (less(r[2], r[0])) {
        if (less(r[0], r[1])) { std::swap(r[0], r[1]); std::swap(r[0], r[2]); }
        else { std::swap(r[0], r[2]); if (less(r[1], r[0])) std::swap(r[0], r[1]); }
    } else {
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
        else if (less(r[2], r[1])) std::swap(r[1], r[2]);
    }
}

template<class Less> void guideSortFive(std::span<std::size_t> r, Less less) {
    if (less(r[1], r[0])) std::swap(r[0], r[1]);
    if (less(r[3], r[2])) std::swap(r[2], r[3]);
    if (less(r[3], r[1])) { std::swap(r[0], r[2]); std::swap(r[1], r[3]); }
#ifndef NDEBUG
    guideSortCheck(!less(r[1], r[0]) && !less(r[3], r[1]) && !less(r[3], r[2]), "unexpected order", 1821);
#endif
    if (less(r[4], r[1])) {
        std::swap(r[3], r[4]); std::swap(r[1], r[3]);
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
    } else if (less(r[4], r[3])) std::swap(r[3], r[4]);
#ifndef NDEBUG
    guideSortCheck(!less(r[1], r[0]) && !less(r[3], r[1]) && !less(r[4], r[3]), "unexpected order", 1838);
    guideSortCheck(!less(r[4], r[2]), "unexpected order", 1842);
#endif
    if (less(r[2], r[1])) {
        std::swap(r[1], r[2]);
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
    } else if (less(r[3], r[2])) std::swap(r[2], r[3]);
}

template<class Less> void guideShortSort(std::span<std::size_t> r, Less less) {
    if (r.size() < 2) return;
    if (r.size() == 2) { if (less(r[1], r[0])) std::swap(r[0], r[1]); return; }
    if (r.size() == 3) { guideSortThree(r, less); return; }
    if (r.size() == 4) {
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
        if (less(r[3], r[2])) std::swap(r[2], r[3]);
        if (less(r[2], r[0])) std::swap(r[0], r[2]);
        if (less(r[3], r[1])) std::swap(r[1], r[3]);
        if (less(r[2], r[1])) std::swap(r[1], r[2]);
        return;
    }
    guideSortFive(r.last(5), less);
    if (r.size() == 5) return;
    for (std::size_t i = r.size() - 6;; --i) {
        std::size_t j = i + 1;
        const auto temp = r[i];
        if (less(r[j], temp)) {
            do { r[j - 1] = r[j]; ++j; } while (j < r.size() && less(r[j], temp));
            r[j - 1] = temp;
        }
        if (i == 0) break;
    }
}

template<class Less> void guideSiftDown(std::span<std::size_t> r, std::size_t parent,
    std::size_t end, Less less) {
    for (;;) {
        auto child = (parent + 1) * 2;
        if (child >= end) {
            if (child == end && less(r[parent], r[--child])) std::swap(r[parent], r[child]);
            break;
        }
        const auto left = child - 1;
        if (less(r[child], r[left])) child = left;
        if (!less(r[parent], r[child])) break;
        std::swap(r[parent], r[child]); parent = child;
    }
}

template<class Less> void guideHeapSort(std::span<std::size_t> r, Less less) {
    if (r.size() < 2) return;
    for (auto i = r.size() / 2; i-- > 0;) guideSiftDown(r, i, r.size(), less);
#ifndef NDEBUG
    std::size_t heapParent = 0;
    for (std::size_t child = 1; child < r.size(); ++child) {
        guideSortCheck(!less(r[heapParent], r[child]), "r is not a heap", 2258);
        heapParent += !(child & 1);
    }
#endif
    std::size_t parent = 0;
    for (auto end = r.size() - 1; end > 0; --end) {
        std::swap(r[0], r[end]);
        parent = 0;
        for (;;) {
            auto child = (parent + 1) * 2;
            if (child >= end) {
                if (child == end) { --child; std::swap(r[parent], r[child]); parent = child; }
                break;
            }
            const auto left = child - 1;
            if (less(r[child], r[left])) child = left;
            std::swap(r[parent], r[child]); parent = child;
        }
        for (auto child = parent; child > 0; child = parent) {
            parent = (child - 1) / 2;
            if (!less(r[parent], r[child])) break;
            std::swap(r[parent], r[child]);
        }
    }
}

template<class Less> std::size_t guideSortPivot(std::span<std::size_t> r, Less less) {
    const auto mid = r.size() / 2;
    if (r.size() < 512) {
        // This path is called only above the size_t short-sort threshold.
        std::size_t sample[]{r[0], r[mid], r[r.size() - 1]};
        guideSortThree(sample, less);
        r[0] = sample[0]; r[mid] = sample[1]; r[r.size() - 1] = sample[2];
#ifndef NDEBUG
        guideSortCheck(!less(r[mid], r[0]), "less than check failed", 4420);
        guideSortCheck(!less(r[r.size() - 1], r[mid]), "less than check failed", 4421);
#endif
    } else {
        const auto quarter = r.size() / 4;
        const std::size_t a = 0, b = mid - quarter, c = mid, d = mid + quarter, e = r.size() - 1;
        if (less(r[c], r[a])) std::swap(r[a], r[c]);
        if (less(r[d], r[b])) std::swap(r[b], r[d]);
        if (less(r[d], r[c])) { std::swap(r[c], r[d]); std::swap(r[a], r[b]); }
        if (less(r[e], r[b])) std::swap(r[b], r[e]);
        if (less(r[e], r[c])) {
            std::swap(r[c], r[e]);
            if (less(r[c], r[a])) std::swap(r[a], r[c]);
        } else if (less(r[c], r[b])) std::swap(r[b], r[c]);
    }
    return mid;
}

template<std::size_t ElementBytes, class Less> void guideQuickSort(std::span<std::size_t> r, std::size_t depth, Less less) {
    constexpr auto threshold = std::max(std::size_t{32}, std::size_t{1024 / ElementBytes});
    while (r.size() > threshold) {
        if (depth == 0) { guideHeapSort(r, less); return; }
        depth = depth >= std::numeric_limits<std::size_t>::max() / 2 ? (depth / 3) * 2 : (depth * 2) / 3;
        const auto pivotIndex = guideSortPivot(r, less);
        const auto pivot = r[pivotIndex];
        std::swap(r[pivotIndex], r[r.size() - 1]);
        auto low = std::numeric_limits<std::size_t>::max(), high = r.size() - 1;
        for (;;) {
            while (less(r[++low], pivot)) {}
#ifndef NDEBUG
            guideSortCheck(low <= high, "sort: invalid comparison function.", 2195);
#endif
            bool finished = false;
            for (;;) {
                if (high == low) { finished = true; break; }
                if (!less(pivot, r[--high])) break;
            }
            if (finished) break;
#ifndef NDEBUG
            guideSortCheck(low <= high, "sort: invalid comparison function.", 2201);
#endif
            if (low == high) break;
            std::swap(r[low], r[high]);
        }
        std::swap(r[r.size() - 1], r[low]);
        auto left = r.first(low), right = r.subspan(low + 1);
        if (right.size() > left.size()) std::swap(left, right);
        guideQuickSort<ElementBytes>(right, depth, less);
        r = left;
    }
    guideShortSort(r, less);
}

template<std::size_t ElementBytes, class Less> void sort(std::span<std::size_t> order, Less less, const char* message) {
    guideQuickSort<ElementBytes>(order, order.size(), less);
#ifndef NDEBUG
    for (std::size_t i = 1; i < order.size(); ++i)
        guideSortCheck(!less(order[i], order[i - 1]), message, 1973);
#else
    (void)message;
#endif
}
}
