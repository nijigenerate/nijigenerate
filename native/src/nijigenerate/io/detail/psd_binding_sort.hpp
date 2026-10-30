#pragma once

// Record-range specialization of the current independent Phobos sort adapters.
// Copyright The D Language Foundation 2007-. BSL-1.0;
// see native/port/licenses/phobos-psd-file.txt.
#include "../depthmap_psd_internal_values.hpp"
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <cstddef>
#include <limits>
#include <utility>

namespace nijigenerate::io::depthmap_psd::detail::binding_sort {

using Range = compat::Slice<PngComposedBinding>;

inline void check(bool condition, const char* message, std::size_t line) {
    if (!condition)
        throw compat::ngStaticError<compat::AssertError>(message,
            compat::RetainedString::fromStaticLiteral("std/algorithm/sorting.d"), line);
}

template<class Less> void sortThree(Range r, std::size_t a, std::size_t b, std::size_t c, Less less) {
    if (less(r[c], r[a])) {
        if (less(r[a], r[b])) { std::swap(r[a], r[b]); std::swap(r[a], r[c]); }
        else { std::swap(r[a], r[c]); if (less(r[b], r[a])) std::swap(r[a], r[b]); }
    } else {
        if (less(r[b], r[a])) std::swap(r[a], r[b]);
        else if (less(r[c], r[b])) std::swap(r[b], r[c]);
    }
}

template<class Less> void sortFive(Range r, Less less) {
    if (less(r[1], r[0])) std::swap(r[0], r[1]);
    if (less(r[3], r[2])) std::swap(r[2], r[3]);
    if (less(r[3], r[1])) { std::swap(r[0], r[2]); std::swap(r[1], r[3]); }
#ifndef NDEBUG
    check(!less(r[1], r[0]) && !less(r[3], r[1]) && !less(r[3], r[2]), "unexpected order", 1821);
#endif
    if (less(r[4], r[1])) {
        std::swap(r[3], r[4]); std::swap(r[1], r[3]);
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
    } else if (less(r[4], r[3])) std::swap(r[3], r[4]);
#ifndef NDEBUG
    check(!less(r[1], r[0]) && !less(r[3], r[1]) && !less(r[4], r[3]), "unexpected order", 1838);
    check(!less(r[4], r[2]), "unexpected order", 1842);
#endif
    if (less(r[2], r[1])) {
        std::swap(r[1], r[2]);
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
    } else if (less(r[3], r[2])) std::swap(r[2], r[3]);
}

template<class Less> void shortSort(Range r, Less less) {
    if (r.size() < 2) return;
    if (r.size() == 2) { if (less(r[1], r[0])) std::swap(r[0], r[1]); return; }
    if (r.size() == 3) { sortThree(r, 0, 1, 2, less); return; }
    if (r.size() == 4) {
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
        if (less(r[3], r[2])) std::swap(r[2], r[3]);
        if (less(r[2], r[0])) std::swap(r[0], r[2]);
        if (less(r[3], r[1])) std::swap(r[1], r[3]);
        if (less(r[2], r[1])) std::swap(r[1], r[2]);
        return;
    }
    sortFive(r.slice(r.size() - 5, r.size()), less);
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

template<class Less> void siftDown(Range r, std::size_t parent, std::size_t end, Less less) {
    for (;;) {
        auto child = (parent + 1) * 2;
        if (child >= end) {
            if (child == end && less(r[parent], r[--child])) std::swap(r[parent], r[child]);
            break;
        }
        const auto left = child - 1;
        if (less(r[child], r[left])) child = left;
        if (!less(r[parent], r[child])) break;
        std::swap(r[parent], r[child]);
        parent = child;
    }
}

template<class Less> void heapSort(Range r, Less less) {
    if (r.size() < 2) return;
    for (auto i = r.size() / 2; i-- > 0;) siftDown(r, i, r.size(), less);
#ifndef NDEBUG
    std::size_t heapParent = 0;
    for (std::size_t child = 1; child < r.size(); ++child) {
        check(!less(r[heapParent], r[child]), "r is not a heap", 2258);
        heapParent += !(child & 1);
    }
#endif
    for (auto end = r.size() - 1; end > 0; --end) {
        std::swap(r[0], r[end]);
        std::size_t parent = 0;
        for (;;) {
            auto child = (parent + 1) * 2;
            if (child >= end) {
                if (child == end) { --child; std::swap(r[parent], r[child]); parent = child; }
                break;
            }
            const auto left = child - 1;
            if (less(r[child], r[left])) child = left;
            std::swap(r[parent], r[child]);
            parent = child;
        }
        for (auto child = parent; child > 0; child = parent) {
            parent = (child - 1) / 2;
            if (!less(r[parent], r[child])) break;
            std::swap(r[parent], r[child]);
        }
    }
}

template<class Less> std::size_t pivotIndex(Range r, Less less) {
    const auto mid = r.size() / 2;
    if (r.size() < 512) {
        // Compare and swap the actual records at each pivot sample position.
        sortThree(r, 0, mid, r.size() - 1, less);
#ifndef NDEBUG
        check(!less(r[mid], r[0]), "less than check failed", 4420);
        check(!less(r[r.size() - 1], r[mid]), "less than check failed", 4421);
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

template<class Less> void quickSort(Range r, std::size_t depth, Less less) {
    // Both the original and native PngComposedBinding records exceed 32 bytes.
    while (r.size() > 32) {
        if (depth == 0) { heapSort(r, less); return; }
        depth = depth >= std::numeric_limits<std::size_t>::max() / 2 ? (depth / 3) * 2 : (depth * 2) / 3;
        const auto position = pivotIndex(r, less);
        const auto pivot = r[position];
        std::swap(r[position], r[r.size() - 1]);
        auto low = std::numeric_limits<std::size_t>::max(), high = r.size() - 1;
        for (;;) {
            while (less(r[++low], pivot)) {}
#ifndef NDEBUG
            check(low <= high, "sort: invalid comparison function.", 2195);
#endif
            bool finished = false;
            for (;;) {
                if (high == low) { finished = true; break; }
                if (!less(pivot, r[--high])) break;
            }
            if (finished) break;
#ifndef NDEBUG
            check(low <= high, "sort: invalid comparison function.", 2201);
#endif
            if (low == high) break;
            std::swap(r[low], r[high]);
        }
        std::swap(r[r.size() - 1], r[low]);
        auto left = r.slice(0, low), right = r.slice(low + 1, r.size());
        if (right.size() > left.size()) std::swap(left, right);
        quickSort(right, depth, less);
        r = left;
    }
    shortSort(r, less);
}

template<class Less> void sort(Range r, Less less) {
    quickSort(r, r.size(), less);
#ifndef NDEBUG
    for (std::size_t i = 1; i < r.size(); ++i)
        check(!less(r[i], r[i - 1]), "Failed to sort range of type PngComposedBinding[]", 1973);
#endif
}

} // namespace nijigenerate::io::depthmap_psd::detail::binding_sort
