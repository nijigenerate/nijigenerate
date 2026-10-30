#pragma once
#include <nijigenerate/compat/exception_message.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <string_view>

namespace nijigenerate::core::settings {
// Borrowed synchronous view of Exception.msg; dialog submission copies its bytes.
struct SettingsExceptionView { std::string_view message; };
void ngSettingsErrorDialog(const compat::ExceptionMessage& exception, compat::RetainedStringSlice backupPath);
void ngSettingsErrorDialog(const SettingsExceptionView* exception, compat::RetainedStringSlice backupPath);
// Host-view ingress owns its bytes before any translation callback.
void ngSettingsErrorDialog(const compat::ExceptionMessage& exception, std::string_view backupPath);
void ngSettingsErrorDialog(const SettingsExceptionView* exception, std::string_view backupPath);
}
