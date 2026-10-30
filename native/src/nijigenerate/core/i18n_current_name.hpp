#pragma once

#include "i18n_culture.hpp"
#include "settings.hpp"

namespace nijigenerate::core::i18n {

inline compat::RetainedString ngLocaleCurrentName() {
    const auto code = settings::incSettingsGet<settings::SettingsString>(
        compat::RetainedString::fromStaticLiteral("lang"),
        settings::SettingsString(compat::RetainedString::fromStaticLiteral("en")));
    return detail::cultureExpression(code.empty() ? std::string_view("en") : code.view());
}

} // namespace nijigenerate::core::i18n
