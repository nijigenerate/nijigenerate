#pragma once
#include "translation.hpp"

namespace nijigenerate::core::i18n::detail {
inline compat::RetainedString localeLanguageNameMarker() {
    return translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("LANG_NAME"));
}
} // namespace nijigenerate::core::i18n::detail
