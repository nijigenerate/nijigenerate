#pragma once

// Selected primitive-field value specializations of LDC 1.41 Phobos sorting.d.
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
#include <type_traits>

namespace nijigenerate::core::math::welding::detail::value_sort {

// Only the actual size_t[2] and Candidate primitive-field arrays call this
// private kernel. Their Source predicates and value copies cannot throw.
template<class T, class Before, std::size_t SourceBytes> class Kernel {
    using Range = std::span<T>;
    Before before;
    template<class Condition, std::size_t N>
    static void check(Condition condition, const char (&message)[N], std::size_t line) {
#ifndef NDEBUG
        if (!condition()) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral(message),
            compat::RetainedString::fromStaticLiteral("std/algorithm/sorting.d"), line);
#else
        (void)condition; (void)message; (void)line;
#endif
    }
    static T& at(Range r, std::size_t index, std::size_t line) {
        if (index >= r.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, r.size(), compat::RetainedString::fromStaticLiteral("std/algorithm/sorting.d"), line);
        return r[index];
    }
    void orderThree(Range r, std::size_t a, std::size_t b, std::size_t c) {
        if (before(r[c], r[a])) {
            if (before(r[a], r[b])) { std::swap(r[a], r[b]); std::swap(r[a], r[c]); }
            else { std::swap(r[a], r[c]); if (before(r[b], r[a])) std::swap(r[a], r[b]); }
        } else if (before(r[b], r[a])) std::swap(r[a], r[b]);
        else if (before(r[c], r[b])) std::swap(r[b], r[c]);
    }
    void medianThree(Range r, std::size_t a, std::size_t b, std::size_t c) {
        check([&] { return r.size() >= 3; }, "r.length must be greater than or equal to Indexes.length", 4349);
        check([&] { return a != b; }, "a != b ", 4365);
        check([&] { return a != c && b != c; }, "a != c && b != c", 4372);
        orderThree(r, a, b, c);
        check([&] { return !before(r[b], r[a]); }, "less than check failed", 4420);
        check([&] { return !before(r[c], r[b]); }, "less than check failed", 4421);
    }
    void sortFive(Range r) {
        check([&] { return r.size() >= 5; }, "r must have more than 4 elements", 1807);
        if (before(r[1], r[0])) std::swap(r[0], r[1]);
        if (before(r[3], r[2])) std::swap(r[2], r[3]);
        if (before(r[3], r[1])) { std::swap(r[0], r[2]); std::swap(r[1], r[3]); }
        check([&] { return !before(r[1], r[0]) && !before(r[3], r[1]) && !before(r[3], r[2]); },
            "unexpected order", 1821);
        if (before(r[4], r[1])) {
            std::swap(r[3], r[4]); std::swap(r[1], r[3]);
            if (before(r[1], r[0])) std::swap(r[0], r[1]);
        } else if (before(r[4], r[3])) std::swap(r[3], r[4]);
        check([&] { return !before(r[1], r[0]) && !before(r[3], r[1]) && !before(r[4], r[3]); },
            "unexpected order", 1838);
        check([&] { return !before(r[4], r[2]); }, "unexpected order", 1842);
        if (before(r[2], r[1])) {
            std::swap(r[1], r[2]); if (before(r[1], r[0])) std::swap(r[0], r[1]);
        } else if (before(r[3], r[2])) std::swap(r[2], r[3]);
    }
    void shortSort(Range r) {
        switch (r.size()) {
        case 0: case 1: return;
        case 2: if (before(r[1], r[0])) std::swap(r[0], r[1]); return;
        case 3: orderThree(r, 0, 1, 2); return;
        case 4:
            if (before(r[1], r[0])) std::swap(r[0], r[1]);
            if (before(r[3], r[2])) std::swap(r[2], r[3]);
            if (before(r[2], r[0])) std::swap(r[0], r[2]);
            if (before(r[3], r[1])) std::swap(r[1], r[3]);
            if (before(r[2], r[1])) std::swap(r[1], r[2]);
            return;
        default: sortFive(r.last(5)); if (r.size() == 5) return;
        }
        check([&] { return r.size() >= 6; }, "r must have more than 5 elements", 1729);
        for (std::size_t i = r.size() - 6;; --i) {
            auto j = i + 1;
            const auto value = r[i];
            if (before(r[j], value)) {
                do { r[j - 1] = r[j]; ++j; } while (j < r.size() && before(r[j], value));
                r[j - 1] = value;
            }
            if (i == 0) break;
        }
    }
    std::size_t pivot(Range r) {
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
        if (before(r[c], r[a])) std::swap(r[a], r[c]);
        if (before(r[d], r[b])) std::swap(r[b], r[d]);
        if (before(r[d], r[c])) { std::swap(r[c], r[d]); std::swap(r[a], r[b]); }
        if (before(r[e], r[b])) std::swap(r[b], r[e]);
        if (before(r[e], r[c])) {
            std::swap(r[c], r[e]); if (before(r[c], r[a])) std::swap(r[a], r[c]);
        } else if (before(r[c], r[b])) std::swap(r[b], r[c]);
        return middle;
    }
    void siftDown(Range r, std::size_t parent, std::size_t end) {
        for (;;) {
            auto child = (parent + 1) * 2;
            if (child >= end) {
                if (child == end && before(r[parent], r[--child])) std::swap(r[parent], r[child]);
                break;
            }
            if (before(r[child], r[child - 1])) --child;
            if (!before(r[parent], r[child])) break;
            std::swap(r[parent], r[child]); parent = child;
        }
    }
    void heapSort(Range r) {
        if (r.size() < 2) return;
        for (auto i = r.size() / 2; i-- > 0;) siftDown(r, i, r.size());
#ifndef NDEBUG
        bool heap = true;
        for (std::size_t child = 1; child < r.size(); ++child) {
            if (before(r[(child - 1) / 2], r[child])) { heap = false; break; }
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
                if (before(r[child], r[child - 1])) --child;
                std::swap(r[parent], r[child]); parent = child;
            }
            for (auto child = parent; child > 0; child = parent) {
                parent = (child - 1) / 2;
                if (!before(r[parent], r[child])) break;
                std::swap(r[parent], r[child]);
            }
        }
    }
    void quickSort(Range r, std::size_t depth) {
        constexpr auto limit = std::max(std::size_t{32}, std::size_t{1024} / SourceBytes);
        while (r.size() > limit) {
            if (depth == 0) { heapSort(r); return; }
            depth = depth >= std::numeric_limits<std::size_t>::max() / 2 ? (depth / 3) * 2 : (depth * 2) / 3;
            const auto index = pivot(r);
            const auto value = r[index];
            std::swap(r[index], r.back());
            auto less = std::numeric_limits<std::size_t>::max();
            auto greater = r.size() - 1;
            for (;;) {
                while (before(at(r, ++less, 2194), value)) {}
                check([&] { return less <= greater; }, "sort: invalid comparison function.", 2195);
                while (greater != less) { if (!before(value, at(r, --greater, 2199))) break; }
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
public:
    explicit Kernel(Before predicate) : before(predicate) {
        static_assert(std::is_trivially_copyable_v<T> && SourceBytes > 0);
    }
    template<std::size_t N> void sort(Range r, const char (&typeFailure)[N]) {
        quickSort(r, r.size());
#ifndef NDEBUG
        for (std::size_t i = 0; i + 1 < r.size(); ++i) {
            if (!before(r[i + 1], r[i])) continue;
            check([&] { return !before(r[i], r[i + 1]); },
                "Predicate for isSorted is not antisymmetric. Both pred(a, b) and pred(b, a) "
                "are true for certain values.", 179);
            check([] { return false; }, typeFailure, 1973);
        }
#else
        (void)typeFailure;
#endif
    }
};

template<std::size_t SourceBytes, class T, class Before, std::size_t N>
void sort(std::span<T> values, Before before, const char (&typeFailure)[N]) {
    Kernel<T, Before, SourceBytes>(before).sort(values, typeFailure);
}
} // namespace nijigenerate::core::math::welding::detail::value_sort
