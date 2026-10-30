#pragma once

// Selected double[] specialization of LDC 1.41 Phobos std.algorithm.sorting.
// Copyright Andrei Alexandrescu 2008-; median-of-five by Teppo Niinimäki.
// Distributed under the Boost Software License, Version 1.0.
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated
// by a source language processor.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>

namespace nijigenerate::core::cv::contours::detail::intersection_sort {
using Range = std::span<double>;

template<class Condition, std::size_t N>
void check(Condition condition, const char (&message)[N], std::size_t line) {
#ifndef NDEBUG
    if (!condition()) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral(message),
        compat::RetainedString::fromStaticLiteral("std/algorithm/sorting.d"), line);
#else
    (void)condition; (void)message; (void)line;
#endif
}
inline double& at(Range r, std::size_t index, std::size_t line) {
    if (index >= r.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
        index, r.size(), compat::RetainedString::fromStaticLiteral("std/algorithm/sorting.d"), line);
    return r[index];
}
inline void orderThree(Range r, std::size_t a, std::size_t b, std::size_t c) {
    if (r[c] < r[a]) {
        if (r[a] < r[b]) { std::swap(r[a], r[b]); std::swap(r[a], r[c]); }
        else { std::swap(r[a], r[c]); if (r[b] < r[a]) std::swap(r[a], r[b]); }
    } else if (r[b] < r[a]) std::swap(r[a], r[b]);
    else if (r[c] < r[b]) std::swap(r[b], r[c]);
}
inline void medianThree(Range r, std::size_t a, std::size_t b, std::size_t c) {
    check([&] { return r.size() >= 3; }, "r.length must be greater than or equal to Indexes.length", 4349);
    check([&] { return a != b; }, "a != b ", 4365);
    check([&] { return a != c && b != c; }, "a != c && b != c", 4372);
    orderThree(r, a, b, c);
    check([&] { return !(r[b] < r[a]); }, "less than check failed", 4420);
    check([&] { return !(r[c] < r[b]); }, "less than check failed", 4421);
}
inline void sortFive(Range r) {
    check([&] { return r.size() >= 5; }, "r must have more than 4 elements", 1807);
    if (r[1] < r[0]) std::swap(r[0], r[1]);
    if (r[3] < r[2]) std::swap(r[2], r[3]);
    if (r[3] < r[1]) { std::swap(r[0], r[2]); std::swap(r[1], r[3]); }
    check([&] { return !(r[1] < r[0]) && !(r[3] < r[1]) && !(r[3] < r[2]); }, "unexpected order", 1821);
    if (r[4] < r[1]) {
        std::swap(r[3], r[4]); std::swap(r[1], r[3]);
        if (r[1] < r[0]) std::swap(r[0], r[1]);
    } else if (r[4] < r[3]) std::swap(r[3], r[4]);
    check([&] { return !(r[1] < r[0]) && !(r[3] < r[1]) && !(r[4] < r[3]); }, "unexpected order", 1838);
    check([&] { return !(r[4] < r[2]); }, "unexpected order", 1842);
    if (r[2] < r[1]) {
        std::swap(r[1], r[2]); if (r[1] < r[0]) std::swap(r[0], r[1]);
    } else if (r[3] < r[2]) std::swap(r[2], r[3]);
}
inline void shortSort(Range r) {
    switch (r.size()) {
    case 0: case 1: return;
    case 2: if (r[1] < r[0]) std::swap(r[0], r[1]); return;
    // shortSort's three-element branch has no medianOf assertions.
    case 3: orderThree(r, 0, 1, 2); return;
    case 4:
        if (r[1] < r[0]) std::swap(r[0], r[1]);
        if (r[3] < r[2]) std::swap(r[2], r[3]);
        if (r[2] < r[0]) std::swap(r[0], r[2]);
        if (r[3] < r[1]) std::swap(r[1], r[3]);
        if (r[2] < r[1]) std::swap(r[1], r[2]);
        return;
    default: sortFive(r.last(5)); if (r.size() == 5) return;
    }
    check([&] { return r.size() >= 6; }, "r must have more than 5 elements", 1729);
    for (std::size_t i = r.size() - 6;; --i) {
        auto j = i + 1;
        const double value = r[i];
        if (r[j] < value) {
            do { r[j - 1] = r[j]; ++j; } while (j < r.size() && r[j] < value);
            r[j - 1] = value;
        }
        if (i == 0) break;
    }
}
inline std::size_t pivot(Range r) {
    const auto middle = r.size() / 2;
    if (r.size() < 512) {
        if (r.size() >= 32) medianThree(r, 0, middle, r.size() - 1);
        return middle;
    }
    const auto quarter = r.size() / 4;
    const std::size_t a = 0, b = middle - quarter, c = middle, d = middle + quarter, e = r.size() - 1;
    check([&] { return r.size() >= 5; }, "r.length must be greater than or equal to Indexes.length", 4349);
    check([&] { return a != b; }, "a != b ", 4365);
    check([&] { return a != c && b != c; }, "a != c && b != c", 4372);
    check([&] { return a != d && b != d && c != d; }, "a != d && b != d && c != d failed", 4379);
    check([&] { return a != e && b != e && c != e && d != e; },
        "a != e && b != e && c != e && d != e failed", 4386);
    if (r[c] < r[a]) std::swap(r[a], r[c]);
    if (r[d] < r[b]) std::swap(r[b], r[d]);
    if (r[d] < r[c]) { std::swap(r[c], r[d]); std::swap(r[a], r[b]); }
    if (r[e] < r[b]) std::swap(r[b], r[e]);
    if (r[e] < r[c]) {
        std::swap(r[c], r[e]); if (r[c] < r[a]) std::swap(r[a], r[c]);
    } else if (r[c] < r[b]) std::swap(r[b], r[c]);
    return middle;
}
inline void siftDown(Range r, std::size_t parent, std::size_t end) {
    for (;;) {
        auto child = (parent + 1) * 2;
        if (child >= end) {
            if (child == end && r[parent] < r[--child]) std::swap(r[parent], r[child]);
            break;
        }
        if (r[child] < r[child - 1]) --child;
        if (!(r[parent] < r[child])) break;
        std::swap(r[parent], r[child]); parent = child;
    }
}
inline void heapSort(Range r) {
    if (r.size() < 2) return;
    for (auto i = r.size() / 2; i-- > 0;) siftDown(r, i, r.size());
#ifndef NDEBUG
    bool heap = true;
    for (std::size_t child = 1; child < r.size(); ++child) {
        if (r[(child - 1) / 2] < r[child]) { heap = false; break; }
    }
    check([&] { return heap; }, "r is not a heap", 2258);
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
            if (r[child] < r[child - 1]) --child;
            std::swap(r[parent], r[child]); parent = child;
        }
        for (auto child = parent; child > 0; child = parent) {
            parent = (child - 1) / 2;
            if (!(r[parent] < r[child])) break;
            std::swap(r[parent], r[child]);
        }
    }
}
inline void quickSort(Range r, std::size_t depth) {
    static_assert(sizeof(double) == 8);
    constexpr auto shortLimit = std::max(std::size_t{32}, std::size_t{1024} / sizeof(double));
    while (r.size() > shortLimit) {
        if (depth == 0) { heapSort(r); return; }
        depth = depth >= std::numeric_limits<std::size_t>::max() / 2 ? (depth / 3) * 2 : (depth * 2) / 3;
        const auto index = pivot(r);
        const double value = r[index];
        std::swap(r[index], r.back());
        auto less = std::numeric_limits<std::size_t>::max();
        auto greater = r.size() - 1;
        for (;;) {
            while (at(r, ++less, 2194) < value) {}
            check([&] { return less <= greater; }, "sort: invalid comparison function.", 2195);
            while (greater != less) { if (!(value < at(r, --greater, 2199))) break; }
            check([&] { return less <= greater; }, "sort: invalid comparison function.", 2201);
            if (less == greater) break;
            std::swap(r[less], r[greater]);
        }
        std::swap(r.back(), r[less]);
        auto left = r.first(less), right = r.subspan(less + 1);
        if (right.size() > left.size()) std::swap(left, right);
        quickSort(right, depth); r = left;
    }
    shortSort(r);
}
inline void sort(Range r) {
    quickSort(r, r.size());
#ifndef NDEBUG
    for (std::size_t i = 0; i + 1 < r.size(); ++i) {
        if (!(r[i + 1] < r[i])) continue;
        check([&] { return !(r[i] < r[i + 1]); },
            "Predicate for isSorted is not antisymmetric. Both pred(a, b) and pred(b, a) "
            "are true for certain values.", 179);
        check([] { return false; }, "Failed to sort range of type double[]", 1973);
    }
#endif
    // The discarded assumeSorted return copies its descriptor without checks.
}
} // namespace nijigenerate::core::cv::contours::detail::intersection_sort
