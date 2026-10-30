#pragma once
#include "i18n_entry.hpp"
#include "i18n_case.hpp"

namespace nijigenerate::core::i18n {
inline bool ngCompareEntries(TLEntry a, TLEntry b) {
    const auto difference = detail::localeCaseIcmp(a.humanName.view(), b.humanName.view());
    return difference == 0 ? a.path.view() < b.path.view() : difference < 0;
}
} // namespace nijigenerate::core::i18n
