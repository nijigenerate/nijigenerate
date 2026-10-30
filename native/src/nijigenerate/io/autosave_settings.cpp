#include "autosave.hpp"
#include <nijigenerate/core/settings.hpp>
#include <algorithm>

namespace nijigenerate::io::autosave {
namespace settings = core::settings;

std::int32_t incGetAutosaveInterval() {
    return settings::incSettingsGet<std::int32_t>(
        compat::RetainedString::fromStaticLiteral("AutosaveInterval"), 5);
}

void incSetAutosaveInterval(std::int32_t interval) {
    interval = std::clamp(interval, std::int32_t{1}, std::int32_t{1440});
    settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("AutosaveInterval"), interval);
}

std::int32_t incGetAutosaveFileLimit() {
    return settings::incSettingsGet<std::int32_t>(
        compat::RetainedString::fromStaticLiteral("AutosaveFileLimit"), 3);
}

void incSetAutosaveFileLimit(std::int32_t fileLimit) {
    fileLimit = std::clamp(fileLimit, std::int32_t{1}, std::int32_t{1000});
    settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("AutosaveFileLimit"), fileLimit);
}

bool incGetAutosaveEnabled() {
    return settings::incSettingsGet<bool>(compat::RetainedString::fromStaticLiteral("AutosaveEnabled"), true);
}

void incSetAutosaveEnabled(bool enabled) {
    settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("AutosaveEnabled"), enabled);
}

AutosaveRecords incGetPrevAutosaves() {
    AutosaveRecords saveRecords;
    const auto autosavePaths = settings::incSettingsGet<settings::SettingsStringArray>(
        compat::RetainedString::fromStaticLiteral("prev_autosaves"));
    const auto mainsavePaths = settings::incSettingsGet<settings::SettingsStringArray>(
        compat::RetainedString::fromStaticLiteral("prev_autosave_mainpaths"));
    for (std::size_t i = 0; i < autosavePaths.size(); ++i) {
        const AutosaveString autosavePath(autosavePaths[i]);
        AutosaveString mainsavePath(compat::RetainedString::emptyLiteral());
        if (i < mainsavePaths.size()) mainsavePath = AutosaveString(mainsavePaths[i]);
        saveRecords.append(AutosaveRecord{autosavePath, mainsavePath});
    }
    return saveRecords;
}

} // namespace nijigenerate::io::autosave
