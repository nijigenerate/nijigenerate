#include "nijigenerate/autorig/detail/json_numeric_locale.hpp"

#if defined(_WIN32)
#include <locale.h>
#include <stdlib.h>

namespace nijigenerate::autorig::json::detail {

void* createNumericLocale(int category, const char* name) {
    return static_cast<void*>(_create_locale(category, name));
}

void freeNumericLocale(void* locale) {
    _free_locale(static_cast<_locale_t>(locale));
}

double parseLocaleDouble(const char* text, char** end, void* locale) {
    return _strtod_l(text, end, static_cast<_locale_t>(locale));
}

}
#endif

#include "nijigenerate/autorig/detail/json_numeric_first_pass.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <string_view>

namespace nijigenerate::autorig::json::detail {
void* createArtifactNumericLocale() {
#if defined(_WIN32)
    auto locale = createNumericLocale(LC_NUMERIC, "C");
    if (!locale) throw compat::ThrowableException(
        copyArtifactMessage("Could not create JSON numeric locale"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/json.d"), 21);
    return locale;
#else
    // The original version branch has no locale creation or cached locale.
    return nullptr;
#endif
}
void freeArtifactNumericLocale(void* locale) {
#if defined(_WIN32)
    freeNumericLocale(locale);
#else
    (void)locale;
#endif
}
double restoreArtifactDouble(compat::RetainedStringSlice token, void* locale) {
#if defined(_WIN32)
    std::array<char, 128> buffer;
    buffer.fill(std::bit_cast<char>(std::uint8_t{0xff}));
    std::optional<compat::SourceCString> terminated;
    const char* input;
    if (token.size() < buffer.size()) {
        std::copy(token.view().begin(), token.view().end(), buffer.begin());
        buffer[token.size()] = 0;
        input = buffer.data();
    } else {
        terminated.emplace(token);
        input = terminated->data();
        compat::detail::ngSourceCStringPostconditions(token.view(), input);
    }
    return parseLocaleDouble(input, nullptr, locale);
#else
    (void)locale;
    // Parse returns source real before the original separate narrowing cast.
    const volatile auto value = parseSourceFloating<numeric::Real>(token.view());
    return static_cast<double>(value);
#endif
}
}
