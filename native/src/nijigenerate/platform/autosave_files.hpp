#pragma once
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <string_view>
#include <string>
#include <cstdint>
#include <vector>
#include <functional>
#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace nijigenerate::platform {
void ngAutosaveCreateDirectories(std::string_view path);
void ngAutosaveWriteEmpty(std::string_view path);
void ngAutosaveWriteEmpty(compat::RetainedStringSlice path);
bool ngAutosaveExists(std::string_view path);
void ngAutosaveRemove(std::string_view path);
void ngAutosaveRemove(compat::RetainedStringSlice path);
struct AutosaveFileEntry {
    compat::RetainedStringSlice name;
    std::uint64_t size{};
    std::uint32_t attributes{};
    bool directory{}, symlink{};
    std::int64_t created{}, accessed{}, modified{};
#if !defined(_WIN32)
    struct stat statBuffer{};
    std::uint32_t lstatMode{};
    bool didStat{}, didLStat{}, nativeSnapshot{};
#endif
};
// The visitor completes for each entry before the next OS enumeration call.
void ngAutosaveForEachDirectory(compat::RetainedStringSlice path,
    const std::function<void(AutosaveFileEntry)>& visit);
#if !defined(_WIN32)
void ngAutosaveEnsureStat(AutosaveFileEntry& entry, bool allowBrokenLink = false);
void ngAutosaveEnsureLStat(AutosaveFileEntry& entry);
#endif
std::vector<AutosaveFileEntry> ngAutosaveReadDirectory(std::string_view path);
std::vector<AutosaveFileEntry> ngAutosaveReadDirectory(compat::RetainedStringSlice path);
}
