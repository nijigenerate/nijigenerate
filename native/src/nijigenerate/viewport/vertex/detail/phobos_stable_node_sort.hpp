#pragma once
// Adapted from independent LDC 1.41 Phobos std.algorithm.sorting TimSortImpl.
// Copyright Andrei Alexandrescu 2008-.
// Boost Software License 1.0; see native/src/nijigenerate/viewport/model/detail/phobos_stable_part_sort.LICENSE.

#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nodes/node.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nijigenerate::viewport::vertex::detail {
using SortedNode = std::shared_ptr<nicxlive::core::nodes::Node>;

#ifndef NDEBUG
[[noreturn]] inline void nodeSortAssertion(compat::RetainedString message, std::size_t sourceLine) {
    throw compat::ngStaticError<compat::AssertError>(std::move(message),
        compat::RetainedString::fromStaticLiteral("std/algorithm/sorting.d"), sourceLine);
}
#endif

// Only the original library's normal production contracts are evaluated here.
// StdUnittest code, fixtures and reference/native comparison code are excluded.
template<class Condition, class Message>
void nodeSortContract(Condition condition, Message message,
    std::size_t sourceLine) {
#ifndef NDEBUG
    if (!condition()) {
        compat::RetainedString failureMessage;
        try {
            failureMessage = compat::RetainedString(message());
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        nodeSortAssertion(std::move(failureMessage), sourceLine);
    }
#else
    (void)condition;
    (void)message;
    (void)sourceLine;
#endif
}

template<std::size_t N> compat::RetainedString nodeSortLiteral(const char (&message)[N]) {
    return compat::RetainedString::fromStaticLiteral(message);
}

template<class Less> class PhobosNodeTimSort {
    using Range = std::span<SortedNode>;
    struct Run { std::size_t base, length; };
    static constexpr std::size_t minimalMerge = 128, minimalGallop = 7, minimalStorage = 256, stackSize = 40;
    compat::Slice<SortedNode> input_;
    compat::Slice<SortedNode> temporary_;
    Less predicate_;

    static compat::Slice<SortedNode> allocateTemporary(std::size_t length) {
        // Only fresh reference-array storage is allocated here. Predicate,
        // merge and existing-owner destruction remain outside this boundary.
        try {
            return compat::Slice<SortedNode>(std::vector<SortedNode>(length));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }

    static Range view(compat::Slice<SortedNode>& values) {
        return values.isNull() ? Range{} : Range(values.data(), values.size());
    }
    template<class T> static T& element(std::span<T> values, std::size_t index,
            std::size_t sourceLine, std::string_view sourceFile = "std/algorithm/sorting.d") {
        if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticView(sourceFile), sourceLine);
        return values[index];
    }
    template<class T> static std::span<T> slice(std::span<T> values, std::size_t first,
            std::size_t last, std::size_t sourceLine) {
        if (first > last || last > values.size())
            throw compat::ngStaticError<compat::ArraySliceError>(first, last, values.size(),
                compat::RetainedString::fromStaticLiteral("std/algorithm/sorting.d"), sourceLine);
        if (!values.data()) return {};
        return values.subspan(first, last - first);
    }
    bool less(const SortedNode& a, const SortedNode& b) { return predicate_(a, b); }
    bool greater(const SortedNode& a, const SortedNode& b) { return less(b, a); }
    bool greaterEqual(const SortedNode& a, const SortedNode& b) { return !less(a, b); }
    bool lessEqual(const SortedNode& a, const SortedNode& b) { return !less(b, a); }

    bool sorted(Range range) {
        if (range.empty()) return true;
        for (std::size_t i = 0; i < range.size() - 1; ++i) {
            if (!less(element(range, i + 1, 178), element(range, i, 178))) continue;
            nodeSortContract([&] { return !less(element(range, i, 180), element(range, i + 1, 180)); },
                [] { return nodeSortLiteral("Predicate for isSorted is not antisymmetric. Both"
                    " pred(a, b) and pred(b, a) are true for certain values."); }, 179);
            return false;
        }
        return true;
    }

    // D's literal 1 is int, including the 32-bit intermediate mask/sign conversion.
    // Overshifts retain an implementation-defined original compiler-profile gate.
    static std::size_t intOneShift(unsigned shift) {
        const std::uint32_t bits = std::uint32_t{1} << shift;
        return static_cast<std::size_t>(std::bit_cast<std::int32_t>(bits));
    }
    static std::size_t minRunLength(std::size_t n) {
        const auto shift = static_cast<unsigned>(std::bit_width(n) - 1) - 5;
        const std::uint32_t mask = (std::uint32_t{1} << shift) - 1u;
        const auto inverse = static_cast<std::size_t>(std::bit_cast<std::int32_t>(~mask));
        return (n >> shift) + ((n & inverse) != 0);
    }
    static void moveEntry(Range from, std::size_t fromIndex, Range to, std::size_t toIndex,
            std::size_t sourceLine = 2930) {
        // Node is a D class reference: core.lifetime.move copies the handle and
        // does not clear the source cell. Both actual native owners stay retained.
        const auto value = element(from, fromIndex, sourceLine);
        element(to, toIndex, sourceLine) = value;
    }

    std::size_t firstRun(Range range) {
        auto finish = [&](std::size_t result) {
            nodeSortContract([&] { return result <= range.size(); },
                [] { return nodeSortLiteral("ret must be less or equal than range.length"); }, 2486);
            return result;
        };
        if (range.size() < 2) return finish(range.size());
        std::size_t i = 2;
        if (lessEqual(element(range, 0, 2496), element(range, 1, 2496))) {
            while (i < range.size() && lessEqual(element(range, i - 1, 2498), element(range, i, 2498))) ++i;
        } else {
            while (i < range.size() && greater(element(range, i - 1, 2502), element(range, i, 2502))) ++i;
            const auto reversed = slice(range, 0, i, 2503);
            std::reverse(reversed.begin(), reversed.end());
        }
        return finish(i);
    }
    void binaryInsertionSort(Range range, std::size_t sortedLength = 1) {
        for (; sortedLength < range.size(); ++sortedLength) {
            SortedNode item = element(range, sortedLength, 2311, "std/range/primitives.d");
            std::size_t lower = 0, upper = sortedLength;
            while (upper != lower) {
                const auto center = (lower + upper) / 2;
                if (less(item, element(range, center, 2526))) upper = center;
                else lower = center + 1;
            }
            for (upper = sortedLength; upper > lower; --upper)
                moveEntry(range, upper - 1, range, upper, 2537);
            element(range, lower, 2543) = item;
        }
        nodeSortContract([&] { return sorted(range); },
            [] { return nodeSortLiteral("range must be sorted"); }, 2512);
    }

    template<bool Reverse, bool Upper> std::size_t gallop(Range range, SortedNode value) {
        std::size_t lower = 0, center = 1, upper = range.size();
        auto& gap = center;
        if constexpr (Reverse) {
            const auto comp = [&](const SortedNode& a, const SortedNode& b) {
                if constexpr (Upper) return less(a, b);
                else return lessEqual(a, b);
            };
            while (gap <= upper) {
                if (comp(value, element(range, upper - gap, 2863))) {
                    upper -= gap;
                    gap *= 2;
                } else {
                    lower = upper - gap;
                    break;
                }
            }
            while (upper != lower) {
                center = lower + (upper - lower) / 2;
                if (comp(value, element(range, center, 2879))) upper = center;
                else lower = center + 1;
            }
        } else {
            const auto comp = [&](const SortedNode& a, const SortedNode& b) {
                if constexpr (Upper) return greaterEqual(a, b);
                else return greater(a, b);
            };
            while (lower + gap < upper) {
                if (comp(value, element(range, lower + gap, 2891))) {
                    lower += gap;
                    gap *= 2;
                } else {
                    upper = lower + gap;
                    break;
                }
            }
            while (lower != upper) {
                center = lower + (upper - lower) / 2;
                if (comp(value, element(range, center, 2907))) lower = center + 1;
                else upper = center;
            }
        }
        nodeSortContract([&] { return lower <= range.size(); },
            [] { return nodeSortLiteral("ret must be less or equal to range.length"); }, 2847);
        return lower;
    }
    compat::Slice<SortedNode> ensureCapacity(std::size_t minimum, compat::Slice<SortedNode> temporary) {
        if (temporary.size() < minimum) {
            auto newSize = intOneShift(static_cast<unsigned>(std::bit_width(minimum)));
            if (newSize < minimum) newSize = minimum;
            temporary = allocateTemporary(newSize);
        }
        nodeSortContract([&] { return temporary.size() >= minimum; },
            [] { return nodeSortLiteral("ensuring the capacity failed"); }, 2615);
        return temporary;
    }

    std::size_t mergeLo(Range range, std::size_t mid, std::size_t minGallop, Range temporary) {
        nodeSortContract([&] { return mid <= range.size(); },
            [] { return nodeSortLiteral("mid must be less than the length of the range"); }, 2660);
        nodeSortContract([&] { return temporary.size() >= mid; },
            [] { return nodeSortLiteral("temp.length must be greater or equal to mid"); }, 2662);
        temporary = slice(temporary, 0, mid, 2665);
        const auto sourceRun = slice(range, 0, mid, 2666);
        std::copy(sourceRun.begin(), sourceRun.end(), temporary.begin());
        moveEntry(range, mid, range, 0);
        std::size_t i = 1, left = 0, right = mid + 1;
        std::size_t countLeft = 0, countRight = 0;
        const auto leftEnd = temporary.size() - 1;
        if (left < leftEnd && right < range.size()) {
            for (;;) {
                countLeft = countRight = 0;
                while ((countLeft | countRight) < minGallop) {
                    if (lessEqual(element(temporary, left, 2684), element(range, right, 2684))) {
                        moveEntry(temporary, left++, range, i++);
                        if (left >= leftEnd) goto loDone;
                        ++countLeft;
                        countRight = 0;
                    } else {
                        moveEntry(range, right++, range, i++);
                        if (right >= range.size()) goto loDone;
                        countLeft = 0;
                        ++countRight;
                    }
                }
                do {
                    const auto leftRange = slice(temporary, left, temporary.size(), 2703);
                    const auto rightValue = element(range, right, 2703);
                    countLeft = gallop<false, true>(leftRange, rightValue);
                    for (std::size_t j = 0; j < countLeft; ++j)
                        moveEntry(temporary, left++, range, i++);
                    if (left >= temporary.size()) goto loDone;
                    const auto rightRange = slice(range, right, range.size(), 2707);
                    const auto leftValue = element(temporary, left, 2707);
                    countRight = gallop<false, false>(rightRange, leftValue);
                    for (std::size_t j = 0; j < countRight; ++j)
                        moveEntry(range, right++, range, i++);
                    if (right >= range.size()) for (;;) {
                        moveEntry(temporary, left++, range, i++);
                        if (left >= temporary.size()) goto loDone;
                    }
                    if (minGallop > 0) --minGallop;
                } while (countLeft >= minimalGallop || countRight >= minimalGallop);
                minGallop += 2;
            }
        }
    loDone:
        while (right < range.size()) moveEntry(range, right++, range, i++);
        while (left < temporary.size()) moveEntry(temporary, left++, range, i++);
        const auto result = minGallop > 0 ? minGallop : 1;
        nodeSortContract([&] { return sorted(range); },
            [] { return nodeSortLiteral("the range must be sorted"); }, 2654);
        return result;
    }
    std::size_t mergeHi(Range range, std::size_t mid, std::size_t minGallop, Range temporary) {
        nodeSortContract([&] { return mid <= range.size(); },
            [] { return nodeSortLiteral("mid must be less or equal to range.length"); }, 2745);
        nodeSortContract([&] { return temporary.size() >= range.size() - mid; }, [&] {
            return "temp.length " + std::to_string(temporary.size()) + " >= range.length " +
                std::to_string(range.size()) + " - mid " + std::to_string(mid);
        }, 2746);
        temporary = slice(temporary, 0, range.size() - mid, 2751);
        const auto sourceRun = slice(range, mid, range.size(), 2752);
        std::copy(sourceRun.begin(), sourceRun.end(), temporary.begin());
        moveEntry(range, mid - 1, range, range.size() - 1);
        std::size_t i = range.size() - 2, left = mid - 2, right = temporary.size() - 1;
        std::size_t countLeft = 0, countRight = 0;
        for (;;) {
            countLeft = countRight = 0;
            while ((countLeft | countRight) < minGallop) {
                if (greaterEqual(element(temporary, right, 2769), element(range, left, 2769))) {
                    moveEntry(temporary, right, range, i--);
                    if (right == 1) {
                        for (;;) {
                            moveEntry(range, left, range, i--);
                            if (left == 0) break;
                            --left;
                        }
                        moveEntry(temporary, 0, range, i);
                        goto hiDone;
                    }
                    --right;
                    countLeft = 0;
                    ++countRight;
                } else {
                    moveEntry(range, left, range, i--);
                    if (left == 0) for (;;) {
                        moveEntry(temporary, right, range, i--);
                        if (right == 0) goto hiDone;
                        --right;
                    }
                    --left;
                    ++countLeft;
                    countRight = 0;
                }
            }
            do {
                const auto rightRange = slice(temporary, 0, right, 2809);
                const auto leftValue = element(range, left, 2809);
                countRight = right - gallop<true, false>(rightRange, leftValue);
                for (std::size_t j = 0; j < countRight; ++j) {
                    moveEntry(temporary, right, range, i--);
                    if (right == 0) goto hiDone;
                    --right;
                }
                const auto leftRange = slice(range, 0, left, 2817);
                const auto rightValue = element(temporary, right, 2817);
                countLeft = left - gallop<true, true>(leftRange, rightValue);
                for (std::size_t j = 0; j < countLeft; ++j) {
                    moveEntry(range, left, range, i--);
                    if (left == 0) for (;;) {
                        moveEntry(temporary, right, range, i--);
                        if (right == 0) goto hiDone;
                        --right;
                    }
                    --left;
                }
                if (minGallop > 0) --minGallop;
            } while (countLeft >= minimalGallop || countRight >= minimalGallop);
            minGallop += 2;
        }
    hiDone:
        const auto result = minGallop > 0 ? minGallop : 1;
        nodeSortContract([&] { return sorted(range); },
            [] { return nodeSortLiteral("the range must be sorted"); }, 2738);
        return result;
    }
    void merge(Range range, std::size_t mid, std::size_t& minGallop) {
        nodeSortContract([&] { return sorted(slice(range, 0, mid, 2579)); },
            [] { return nodeSortLiteral("range[0 .. mid] must be sorted"); }, 2579);
        nodeSortContract([&] { return sorted(slice(range, mid, range.size(), 2581)); },
            [] { return nodeSortLiteral("range[mid .. range.length] must be sorted"); }, 2581);
        nodeSortContract([&] { return mid < range.size(); },
            [] { return nodeSortLiteral("mid must be less than the length of the range"); }, 2587);
        const auto firstRange = slice(range, 0, mid, 2591);
        const auto firstValue = element(range, mid, 2591);
        const auto firstElement = gallop<false, true>(firstRange, firstValue);
        const auto lastRange = slice(range, mid, range.size(), 2592);
        const auto lastValue = element(range, mid - 1, 2592);
        const auto lastElement = gallop<true, false>(lastRange, lastValue) + mid;
        range = slice(range, firstElement, lastElement, 2593);
        mid -= firstElement;
        if (mid == 0 || mid == range.size()) return;
        if (mid <= range.size() / 2) {
            temporary_ = ensureCapacity(mid, temporary_);
            minGallop = mergeLo(range, mid, minGallop, view(temporary_));
        } else {
            temporary_ = ensureCapacity(range.size() - mid, temporary_);
            minGallop = mergeHi(range, mid, minGallop, view(temporary_));
        }
    }
    void mergeAt(Range range, std::span<Run> stack, std::size_t at, std::size_t& minGallop) {
        nodeSortContract([&] { return stack.size() >= 2; },
            [] { return nodeSortLiteral("stack be be greater than 1"); }, 2554);
        nodeSortContract([&] { return stack.size() - at == 2 || stack.size() - at == 3; }, [&] {
            return "stack.length - at " + std::to_string(stack.size() - at) + " must be 2 or 3";
        }, 2555);
        const auto base = element(stack, at, 2560).base;
        const auto mid = element(stack, at, 2561).length;
        const auto length = element(stack, at + 1, 2562).length + mid;
        element(stack, at, 2565) = {base, length};
        if (stack.size() - at == 3) element(stack, stack.size() - 2, 2566) = element(stack, stack.size() - 1, 2566);
        merge(slice(range, base, base + length, 2569), mid, minGallop);
    }

public:
    PhobosNodeTimSort(compat::Slice<SortedNode> input, Less predicate)
        : input_(std::move(input)), predicate_(std::move(predicate)) {}
    void sort() {
        auto range = view(input_);
        if (range.size() <= minimalMerge) {
            binaryInsertionSort(range);
        } else {
            const auto minRun = minRunLength(range.size());
            const auto minTemp = std::min(range.size() / 2, minimalStorage);
            std::size_t minGallop = minimalGallop;
            std::array<Run, stackSize> storage;
            std::span<Run> stack(storage);
            std::size_t stackLength = 0;
            if (temporary_.size() < minTemp)
                temporary_ = allocateTemporary(minTemp);
            for (std::size_t i = 0; i < range.size();) {
                auto runLength = firstRun(slice(range, i, range.size(), 2397));
                if (runLength < minRun) {
                    const auto force = range.size() - i > minRun ? minRun : range.size() - i;
                    binaryInsertionSort(slice(range, i, i + force, 2404), runLength);
                    runLength = force;
                }
                element(stack, stackLength++, 2409) = {i, runLength};
                i += runLength;
                while (stackLength > 1) {
                    const auto run4 = stackLength - 1, run3 = stackLength - 2;
                    const auto run2 = stackLength - 3, run1 = stackLength - 4;
                    if ((stackLength > 2 && element(stack, run2, 2421).length <=
                            element(stack, run3, 2421).length + element(stack, run4, 2421).length) ||
                        (stackLength > 3 && element(stack, run1, 2422).length <=
                            element(stack, run3, 2422).length + element(stack, run2, 2422).length)) {
                        const auto at = element(stack, run2, 2424).length < element(stack, run4, 2424).length ? run2 : run3;
                        mergeAt(range, slice(stack, 0, stackLength, 2425), at, minGallop);
                    } else if (element(stack, run3, 2427).length > element(stack, run4, 2427).length) break;
                    else mergeAt(range, slice(stack, 0, stackLength, 2428), run3, minGallop);
                    --stackLength;
                }
            }
            while (stackLength > 1) {
                const auto run3 = stackLength - 1, run2 = stackLength - 2, run1 = stackLength - 3;
                const auto at = stackLength >= 3 &&
                    element(stack, run1, 2466).length <= element(stack, run3, 2466).length ? run1 : run2;
                mergeAt(range, slice(stack, 0, stackLength, 2468), at, minGallop);
                --stackLength;
            }
        }
        nodeSortContract([&] { return sorted(range); },
            [] { return nodeSortLiteral("Failed to sort range of type Node[]"); }, 1973);
    }
};

template<class Less> void phobosStableNodeSort(compat::Slice<SortedNode> values, Less predicate) {
    PhobosNodeTimSort<Less> sorting(std::move(values), std::move(predicate));
    sorting.sort();
}
} // namespace nijigenerate::viewport::vertex::detail
