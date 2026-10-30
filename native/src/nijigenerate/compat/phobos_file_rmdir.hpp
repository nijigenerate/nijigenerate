#pragma once
// Used std.file.rmdirRecurse, The D Language Foundation, BSL-1.0.
// See port/licenses/phobos-psd-file.txt.
#include <nijigenerate/io/autosave_directory_entry.hpp>

namespace nijigenerate::compat::phobos {
// The existing production carrier implements the actual Source DirEntry cache.
using DirectoryEntry = io::autosave::AutosaveDirectoryEntry;
void ngRmdirRecurse(RetainedStringSlice path);
void ngRmdirRecurse(DirectoryEntry& entry);
void ngRmdirRecurse(DirectoryEntry&& entry);
} // namespace nijigenerate::compat::phobos
