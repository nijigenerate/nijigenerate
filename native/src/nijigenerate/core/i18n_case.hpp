#pragma once
// Algorithm adapted from Phobos std.uni/std.utf, Boost Software License 1.0.
// Attribution and complete license: native/port/licenses/phobos-locale-case.txt.
#include "i18n_case_data.hpp"
#include <common/unicode_lowercase.hpp>
#include <cstddef>
#include <iterator>
#include <string_view>

namespace nijigenerate::core::i18n::detail {
using LocaleCaseCursor = ::nicxlive::core::common::unicode::ReplacementUtf8Cursor;

inline std::uint32_t localeCaseScalar(std::uint64_t entry, std::size_t i) {
    return i == 0 ? static_cast<std::uint32_t>((entry >> 36) & 0x1ffff) :
        i == 1 ? static_cast<std::uint32_t>((entry >> 24) & 0xfff) :
        static_cast<std::uint32_t>((entry >> 12) & 0xfff);
}
inline int localeFullCasedCompare(std::uint32_t lhs, std::uint32_t rhs, LocaleCaseCursor& tail) {
    std::size_t low = 0, high = std::size(localeCaseIndex);
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        if ((localeCaseIndex[middle] & 0x1fffff) < lhs) low = middle + 1;
        else high = middle;
    }
    if (low == std::size(localeCaseIndex) || (localeCaseIndex[low] & 0x1fffff) != lhs)
        return static_cast<int>(lhs);
    const auto index = static_cast<std::size_t>(localeCaseIndex[low] >> 21);
    const auto metadata = localeCaseEntries[index];
    const auto start = index - ((metadata >> 8) & 15);
    const auto end = start + ((metadata >> 4) & 15);
    for (auto i = start; i < end; ++i) {
        const auto entry = localeCaseEntries[i];
        const auto length = static_cast<std::size_t>(entry & 15);
        if (localeCaseScalar(entry, 0) != rhs) continue;
        if (length == 1) return 0;
        auto candidate = tail;
        bool matches = true;
        for (std::size_t k = 1; k < length; ++k) {
            if (candidate.empty() || candidate.take() != localeCaseScalar(entry, k)) { matches = false; break; }
        }
        if (matches) { tail = candidate; return 0; }
    }
    return static_cast<int>(localeCaseScalar(localeCaseEntries[start], 0));
}
inline int localeCaseIcmp(std::string_view left, std::string_view right) {
    const auto end = left.size() < right.size() ? left.size() : right.size();
    std::size_t i = 0;
    for (; i < end; ++i) {
        const auto lhs = static_cast<std::uint8_t>(left[i]);
        const auto rhs = static_cast<std::uint8_t>(right[i]);
        if ((lhs | rhs) >= 0x80) break;
        if (lhs == rhs) continue;
        const auto lower = [](std::uint8_t c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; };
        const auto difference = lower(lhs) - lower(rhs);
        if (difference) return difference;
    }
    if (i == end) return (left.size() > right.size()) - (right.size() > left.size());
    LocaleCaseCursor first{left, i}, second{right, i};
    for (;;) {
        if (first.empty()) return second.empty() ? 0 : -1;
        const auto lhs = first.take();
        if (second.empty()) return 1;
        const auto rhs = second.take();
        if (lhs == rhs) continue;
        const auto lr = localeFullCasedCompare(lhs, rhs, second);
        if (!lr) continue;
        const auto rl = localeFullCasedCompare(rhs, lhs, first);
        if (!rl) continue;
        return lr - rl;
    }
}
} // namespace nijigenerate::core::i18n::detail
