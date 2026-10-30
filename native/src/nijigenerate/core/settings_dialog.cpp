#include "settings_dialog.hpp"
#include "translation.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::core::settings {
namespace {
std::string_view exceptionMessage(const compat::ExceptionMessage& exception) {
    return exception.ngExceptionMessage();
}
std::string_view exceptionMessage(const SettingsExceptionView& exception) {
    return exception.message;
}
template<class Exception>
void settingsErrorDialog(const Exception* exception, compat::RetainedStringSlice backupPath) {
    using translation::ngTranslateRetained;
    const auto initial = ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "Oops! Your settings.json file is corrupted. Nijigenerate will load the default settings.\n"));
    compat::RetainedString error = initial;
    const auto moved = ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "The corrupted settings file has been moved to "));
    try {
        std::string message(error.view());
        message += moved.view();
        message += backupPath.view();
        message += ".\n";
        error = compat::RetainedString(std::move(message));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    const auto report = ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "If you always see this message, please report this issue to Nijigenerate\n"));
    try {
        std::string message(error.view());
        message += report.view();
        error = compat::RetainedString(std::move(message));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    const auto errorLabel = ngTranslateRetained(compat::RetainedString::fromStaticLiteral("\nError message: "));
    const auto exceptionText = exceptionMessage(*compat::ngRequireObject(exception));
    try {
        std::string message(error.view());
        message += errorLabel.view();
        message += exceptionText;
        error = compat::RetainedString(std::move(message));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    const auto title = translation::ngTranslateCString("Error");
    using namespace widgets::dialog;
    ngDialog(DialogTag::retained(title.data(), title.owner), DialogText(std::move(error)));
}
}
void ngSettingsErrorDialog(const compat::ExceptionMessage& exception, compat::RetainedStringSlice backupPath) {
    settingsErrorDialog(&exception, std::move(backupPath));
}
void ngSettingsErrorDialog(const SettingsExceptionView* exception, compat::RetainedStringSlice backupPath) {
    settingsErrorDialog(exception, std::move(backupPath));
}
void ngSettingsErrorDialog(const compat::ExceptionMessage& exception, std::string_view backupPath) {
    compat::RetainedString backup;
    try {
        backup = compat::RetainedString(backupPath);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    ngSettingsErrorDialog(exception, compat::RetainedStringSlice(std::move(backup)));
}
void ngSettingsErrorDialog(const SettingsExceptionView* exception, std::string_view backupPath) {
    compat::RetainedString backup;
    try {
        backup = compat::RetainedString(backupPath);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    ngSettingsErrorDialog(exception, compat::RetainedStringSlice(std::move(backup)));
}
} // namespace nijigenerate::core::settings
