// Adapted from Phobos std.algorithm.sorting, shipped with LDC 1.41.0.
// Copyright: Andrei Alexandrescu 2008-.
// Boost Software License 1.0; see nijigenerate/core/selector/phobos_sort.LICENSE.
// Private Resource[] specialization for binding/base.d, whose comparator may throw.
#pragma once

#include <algorithm>
#include "base.hpp"
#include <nijigenerate/compat/assert_error.hpp>

namespace nijigenerate::commands::binding::detail::resource_sort {

inline ResourceRef& at(compat::Slice<ResourceRef>& range, std::size_t index, std::size_t line) {
    if (index >= range.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, range.size(),
        compat::RetainedString::fromStaticLiteral("std/algorithm/sorting.d"), line);
    return range.data()[index];
}

template<class Condition, std::size_t N>
void check(Condition condition, const char (&message)[N], std::size_t line) {
#ifndef NDEBUG
    if (!condition()) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral(message),
        compat::RetainedString::fromStaticLiteral("std/algorithm/sorting.d"), line);
#else
    (void)condition;
    (void)message;
    (void)line;
#endif
}

// Preserve source comparator/callback order, shallow array descriptors and swaps.
template<class Less>
void phobosSort3(compat::Slice<ResourceRef> r, Less less, std::size_t a, std::size_t b, std::size_t c) {
    if (less(r[c], r[a])) {
        if (less(r[a], r[b])) { std::swap(r[a], r[b]); std::swap(r[a], r[c]); }
        else { std::swap(r[a], r[c]); if (less(r[b], r[a])) std::swap(r[a], r[b]); }
    } else if (less(r[b], r[a])) std::swap(r[a], r[b]);
    else if (less(r[c], r[b])) std::swap(r[b], r[c]);
}
template<class Less>
void phobosSort5(compat::Slice<ResourceRef> r, Less less) {
    check([&] { return r.size() >= 5; }, "r must have more than 4 elements", 1807);
    if (less(r[1], r[0])) std::swap(r[0], r[1]);
    if (less(r[3], r[2])) std::swap(r[2], r[3]);
    if (less(r[3], r[1])) { std::swap(r[0], r[2]); std::swap(r[1], r[3]); }
    check([&] { return !less(r[1], r[0]) && !less(r[3], r[1]) && !less(r[3], r[2]); },
        "unexpected order", 1821);
    if (less(r[4], r[1])) {
        std::swap(r[3], r[4]); std::swap(r[1], r[3]);
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
    } else if (less(r[4], r[3])) std::swap(r[3], r[4]);
    check([&] { return !less(r[1], r[0]) && !less(r[3], r[1]) && !less(r[4], r[3]); },
        "unexpected order", 1838);
    check([&] { return !less(r[4], r[2]); }, "unexpected order", 1842);
    if (less(r[2], r[1])) {
        std::swap(r[1], r[2]);
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
    } else if (less(r[3], r[2])) std::swap(r[2], r[3]);
}
template<class Less>
void phobosShortSort(compat::Slice<ResourceRef> r, Less less) {
    if (r.size() < 2) return;
    if (r.size() == 2) { if (less(r[1], r[0])) std::swap(r[0], r[1]); return; }
    if (r.size() == 3) { phobosSort3(r, less, 0, 1, 2); return; }
    if (r.size() == 4) {
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
        if (less(r[3], r[2])) std::swap(r[2], r[3]);
        if (less(r[2], r[0])) std::swap(r[0], r[2]);
        if (less(r[3], r[1])) std::swap(r[1], r[3]);
        if (less(r[2], r[1])) std::swap(r[1], r[2]);
        return;
    }
    phobosSort5(r.slice(r.size() - 5, r.size()), less);
    if (r.size() == 5) return;
    check([&] { return r.size() >= 6; }, "r must have more than 5 elements", 1729);
    // getName may throw: the D nothrow move branch is not selected.
    for (std::size_t i = r.size() - 5; i-- > 0;) {
        auto j = i;
        while (less(r[j + 1], r[j])) {
            std::swap(r[j], r[j + 1]);
            if (j == r.size() - 2) break;
            ++j;
        }
    }
}
template<class Less>
void phobosHeapSort(compat::Slice<ResourceRef> r, Less less) {
    if (r.size() < 2) return;
    for (std::size_t i = r.size() / 2; i-- > 0;) {
        auto parent = i;
        while (true) {
            auto child = (parent + 1) * 2;
            if (child >= r.size()) {
                if (child == r.size() && less(r[parent], r[--child])) std::swap(r[parent], r[child]);
                break;
            }
            const auto leftChild = child - 1;
            if (less(r[child], r[leftChild])) child = leftChild;
            if (!less(r[parent], r[child])) break;
            std::swap(r[parent], r[child]);
            parent = child;
        }
    }
#ifndef NDEBUG
    for (std::size_t child = 1; child < r.size(); ++child) {
        if (!less(r[(child - 1) / 2], r[child])) continue;
        check([] { return false; }, "r is not a heap", 2258);
    }
#endif
    for (auto end = r.size() - 1; end > 0; --end) {
        std::swap(r[0], r[end]);
        std::size_t parent = 0;
        while (true) {
            auto child = (parent + 1) * 2;
            if (child >= end) {
                if (child == end) { --child; std::swap(r[parent], r[child]); parent = child; }
                break;
            }
            const auto leftChild = child - 1;
            if (less(r[child], r[leftChild])) child = leftChild;
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
template<class Less>
void phobosQuickSort(compat::Slice<ResourceRef> r, Less less, std::size_t depth,
        std::size_t shortThreshold = std::max<std::size_t>(32, 1024 / sizeof(void*))) {
    // D Resource interface references have pointer width, independent of ResourceRef.
    while (r.size() > shortThreshold) {
        if (depth == 0) { phobosHeapSort(r, less); return; }
        depth = depth >= static_cast<std::size_t>(-1) / 2 ? (depth / 3) * 2 : (depth * 2) / 3;
        const auto mid = r.size() / 2;
        if (r.size() < 512) {
            if (r.size() >= 32) {
                phobosSort3(r, less, 0, mid, r.size() - 1);
                check([&] { return !less(r[mid], r[0]); }, "less than check failed", 4420);
                check([&] { return !less(r[r.size() - 1], r[mid]); }, "less than check failed", 4421);
            }
        }
        else {
            const auto a = std::size_t{0}, b = mid - r.size() / 4, c = mid,
                       d = mid + r.size() / 4, e = r.size() - 1;
            if (less(r[c], r[a])) std::swap(r[a], r[c]);
            if (less(r[d], r[b])) std::swap(r[b], r[d]);
            if (less(r[d], r[c])) { std::swap(r[c], r[d]); std::swap(r[a], r[b]); }
            if (less(r[e], r[b])) std::swap(r[b], r[e]);
            if (less(r[e], r[c])) {
                std::swap(r[c], r[e]);
                if (less(r[c], r[a])) std::swap(r[a], r[c]);
            } else if (less(r[c], r[b])) std::swap(r[b], r[c]);
        }
        const auto pivot = r[mid];
        std::swap(r[mid], r[r.size() - 1]);
        std::size_t lessI = static_cast<std::size_t>(-1), greaterI = r.size() - 1;
        while (true) {
            while (less(at(r, ++lessI, 2194), pivot)) {}
            check([&] { return lessI <= greaterI; }, "sort: invalid comparison function.", 2195);
            bool exhausted = false;
            for (;;) {
                if (greaterI == lessI) { exhausted = true; break; }
                if (!less(pivot, r[--greaterI])) break;
            }
            if (exhausted) break;
            check([&] { return lessI <= greaterI; }, "sort: invalid comparison function.", 2201);
            if (lessI == greaterI) break;
            std::swap(r[lessI], r[greaterI]);
        }
        std::swap(r[r.size() - 1], r[lessI]);
        auto left = r.slice(0, lessI), right = r.slice(lessI + 1, r.size());
        if (right.size() > left.size()) std::swap(left, right);
        phobosQuickSort(right, less, depth, shortThreshold);
        r = left;
    }
    phobosShortSort(r, less);
}
template<class Less>
void sort(compat::Slice<ResourceRef> r, Less less) {
    phobosQuickSort(r, less, r.size());
#ifndef NDEBUG
    for (std::size_t index = 1; index < r.size(); ++index) {
        if (!less(r[index], r[index - 1])) continue;
        check([&] { return !less(r[index - 1], r[index]); },
            "Predicate for isSorted is not antisymmetric. Both"
            " pred(a, b) and pred(b, a) are true for certain values.", 179);
        check([] { return false; }, "Failed to sort range of type Resource[]", 1973);
    }
#endif
}
}
