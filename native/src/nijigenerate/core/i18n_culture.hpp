#pragma once
// Narrow actual i18n-d 1.0.2 callees. BSD-2 notice: native/port/licenses/i18n-d.txt.
#include "i18n_culture_data.hpp"
#include "i18n_state.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include <stdexcept>

namespace nijigenerate::core::i18n::detail {
template<std::size_t N>
bool cultureContains(std::string_view code, const std::string_view (&values)[N]) {
    for (auto value : values) if (value == code) return true;
    return false;
}
template<std::size_t N>
bool cultureHasName(std::string_view code, const LocaleCultureName (&values)[N]) {
    for (const auto& value : values) if (value.code == code) return true;
    return false;
}
template<std::size_t N>
compat::RetainedString cultureName(std::string_view code, const LocaleCultureName (&values)[N]) {
    for (const auto& value : values)
        if (value.code == code) return compat::RetainedString::fromStaticView(value.name);
    return {};
}
inline std::string_view cultureCheckedSlice(std::string_view code, std::size_t first, std::size_t last) {
    if (first > last || last > code.size()) throw compat::ArraySliceError(first, last, code.size());
    return code.substr(first, last - first);
}
inline bool cultureCodeValid(std::string_view code) {
    if (code == "C") return true;
    if (code.size() < 2) return false;
    const auto language = code.substr(0, 2);
    if (code.size() == 2) return cultureContains(language, localeLanguageCodes);
    if (code.size() >= 5) return (code[2] == '-' || code[2] == '_') &&
        cultureContains(language, localeLanguageCodes) && cultureContains(code.substr(3, 2), localeCountryCodes);
    return false;
}
inline compat::RetainedString cultureEnglishLanguage(std::string_view code) {
    if (!cultureCodeValid(code)) return {};
    return cultureName(cultureCheckedSlice(code, 0, 2), localeLanguageNames);
}
inline compat::RetainedString cultureEnglishCountry(std::string_view code) {
    if (!cultureCodeValid(code) || code.size() < 5) return {};
    // Preserve the actual dependency's fixed upper endpoint, including errors.
    if (!cultureHasName(cultureCheckedSlice(code, code.size() - 2, 5), localeCountryNames)) return {};
    return cultureName(cultureCheckedSlice(code, 3, 5), localeCountryNames);
}
inline compat::RetainedString cultureExpression(std::string_view code) {
    for (const auto& entry : localeFiles) if (entry.code.view() == code) return entry.humanName;
    if (code.size() >= 5) {
        const auto language = cultureEnglishLanguage(code);
        const auto country = code == "zh-CN" ? compat::RetainedString::fromStaticLiteral("Simplified") :
            code == "zh-TW" ? compat::RetainedString::fromStaticLiteral("Traditional") : cultureEnglishCountry(code);
        return allocateLocaleStorage([&] {
            return compat::RetainedString(std::string(language.view()) + " (" + std::string(country.view()) + ")");
        });
    }
    return cultureEnglishLanguage(code);
}
} // namespace nijigenerate::core::i18n::detail
