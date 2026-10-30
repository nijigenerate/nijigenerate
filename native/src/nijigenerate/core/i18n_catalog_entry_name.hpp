#pragma once
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/platform/locale_catalog_entry_name.hpp"
#include <string_view>
namespace nijigenerate::core::i18n::detail {
inline compat::RetainedString localeCodeFromMoEntry(const compat::RetainedString& filename) {
    const auto range = platform::localeCodeRangeFromMoEntry(filename.view());
    return filename.subslice(range.first, range.last);
}
inline bool localeMoFilenameMatches(std::string_view filename) {
    return platform::localeMoFilenameMatches(filename);
}
} // namespace nijigenerate::core::i18n::detail
