#pragma once
#include "i18n_state.hpp"

namespace nijigenerate::core::i18n {
inline compat::Slice<TLEntry> ngLocaleGetEntries() {
    return detail::localeFiles;
}
} // namespace nijigenerate::core::i18n
