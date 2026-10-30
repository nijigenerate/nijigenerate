#include "settings.hpp"
#include "path.hpp"
#include <nijigenerate/platform/path_policy.hpp>
#include <nijigenerate/compat/phobos_file_rename.hpp>
#include <nijigenerate/compat/phobos_local_timestamp.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace nijigenerate::core::settings {
namespace {
// D starts with an object, but parseJSON may later replace it with any JSON type.
thread_local SettingsJsonValue settings = SettingsJsonObject{};
}
std::string incSettingsPath() {
    return platform::ngJoinRelativePath(path::incGetAppConfigPath(), "settings.json");
}
compat::RetainedString incSettingsMoveCorruptedFile(compat::RetainedStringSlice path) {
    std::string prefix;
    try {
        if (!path.empty()) prefix.assign(path.data(), path.size());
        prefix.push_back('.');
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    const auto timestamp = compat::phobos::ngLocalIsoTimestamp();
    compat::RetainedString backup;
    try {
        prefix.append(timestamp.view());
        backup = compat::RetainedString(std::move(prefix));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    compat::phobos::ngFileRename(path, compat::RetainedStringSlice(backup));
    return backup;
}
std::string incSettingsMoveCorruptedFile(const std::string& path) {
    const auto backup = incSettingsMoveCorruptedFile(
        compat::RetainedStringSlice(compat::RetainedString(std::string_view(path))));
    return std::string(backup.view());
}
SettingsJsonValue& detail::settingsStore() { return settings; }
const SettingsJsonValue* detail::findSetting(std::string_view name) {
    if (!settings.is_object()) (void)settings.sourceObjectNoRef();
    return settings.as_object().if_contains(name);
}
void detail::setSetting(SettingsString name, SettingsJsonValue value) {
    settings.sourceIndexAssign(std::move(name),
        [&](SettingsJsonValue& target) { target = value; }, [&] { return value; });
}
bool incSettingsCanGet(SettingsString name) { return detail::findSetting(name.view()) != nullptr; }
} // namespace nijigenerate::core::settings
