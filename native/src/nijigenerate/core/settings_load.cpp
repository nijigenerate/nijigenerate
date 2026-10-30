#include "settings.hpp"
#include "settings_dialog.hpp"
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/compat/phobos_file_read_text.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/platform/settings_file.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::core::settings {
namespace {
compat::RetainedString retainPath(std::string path) {
    // The actual path provider runs before this primitive backing allocation.
    try {
        return compat::RetainedString(std::move(path));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
void incSettingsLoad() {
    if (platform::ngSettingsFileExists(incSettingsPath())) {
        try {
            // Default parseJSON returns a null root for empty input. Complete
            // parsing before replacing the retained module root in either case.
            auto parsed = autorig::json::detail::parseJSON(
                compat::phobos::ngReadUtf8Text(retainPath(incSettingsPath())));
            auto& settings = detail::settingsStore();
            settings = std::move(parsed);
            if (settings.sourceObjectRef().empty())
                throw SettingsJsonError::fromStaticLiteral("Settings file is empty");
            return;
        } catch (const SettingsJsonError& error) {
            const auto backup = incSettingsMoveCorruptedFile(
                compat::RetainedStringSlice(retainPath(incSettingsPath())));
            ngSettingsErrorDialog(error, compat::RetainedStringSlice(backup));
        }
    }
    // Preserve the parsed root on failure: original defaults do not reset it.
    incSettingsSet(SettingsString(compat::RetainedString::fromStaticLiteral("ViewportZoomMode")),
        SettingsString(compat::RetainedString::fromStaticLiteral("MousePosition")));
    incSettingsSet(SettingsString(compat::RetainedString::fromStaticLiteral("ViewportZoomSpeed")), 5.0);
    incSettingsSet(SettingsString(compat::RetainedString::fromStaticLiteral("KeepLayerFolder")),
        SettingsString(compat::RetainedString::fromStaticLiteral("Ask")));
    incSettingsSet(SettingsString(compat::RetainedString::fromStaticLiteral("LayerGroupReplacement")),
        SettingsString(compat::RetainedString::fromStaticLiteral("Node")));
}
} // namespace nijigenerate::core::settings
