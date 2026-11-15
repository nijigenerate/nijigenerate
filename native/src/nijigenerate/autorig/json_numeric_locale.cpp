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
    auto locale = createNumericLocale(LC_NUMERIC, "C");
    if (!locale) throw compat::ThrowableException(copyArtifactMessage("Could not create JSON numeric locale"));
    return locale;
}
void freeArtifactNumericLocale(void* locale) {
    if (locale) freeNumericLocale(locale);
}
double restoreArtifactDouble(compat::RetainedStringSlice token, void* locale) {
    // Parse directly to correctly rounded binary64. On Apple ARM, long double
    // has no extra precision, so accumulating decimal digits and scaling them
    // can change a serialized coordinate by one ULP and invalidate ownership.
    const auto text = allocateArtifactNumberText([&] { return std::string(token.view()); });
    char* end = nullptr;
    const double value = parseLocaleDouble(text.c_str(), &end, locale);
    if (end != text.c_str() + text.size())
        throw compat::ThrowableException(copyArtifactMessage("Invalid JSON artifact number"));
    if (!std::isfinite(value))
        throw compat::ThrowableException(copyArtifactMessage("Nonfinite JSON artifact number"));
    return value;
}
}
