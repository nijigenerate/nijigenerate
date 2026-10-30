#pragma once
// Required i18n-d 1.0.2 catalog-name behavior. Copyright (c) 2020, Luna.
// BSD-2-Clause notice: native/port/licenses/i18n-d.txt.
#include "i18n_catalog_native_data.hpp"
#include "i18n_mo.hpp"
#include <cassert>
#include <cstdint>
#include <span>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace nijigenerate::core::i18n::detail {

inline compat::RetainedString catalogNameFromBytes(
    std::span<const std::uint8_t> bytes, std::string_view languageCode) {
    auto file = loadCatalogMO(bytes);
    const auto translatedName = queryCatalogMO(file.get(), "LANG_NAME");
    auto name = allocateMoStorage([&] { return compat::RetainedString(translatedName); });
    file.reset();
    if (name.view() == "LANG_NAME") name = cultureNativeLanguageEstimate(languageCode);
    if (name.view() == "LANG_NAME") name = cultureEnglishLanguage(languageCode);
    if (name.empty()) return compat::RetainedString::fromStaticLiteral("<UNKNOWN LANGUAGE>");
    return name;
}
inline compat::RetainedString catalogNameFromNonemptyBytes(
    std::span<const std::uint8_t> bytes, std::string_view languageCode) {
    assert(!bytes.empty());
    return catalogNameFromBytes(bytes, languageCode);
}

} // namespace nijigenerate::core::i18n::detail
