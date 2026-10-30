#include "settings.hpp"
#include "settings_json_string.hpp"
#include <nijigenerate/compat/phobos_file_write.hpp>
#include <nijigenerate/compat/phobos_file_rename.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::core::settings {
std::string ngSettingsJsonString(const SettingsJsonValue& value) {
    const auto result = compat::ngJsonStringify(value);
    const auto text = result.view();
    return compat::json_detail::allocateStringifyStorage([&] { return std::string(text); });
}

void incSettingsSave() {
    auto swapName = incSettingsPath();
    compat::RetainedString swapPath;
    try {
        swapName += ".swp";
        swapPath = compat::RetainedString(std::move(swapName));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    const auto content = compat::ngJsonStringify(detail::settingsStore());
    compat::phobos::ngFileWrite(compat::RetainedStringSlice(swapPath), content.view());
    auto destinationName = incSettingsPath();
    compat::RetainedString destination;
    try {
        destination = compat::RetainedString(std::move(destinationName));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    compat::phobos::ngFileRename(compat::RetainedStringSlice(swapPath),
        compat::RetainedStringSlice(destination));
}
} // namespace nijigenerate::core::settings
