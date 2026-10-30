#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "autosave_files.hpp"
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/compat/phobos_file_exception.hpp>
#include <nijigenerate/compat/phobos_temp_cstring.hpp>
#include <nijigenerate/compat/phobos_file_write.hpp>
#include <nijigenerate/compat/core_time_exception.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <filesystem>
#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>
#include <limits>
#include <cstdio>
#include <cerrno>
#include <utility>
#include <cwchar>
#include <span>
#include <cstring>
#include <array>
#include <new>
#include <source_location>
#include <stdexcept>
#if defined(_WIN32)
#include <Windows.h>
#else
#include <dirent.h>
#endif

namespace nijigenerate::platform {
namespace {
template<class Char> bool matchesBackupName(std::basic_string_view<Char> filename) {
    const auto count = filename.size();
    if (count < 4 || filename[count - 4] != '.') return false;
    const auto patternLetter = [](Char value) {
#if defined(_WIN32) || defined(__APPLE__)
        // The fixed narrow-string glob compares code units, not Unicode folds.
        if (value == 'I') return static_cast<Char>('i');
        if (value == 'N') return static_cast<Char>('n');
        if (value == 'X') return static_cast<Char>('x');
#endif
        return value;
    };
    return patternLetter(filename[count - 3]) == 'i'
        && patternLetter(filename[count - 2]) == 'n'
        && patternLetter(filename[count - 1]) == 'x';
}

template<class Allocate> decltype(auto) allocateFileBytes(Allocate&& allocate) {
    try {
        return std::forward<Allocate>(allocate)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

#if defined(_WIN32)
std::wstring filesystemName(std::string_view bytes) {
    // Replacement decoding consumes the offending continuation too, as the D provider does.
    std::wstring result;
    for (std::size_t cursor = 0; cursor < bytes.size(); ) {
        const auto first = static_cast<unsigned char>(bytes[cursor++]);
        std::uint32_t scalar = first;
        if (first >= 0x80) {
            scalar = 0xfffd;
            if (first >= 0xc0) {
                const int width = first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
                std::uint32_t decoded = first & (width == 2 ? 0x1f : width == 3 ? 0x0f : 0x07);
                bool valid = true;
                for (int index = 1; index < width; ++index) {
                    if (cursor == bytes.size()) { valid = false; break; }
                    const auto next = static_cast<unsigned char>(bytes[cursor++]);
                    if ((next & 0xc0) != 0x80) { valid = false; break; }
                    decoded = (decoded << 6) | (next & 0x3f);
                }
                const std::uint32_t minimum = width == 2 ? 0x80 : width == 3 ? 0x800 : 0x10000;
                if (valid && first <= 0xf4 && decoded >= minimum && decoded <= 0x10ffff &&
                        !(decoded >= 0xd800 && decoded <= 0xdfff)) scalar = decoded;
            }
        }
        if (scalar <= 0xffff) result.push_back(static_cast<wchar_t>(scalar));
        else {
            scalar -= 0x10000;
            result.push_back(static_cast<wchar_t>(0xd800 + (scalar >> 10)));
            result.push_back(static_cast<wchar_t>(0xdc00 + (scalar & 0x3ff)));
        }
    }
    return result;
}

[[noreturn]] void fileFailure(const char* operation, const std::wstring& path, DWORD error) {
    throw std::filesystem::filesystem_error(operation, std::filesystem::path(path),
        std::error_code(static_cast<int>(error), std::system_category()));
}
#else
std::string filesystemName(std::string_view bytes) { return std::string(bytes); }
#endif
}

void ngAutosaveCreateDirectories(std::string_view path) {
    compat::phobos::ngMkdirRecurse(path);
}

void ngAutosaveWriteEmpty(std::string_view path) {
    const auto name = filesystemName(path);
#if defined(_WIN32)
    const auto handle = CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) fileFailure("write", name, GetLastError());
    if (!CloseHandle(handle)) fileFailure("close", name, GetLastError());
#else
    std::ofstream file(name.c_str(), std::ios::binary | std::ios::trunc);
    if (!file) throw std::filesystem::filesystem_error("write", name,
        std::make_error_code(std::errc::io_error));
    file.close();
    if (!file) throw std::filesystem::filesystem_error("close", name,
        std::make_error_code(std::errc::io_error));
#endif
}

void ngAutosaveWriteEmpty(compat::RetainedStringSlice path) {
    compat::phobos::ngFileWrite(path, std::string_view{});
}

bool ngAutosaveExists(std::string_view path) {
    return compat::phobos::ngFileExists(path);
}

void ngAutosaveRemove(std::string_view path) {
    const auto name = filesystemName(path);
#if defined(_WIN32)
    if (!DeleteFileW(name.c_str())) fileFailure("remove", name, GetLastError());
#else
    if (std::remove(name.c_str()) != 0)
        throw std::filesystem::filesystem_error("remove", name,
            std::error_code(errno, std::generic_category()));
#endif
}

// Used std.file.remove body, The D Language Foundation; BSL-1.0.
// See native/port/licenses/phobos-psd-file.txt.
void ngAutosaveRemove(compat::RetainedStringSlice path) {
    const auto fail = [](compat::RetainedString name, std::uint32_t code, std::size_t line) {
        compat::FileException error(std::move(name), code);
        error.file = compat::RetainedString::fromStaticLiteral("std/file.d");
        error.line = line;
        throw error;
    };
#if defined(_WIN32)
    const compat::phobos::TempCStringBuffer<wchar_t> name(path.view());
    if (!DeleteFileW(name.c_str())) {
        const auto diagnostic = path.isNull() ? compat::RetainedString{} : path.retainedString();
        const auto code = static_cast<std::uint32_t>(GetLastError());
        fail(diagnostic, code, 1065);
    }
#else
    const compat::phobos::TempCStringBuffer<char> name(path.view());
    if (std::remove(name.c_str()) != 0) {
        constexpr std::string_view prefix = "Failed to remove file ";
        const auto suffix = path.isNull() ? std::string_view{"(null)"} : path.view();
        const auto retainedDiagnostic = allocateFileBytes([&] {
            std::string diagnostic;
            diagnostic.reserve(prefix.size() + suffix.size());
            diagnostic.append(prefix);
            if (!suffix.empty()) diagnostic.append(suffix);
            return compat::RetainedString(std::move(diagnostic));
        });
        const auto code = static_cast<std::uint32_t>(errno);
        fail(retainedDiagnostic, code, 1078);
    }
#endif
}

std::vector<AutosaveFileEntry> ngAutosaveReadDirectory(std::string_view path) {
    return ngAutosaveReadDirectory(compat::RetainedStringSlice(compat::RetainedString(path)));
}

// Used std.file DirIterator/DirEntry and std.datetime.systime FILETIME conversion;
// The D Language Foundation / Jonathan M Davis, BSL-1.0. See port/licenses.
std::vector<AutosaveFileEntry> ngAutosaveReadDirectory(compat::RetainedStringSlice path) {
    std::vector<AutosaveFileEntry> result;
    ngAutosaveForEachDirectory(path, [&](AutosaveFileEntry entry) {
        result.push_back(std::move(entry));
    });
    return result;
}

#if !defined(_WIN32)
namespace {
[[noreturn]] void statFailure(compat::RetainedStringSlice name, std::size_t line) {
    constexpr std::string_view prefix = "Failed to stat file `";
    const auto retainedDiagnostic = allocateFileBytes([&] {
        std::string diagnostic;
        diagnostic.reserve(prefix.size() + name.size() + 1);
        diagnostic.append(prefix);
        if (!name.empty()) diagnostic.append(name.view());
        diagnostic.push_back('\'');
        return compat::RetainedString(std::move(diagnostic));
    });
    const auto code = static_cast<std::uint32_t>(errno);
    compat::FileException error(retainedDiagnostic, code);
    error.file = compat::RetainedString::fromStaticLiteral("std/file.d");
    error.line = line;
    throw error;
}
}

void ngAutosaveEnsureLStat(AutosaveFileEntry& entry) {
    if (entry.didLStat) return;
    struct stat value;
    const compat::phobos::TempCStringBuffer<char> name(entry.name.view());
    if (::lstat(name.c_str(), &value) != 0) statFailure(entry.name, 4100);
    entry.lstatMode = static_cast<std::uint32_t>(value.st_mode);
    entry.didLStat = true;
}

void ngAutosaveEnsureStat(AutosaveFileEntry& entry, bool allowBrokenLink) {
    if (entry.didStat) return;
    if (!allowBrokenLink) {
        const compat::phobos::TempCStringBuffer<char> name(entry.name.view());
        if (::stat(name.c_str(), &entry.statBuffer) != 0) statFailure(entry.name, 4059);
        entry.didStat = true;
        return;
    }
    const auto success = [&] {
        const compat::phobos::TempCStringBuffer<char> name(entry.name.view());
        return ::stat(name.c_str(), &entry.statBuffer) == 0;
    }();
    if (success) entry.didStat = true;
    else {
        ngAutosaveEnsureLStat(entry);
        entry.statBuffer = {};
        entry.statBuffer.st_mode = S_IFLNK;
    }
}
#endif

void ngAutosaveForEachDirectory(compat::RetainedStringSlice path,
        const std::function<void(AutosaveFileEntry)>& visit) {
#if defined(_WIN32)
    const std::array patternSegments{path.retainedString(), compat::RetainedString::fromStaticLiteral("*.*")};
    const auto searchPattern = compat::phobos::ngBuildPath(patternSegments);
    WIN32_FIND_DATAW data{};
    const auto handle = [&] {
        const compat::phobos::TempCStringBuffer<wchar_t> pattern(searchPattern.view());
        return FindFirstFileW(pattern.c_str(), &data);
    }();
    if (handle == INVALID_HANDLE_VALUE) {
        const auto code = static_cast<std::uint32_t>(GetLastError());
        compat::FileException error(path.retainedString(), code);
        error.file = compat::RetainedString::fromStaticLiteral("std/file.d");
        error.line = 4683;
        throw error;
    }
    struct HandleGuard { HANDLE value; ~HandleGuard() { FindClose(value); } } guard{handle};
    const auto instant = [](FILETIME value) {
        constexpr std::int64_t epoch = 504911232000000000;
        const auto ticks = (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
        if (ticks > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() - epoch)) {
            throw compat::TimeException(compat::RetainedString::fromStaticLiteral(
                "The given FILETIME cannot be represented as a stdTime value."),
                compat::RetainedString::fromStaticLiteral("std/datetime/systime.d"), 10134);
        }
        return static_cast<std::int64_t>(ticks) + epoch;
    };
    const auto nameBytes = [](std::wstring_view name) {
        compat::phobos::detail::UnicodeCursor<std::span<const wchar_t>, wchar_t> cursor(
            std::span<const wchar_t>(name.data(), name.size()));
        std::string bytes;
        while (!cursor.empty()) {
            const auto scalar = cursor.front();
            allocateFileBytes([&] { compat::file_exception_detail::appendScalar(bytes, scalar); });
            cursor.popFront();
        }
        return bytes;
    };
    do {
        if (std::wcscmp(data.cFileName, L".") == 0 || std::wcscmp(data.cFileName, L"..") == 0) continue;
        data.cFileName[sizeof(data.cFileName) / sizeof(data.cFileName[0]) - 1] = 0;
        const std::wstring_view filename(data.cFileName, std::wcslen(data.cFileName));
        AutosaveFileEntry entry;
        auto filenameBytes = nameBytes(filename);
        const auto retainedFilename = allocateFileBytes([&] {
            return compat::RetainedString(std::move(filenameBytes));
        });
        const std::array nameSegments{path.retainedString(), retainedFilename};
        entry.name = compat::phobos::ngBuildPath(nameSegments);
        entry.size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
        entry.created = instant(data.ftCreationTime);
        entry.accessed = instant(data.ftLastAccessTime);
        entry.modified = instant(data.ftLastWriteTime);
        entry.attributes = data.dwFileAttributes;
        entry.directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        entry.symlink = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        if (matchesBackupName(filename)) visit(std::move(entry));
    } while (FindNextFileW(handle, &data));
#else
    const auto handle = [&] {
        const compat::phobos::TempCStringBuffer<char> name(path.empty() ? std::string_view{"."} : path.view());
        return ::opendir(name.c_str());
    }();
    if (!handle) {
        const auto code = static_cast<std::uint32_t>(errno);
        compat::FileException error(path.retainedString(), code);
        error.file = compat::RetainedString::fromStaticLiteral("std/file.d");
        error.line = 4752;
        throw error;
    }
    struct DirectoryGuard { DIR* value; ~DirectoryGuard() { ::closedir(value); } } guard{handle};
    while (const auto item = ::readdir(handle)) {
        if (std::strcmp(item->d_name, ".") == 0 || std::strcmp(item->d_name, "..") == 0) continue;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        const std::string_view filename(item->d_name, item->d_namlen);
#else
        const std::string_view filename(item->d_name, std::strlen(item->d_name));
#endif
        AutosaveFileEntry entry;
        const std::array nameSegments{path.retainedString(), compat::RetainedString::fromBorrowedView(filename)};
        entry.name = compat::phobos::ngBuildPath(nameSegments);
        if (matchesBackupName(filename)) visit(std::move(entry));
    }
#endif
}
}
