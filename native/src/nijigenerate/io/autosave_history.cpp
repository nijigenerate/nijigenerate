#include "autosave.hpp"
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/platform/autosave_files.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace nijigenerate::io::autosave {
namespace detail {
std::int64_t elapsedAutosaveMinutes();
void resetAutosaveTimer();
}
namespace {
void storeRecords(const AutosaveRecords& records) {
    core::settings::SettingsStringArray paths, mains;
    for (const auto& record : records) {
        paths.append(record.autosavePath.retainedString());
        mains.append(record.mainsavePath.retainedString());
    }
    core::settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("prev_autosaves"), paths);
    core::settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("prev_autosave_mainpaths"), mains);
    core::settings::incSettingsSave();
}
}
void incAddPrevAutosave(AutosaveString path) {
    auto previous = incGetPrevAutosaves();
    for (std::size_t i = 0; i < previous.size(); ++i) {
        if (previous[i].autosavePath != path) continue;
        for (std::size_t j = i; j + 1 < previous.size(); ++j) previous[j] = previous[j + 1];
        previous = previous.slice(0, previous.size() - 1);
        break;
    }
    const AutosaveRecord first{path, project::incProjectPath()};
    if (previous.size() == (std::numeric_limits<std::size_t>::max)())
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    AutosaveRecords records;
    try {
        records = AutosaveRecords(std::vector<AutosaveRecord>(previous.size() + 1));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    records[0] = first;
    for (std::size_t i = 0; i < previous.size(); ++i) records[i + 1] = previous[i];
    if (records.size() > 10) records = records.slice(0, 10);
    storeRecords(records);
}
void incCheckAutosave() {
    if (project::incProjectPath().empty() || !incGetAutosaveEnabled()) return;
    const std::int64_t interval = incGetAutosaveInterval();
    const std::int64_t elapsedMinutes = detail::elapsedAutosaveMinutes();
    if (elapsedMinutes >= interval) {
        incAutosaveProject(project::incProjectPath());
        detail::resetAutosaveTimer();
    }
}
void incPruneAutosaveList() {
    const auto previous = incGetPrevAutosaves();
    AutosaveRecords records;
    const auto count = previous.size();
    for (std::size_t i = 0; i < count; ++i) {
        const auto path = previous[i].autosavePath;
        if (platform::ngAutosaveExists(path.view())) records.append(previous[i]);
    }
    storeRecords(records);
}
void incAutosaveProject(AutosaveString path) {
    const auto lockpath = path;
    path = path.slice(0, platform::ngAutosaveStripExtension(path.view()).size());
    const auto basename = platform::ngAutosaveBaseName(path.view());
    AutosaveString retainedBasename;
    if (!basename.empty()) {
        const auto offset = static_cast<std::size_t>(basename.data() - path.data());
        retainedBasename = path.slice(offset, offset + basename.size());
    }
    const auto directory = getAutosaveDir(path);
    platform::ngAutosaveCreateDirectories(directory.view());
    const std::array segments{directory.retainedString(), retainedBasename.retainedString()};
    path = AutosaveString(compat::phobos::ngBuildPath(segments));
    auto entries = currentBackups(directory);
    const auto limit = incGetAutosaveFileLimit();
    // The original size_t comparison converts negative stored settings to unsigned.
    while (entries.size() >= static_cast<std::size_t>(limit)) {
        if (entries.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(
            std::size_t{0}, entries.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/autosave.d"), 154);
        platform::ngAutosaveRemove(entries.data()[0].name());
        entries = currentBackups(directory);
    }
    incPruneAutosaveList();
    project::incSaveProject(path, bakStampString());
    incCreateLockfile(lockpath);
}
}
