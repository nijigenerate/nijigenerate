#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <nijigenerate/compat/retained_string.hpp>

namespace nijigenerate::core::translation {
// The translation state is thread local, as in i18n-d 1.0.2.
void ngClearLanguage();
bool ngLoadLanguage(compat::RetainedString file);
bool ngLoadLanguage(const std::string& file);
bool ngLoadLanguage(const char* file);
std::string ngTranslate(std::string_view text);
compat::RetainedString ngTranslateRetained(compat::RetainedString text);
// A C-string result retains the actual lookup copy or MO byte owner across
// reentrant locale changes; fallback owns one source toStringz result.
struct TranslatedCString {
    const char* pointer;
    std::shared_ptr<const void> owner;
    const char* data() const noexcept { return pointer; }
};
TranslatedCString ngTranslateCString(std::string_view text);
} // namespace nijigenerate::core::translation
