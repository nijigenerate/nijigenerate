// Adapted from Phobos std.algorithm.sorting, shipped with LDC 1.41.0.
// Copyright: Andrei Alexandrescu 2008-.
// Boost Software License 1.0; see phobos_sort.LICENSE.
#pragma once

#include <algorithm>
#include <span>

namespace nijigenerate::core::selector::detail {

// Tokenizer exposes pointers into its sorted array, so even equal-key permutations
// are observable. std::sort cannot preserve the D baseline's ordering.
template<class T, class Less>
void phobosSort3(std::span<T> r, Less less, std::size_t a, std::size_t b, std::size_t c) {
    if (less(r[c], r[a])) {
        if (less(r[a], r[b])) { std::swap(r[a], r[b]); std::swap(r[a], r[c]); }
        else { std::swap(r[a], r[c]); if (less(r[b], r[a])) std::swap(r[a], r[b]); }
    } else if (less(r[b], r[a])) std::swap(r[a], r[b]);
    else if (less(r[c], r[b])) std::swap(r[b], r[c]);
}
template<class T, class Less>
void phobosSort5(std::span<T> r, Less less) {
    if (less(r[1], r[0])) std::swap(r[0], r[1]);
    if (less(r[3], r[2])) std::swap(r[2], r[3]);
    if (less(r[3], r[1])) { std::swap(r[0], r[2]); std::swap(r[1], r[3]); }
    if (less(r[4], r[1])) {
        std::swap(r[3], r[4]); std::swap(r[1], r[3]);
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
    } else if (less(r[4], r[3])) std::swap(r[3], r[4]);
    if (less(r[2], r[1])) {
        std::swap(r[1], r[2]);
        if (less(r[1], r[0])) std::swap(r[0], r[1]);
    } else if (less(r[3], r[2])) std::swap(r[2], r[3]);
}
template<class T, class Less>
void phobosShortSort(std::span<T> r, Less less) {
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
    phobosSort5(r.last(5), less);
    if (r.size() == 5) return;
    for (std::size_t i = r.size() - 5; i-- > 0;) {
        auto j = i;
        while (less(r[j + 1], r[j])) {
            std::swap(r[j], r[j + 1]);
            if (j == r.size() - 2) break;
            ++j;
        }
    }
}
template<class T, class Less>
void phobosHeapSort(std::span<T> r, Less less) {
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
template<class T, class Less>
void phobosQuickSort(std::span<T> r, Less less, std::size_t depth, std::size_t shortThreshold = 1024 / 24) {
    // D Token is enum + 64-bit slice: 24 bytes, independently of C++ string size.
    while (r.size() > shortThreshold) {
        if (depth == 0) { phobosHeapSort(r, less); return; }
        depth = depth >= static_cast<std::size_t>(-1) / 2 ? (depth / 3) * 2 : (depth * 2) / 3;
        const auto mid = r.size() / 2;
        if (r.size() < 512) phobosSort3(r, less, 0, mid, r.size() - 1);
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
        std::swap(r[mid], r.back());
        std::size_t lessI = static_cast<std::size_t>(-1), greaterI = r.size() - 1;
        while (true) {
            while (less(r[++lessI], pivot)) {}
            while (greaterI != lessI) {
                if (!less(pivot, r[--greaterI])) break;
            }
            if (lessI == greaterI) break;
            std::swap(r[lessI], r[greaterI]);
        }
        std::swap(r.back(), r[lessI]);
        auto left = r.first(lessI), right = r.subspan(lessI + 1);
        if (right.size() > left.size()) std::swap(left, right);
        phobosQuickSort(right, less, depth, shortThreshold);
        r = left;
    }
    phobosShortSort(r, less);
}
template<class T, class Less>
void phobosTokenSort(std::span<T> r, Less less) { phobosQuickSort(r, less, r.size()); }
}
