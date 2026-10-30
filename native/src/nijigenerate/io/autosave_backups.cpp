#include "autosave.hpp"
#include <nijigenerate/platform/autosave_files.hpp>

namespace nijigenerate::io::autosave {
AutosaveDirectoryEntries currentBackups(AutosaveString projectAutosaveDir) {
    AutosaveDirectoryEntries result;
    platform::ngAutosaveForEachDirectory(projectAutosaveDir, [&](platform::AutosaveFileEntry value) {
        AutosaveDirectoryEntry entry(std::move(value));
        // DirIterator.front returns by value; the predicate does not prime the stored entry's stat cache.
        auto predicateEntry = entry;
        if (predicateEntry.isFile()) result.append(entry);
    });
    return result;
}
}
