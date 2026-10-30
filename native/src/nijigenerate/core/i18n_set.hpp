#pragma once
#include "i18n_entry_for.hpp"
#include "settings.hpp"
#include "translation.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"

namespace nijigenerate::core::i18n {
inline void ngLocaleSet(settings::SettingsString code) {
    settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("lang"), code);
    if (code.empty() || code.view() == "en") {
        translation::ngClearLanguage();
        return;
    }
    const auto entry = ngLocaleGetEntryFor(code.view());
    translation::ngLoadLanguage(compat::ngRequireObject(entry)->file);
}
} // namespace nijigenerate::core::i18n
