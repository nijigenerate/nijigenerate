#pragma once
#include "i18n_entry.hpp"
#include "nijigenerate/compat/slice.hpp"

namespace nijigenerate::core::i18n::detail {
inline thread_local compat::Slice<TLEntry> localeFiles;
} // namespace nijigenerate::core::i18n::detail
