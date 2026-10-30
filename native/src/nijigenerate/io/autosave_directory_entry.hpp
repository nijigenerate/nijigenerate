#pragma once
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/platform/autosave_files.hpp>
#include <bit>
#include <cstdint>
#include <utility>

namespace nijigenerate::io::autosave {
// UTC instants use the D stdTime epoch and hundred-nanosecond unit.
struct AutosaveEntryTime { std::int64_t stdTime{}; };

class AutosaveDirectoryEntry {
public:
    using String = compat::RetainedStringSlice;
    AutosaveDirectoryEntry() = default;
    explicit AutosaveDirectoryEntry(platform::AutosaveFileEntry entry) : entry_(std::move(entry)) {}
    AutosaveDirectoryEntry(String name, std::uint64_t size, std::uint32_t attributes,
            bool directory, bool symlink, AutosaveEntryTime created, AutosaveEntryTime accessed,
            AutosaveEntryTime modified)
        : entry_{std::move(name), size, attributes, directory, symlink,
            created.stdTime, accessed.stdTime, modified.stdTime} {
#if !defined(_WIN32)
        entry_.nativeSnapshot = true;
#endif
    }
    String name() const noexcept { return entry_.name; }
    operator String() const noexcept { return entry_.name; }
#if defined(_WIN32)
    bool isDir() const noexcept { return entry_.directory; }
    bool isFile() const noexcept { return !entry_.directory; }
    bool isSymlink() const noexcept { return entry_.symlink; }
    std::uint64_t size() const noexcept { return entry_.size; }
    std::uint32_t attributes() const noexcept { return entry_.attributes; }
    std::uint32_t linkAttributes() const noexcept { return entry_.attributes; }
    AutosaveEntryTime timeCreated() const noexcept { return {entry_.created}; }
    AutosaveEntryTime timeLastAccessed() const noexcept { return {entry_.accessed}; }
    AutosaveEntryTime timeLastModified() const noexcept { return {entry_.modified}; }
#else
    bool isDir() {
        if (entry_.nativeSnapshot) return entry_.directory;
        platform::ngAutosaveEnsureStat(entry_, true);
        return (entry_.statBuffer.st_mode & S_IFMT) == S_IFDIR;
    }
    bool isFile() {
        if (entry_.nativeSnapshot) return !entry_.directory;
        platform::ngAutosaveEnsureStat(entry_, true);
        return (entry_.statBuffer.st_mode & S_IFMT) == S_IFREG;
    }
    bool isSymlink() {
        if (entry_.nativeSnapshot) return entry_.symlink;
        platform::ngAutosaveEnsureLStat(entry_);
        return (entry_.lstatMode & S_IFMT) == S_IFLNK;
    }
    std::uint64_t size() {
        if (entry_.nativeSnapshot) return entry_.size;
        platform::ngAutosaveEnsureStat(entry_);
        return static_cast<std::uint64_t>(entry_.statBuffer.st_size);
    }
    std::uint32_t attributes() {
        if (entry_.nativeSnapshot) return entry_.attributes;
        platform::ngAutosaveEnsureStat(entry_);
        return static_cast<std::uint32_t>(entry_.statBuffer.st_mode);
    }
    std::uint32_t linkAttributes() {
        if (entry_.nativeSnapshot) return entry_.attributes;
        platform::ngAutosaveEnsureLStat(entry_);
        return entry_.lstatMode;
    }
    AutosaveEntryTime timeStatusChanged() {
        platform::ngAutosaveEnsureStat(entry_);
        return {statTime<'c'>(entry_.statBuffer)};
    }
    // Legacy native snapshots expose this name; Source POSIX uses timeStatusChanged.
    AutosaveEntryTime timeCreated() const noexcept { return {entry_.created}; }
    AutosaveEntryTime timeLastAccessed() {
        if (entry_.nativeSnapshot) return {entry_.accessed};
        platform::ngAutosaveEnsureStat(entry_);
        return {statTime<'a'>(entry_.statBuffer)};
    }
    AutosaveEntryTime timeLastModified() {
        if (entry_.nativeSnapshot) return {entry_.modified};
        platform::ngAutosaveEnsureStat(entry_);
        return {statTime<'m'>(entry_.statBuffer)};
    }
    struct stat statBuf() {
        platform::ngAutosaveEnsureStat(entry_);
        return entry_.statBuffer;
    }
#endif
private:
    platform::AutosaveFileEntry entry_;
#if !defined(_WIN32)
    // Used std.file.statTimeToStdTime and std.datetime.systime.unixTimeToStdTime;
    // The D Language Foundation / Jonathan M Davis, BSL-1.0. See port/licenses.
    template<char Which, class Stat> static std::int64_t statTime(const Stat& value) {
        std::int64_t seconds;
        std::int64_t nanoseconds = 0;
        if constexpr (Which == 'a') {
            seconds = value.st_atime;
            if constexpr (requires { value.st_atim.tv_nsec; }) nanoseconds = value.st_atim.tv_nsec;
            else if constexpr (requires { value.st_atimespec.tv_nsec; }) nanoseconds = value.st_atimespec.tv_nsec;
            else if constexpr (requires { value.st_atimensec; }) nanoseconds = value.st_atimensec;
            else if constexpr (requires { value.st_atime_nsec; }) nanoseconds = value.st_atime_nsec;
            else if constexpr (requires { value.__st_atimensec; }) nanoseconds = value.__st_atimensec;
        } else if constexpr (Which == 'm') {
            seconds = value.st_mtime;
            if constexpr (requires { value.st_mtim.tv_nsec; }) nanoseconds = value.st_mtim.tv_nsec;
            else if constexpr (requires { value.st_mtimespec.tv_nsec; }) nanoseconds = value.st_mtimespec.tv_nsec;
            else if constexpr (requires { value.st_mtimensec; }) nanoseconds = value.st_mtimensec;
            else if constexpr (requires { value.st_mtime_nsec; }) nanoseconds = value.st_mtime_nsec;
            else if constexpr (requires { value.__st_mtimensec; }) nanoseconds = value.__st_mtimensec;
        } else {
            seconds = value.st_ctime;
            if constexpr (requires { value.st_ctim.tv_nsec; }) nanoseconds = value.st_ctim.tv_nsec;
            else if constexpr (requires { value.st_ctimespec.tv_nsec; }) nanoseconds = value.st_ctimespec.tv_nsec;
            else if constexpr (requires { value.st_ctimensec; }) nanoseconds = value.st_ctimensec;
            else if constexpr (requires { value.st_ctime_nsec; }) nanoseconds = value.st_ctime_nsec;
            else if constexpr (requires { value.__st_ctimensec; }) nanoseconds = value.__st_ctimensec;
        }
        const auto ticks = static_cast<std::uint64_t>(seconds) * UINT64_C(10000000)
            + UINT64_C(621355968000000000) + static_cast<std::uint64_t>(nanoseconds / 100);
        return std::bit_cast<std::int64_t>(ticks);
    }
#endif
};
}
