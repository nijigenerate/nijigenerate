#include "detail/json_numeric_locale.hpp"
#include "detail/json_numeric_first_pass.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <clocale>
#include <cstdlib>
#ifndef _WIN32
#include <locale.h>
#endif

namespace nijigenerate::autorig::json::detail {
void* createNumericLocale(int category, const char* name) {
#ifdef _WIN32
    return _create_locale(category, name);
#else
    (void)category;
    return newlocale(LC_NUMERIC_MASK, name, nullptr);
#endif
}
void freeNumericLocale(void* locale) {
#ifdef _WIN32
    _free_locale(static_cast<_locale_t>(locale));
#else
    if (locale) freelocale(static_cast<locale_t>(locale));
#endif
}
double parseLocaleDouble(const char* text, char** end, void* locale) {
#ifdef _WIN32
    return _strtod_l(text, end, static_cast<_locale_t>(locale));
#else
    return strtod_l(text, end, static_cast<locale_t>(locale));
#endif
}
void* createArtifactNumericLocale() {
#ifdef _WIN32
    auto locale = createNumericLocale(LC_NUMERIC, "C");
    if (!locale) throw compat::ThrowableException(copyArtifactMessage("Could not create JSON numeric locale"));
    return locale;
#else
    return nullptr;
#endif
}
void freeArtifactNumericLocale(void* locale) {
    if (locale) freeNumericLocale(locale);
}
double restoreArtifactDouble(compat::RetainedStringSlice token, void* locale) {
#ifdef _WIN32
    const auto text = allocateArtifactNumberText([&] { return std::string(token.view()); });
    const double value = parseLocaleDouble(text.c_str(), nullptr, locale);
#else
    (void)locale;
    const double value = static_cast<double>(parseSourceFloating<long double>(token.view()));
#endif
    if (!std::isfinite(value))
        throw compat::ThrowableException(copyArtifactMessage("Nonfinite JSON artifact number"));
    return value;
}
}
