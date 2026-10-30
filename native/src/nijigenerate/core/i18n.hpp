#pragma once

#include "i18n_entry.hpp"
#include "i18n_entries.hpp"
#include "i18n_entry_for.hpp"
#include "i18n_current_name.hpp"
#include "i18n_set.hpp"
#include "i18n_compare.hpp"
#include "i18n_duplicates.hpp"
#include "i18n_scan.hpp"

namespace nijigenerate::core::i18n {

// Explicit application entry points; importing this header does not initialize locales.
void ngLocaleInit();
void ngLocaleInitFromSettings();

} // namespace nijigenerate::core::i18n
