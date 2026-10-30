#pragma once
// Existing public spellings of the canonical shared std.uni source provider.
#include <common/unicode_lowercase.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <string>
#include <string_view>

namespace nijigenerate::io::imageexport::detail {
using ExtensionBoundsError = ::nicxlive::core::common::unicode::CaseBoundsError;
inline std::optional<std::size_t> lowerExtensionPrefix(std::string_view text) {
    return ::nicxlive::core::common::unicode::lowercasePrefix(text);
}
inline std::string lowerExtensionChanged(std::string_view text, std::size_t prefix) {
    return ::nicxlive::core::common::unicode::lowercaseChanged(text, prefix);
}
inline std::string lowerExtension(std::string_view text) {
    return ::nicxlive::core::common::unicode::toLowerBytes(text);
}
inline compat::RetainedString lowerExtension(compat::RetainedStringSlice text) {
    return ::nicxlive::core::common::unicode::toLower(text.retainedString());
}
} // namespace nijigenerate::io::imageexport::detail
