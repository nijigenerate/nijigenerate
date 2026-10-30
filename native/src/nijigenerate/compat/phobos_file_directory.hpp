#pragma once
// Used std.file directory bodies, Copyright The D Language Foundation 2007-2011;
// std.path lexical bodies, Copyright (c) 2000-2014, the authors. BSL-1.0.
// See port/licenses/phobos-psd-file.txt and the installed std/path.d attribution.
#include "phobos_file_exists.hpp"
#include "phobos_file_exception.hpp"
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace nijigenerate::compat::phobos {
namespace directory_detail {
inline bool dirSeparator(char value) noexcept {
#ifdef _WIN32
    return value == '/' || value == '\\';
#else
    return value == '/';
#endif
}
inline bool separator(char value) noexcept {
#ifdef _WIN32
    return dirSeparator(value) || value == ':';
#else
    return dirSeparator(value);
#endif
}
inline std::string_view trimSeparators(std::string_view path) noexcept {
    while (!path.empty() && dirSeparator(path.back())) path.remove_suffix(1);
    return path;
}
// Return the position after the final separator (zero when there is none).
inline std::size_t afterSeparator(std::string_view path) noexcept {
    auto end = path.size();
    while (end && !separator(path[end - 1])) --end;
    return end;
}
#ifdef _WIN32
inline bool isUNC(std::string_view path) noexcept {
    return path.size() >= 3 && dirSeparator(path[0]) && dirSeparator(path[1]) && !dirSeparator(path[2]);
}
inline std::size_t uncRootLength(std::string_view path) noexcept {
    std::size_t i = 3;
    while (i < path.size() && !dirSeparator(path[i])) ++i;
    if (i < path.size()) {
        auto j = i;
        do { ++j; } while (j < path.size() && dirSeparator(path[j]));
        if (j < path.size()) {
            do { ++j; } while (j < path.size() && !dirSeparator(path[j]));
            i = j;
        }
    }
    return i;
}
#endif
inline std::string_view baseName(std::string_view path) noexcept {
    auto p1 = path;
#ifdef _WIN32
    if (path.size() >= 2 && path[1] == ':') p1.remove_prefix(2);
    else if (isUNC(path)) p1.remove_prefix(uncRootLength(path));
#endif
    if (p1.empty()) {
#ifdef _WIN32
        if (isUNC(path)) return path.substr(0, 1);
#endif
        return {};
    }
    const auto p2 = trimSeparators(p1);
    if (p2.empty()) return p1.substr(0, 1);
    return p2.substr(afterSeparator(p2));
}
inline std::string_view dirName(std::string_view path) noexcept {
    if (path.empty()) return ".";
    const auto p = trimSeparators(path);
    if (p.empty()) return path.substr(0, 1);
#ifdef _WIN32
    if (isUNC(p) && uncRootLength(p) == p.size()) return p;
    if (p.size() == 2 && p[1] == ':' && path.size() > 2) return path.substr(0, 3);
#endif
    const auto after = afterSeparator(p);
    if (!after) return ".";
    const auto i = after - 1;
    if (!i) return p.substr(0, 1);
#ifdef _WIN32
    if (p[i] == ':' || p[i - 1] == ':') return p.substr(0, i + 1);
#endif
    return trimSeparators(p.substr(0, i));
}
[[noreturn]] inline void fileError(RetainedString name, std::uint32_t code, std::size_t line) {
    FileException error(std::move(name), code);
    error.file = RetainedString::fromStaticLiteral("std/file.d");
    error.line = line;
    throw error;
}
inline std::uint32_t getAttributes(std::string_view path) {
#ifdef _WIN32
    const TempCStringBuffer<wchar_t> name(path);
    const auto attributes = GetFileAttributesW(name.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const auto code = GetLastError();
        fileError(RetainedString(path), code, 2051);
    }
    return attributes;
#else
    const TempCStringBuffer<char> name(path);
    struct stat attributes;
    if (::stat(name.c_str(), &attributes) != 0) {
        const auto code = errno;
        fileError(RetainedString(path), static_cast<std::uint32_t>(code), 2064);
    }
    return static_cast<std::uint32_t>(attributes.st_mode);
#endif
}
inline bool isDirectory(std::string_view path) {
#ifdef _WIN32
    return (getAttributes(path) & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    return (getAttributes(path) & S_IFMT) == S_IFDIR;
#endif
}
inline bool ensureDirExists(std::string_view path) {
#ifdef _WIN32
    const TempCStringBuffer<wchar_t> name(path);
    if (CreateDirectoryW(name.c_str(), nullptr)) return true;
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        // The source lazy pathname.idup is evaluated before the default error code.
        const RetainedString copied(path);
        const auto code = GetLastError();
        fileError(copied, code, 3033);
    }
#else
    const TempCStringBuffer<char> name(path);
    if (::mkdir(name.c_str(), 0777) == 0) return true;
    if (errno != EEXIST && errno != EISDIR) {
        const auto code = errno;
        fileError(RetainedString(path), static_cast<std::uint32_t>(code), 3041);
    }
#endif
    if (!isDirectory(path)) {
        const RetainedString copied(path);
        const auto code = file_exception_detail::currentError();
        fileError(copied, code, 3043);
    }
    return false;
}
} // namespace directory_detail

inline RetainedString ngDirName(RetainedString path) {
    const auto trimmed = directory_detail::trimSeparators(path.view());
    if (path.empty() || (!trimmed.empty() && directory_detail::afterSeparator(trimmed) == 0))
        return RetainedString::fromStaticLiteral(".");
    const auto parent = directory_detail::dirName(path.view());
    // All remaining Source branches return a prefix retaining the original path.
    return path.subslice(0, parent.size());
}
inline bool ngIsFile(std::string_view path) {
#ifdef _WIN32
    return !directory_detail::isDirectory(path);
#else
    return (directory_detail::getAttributes(path) & S_IFMT) == S_IFREG;
#endif
}

inline void ngMkdirRecurse(std::string_view path) {
    const auto left = directory_detail::dirName(path);
    if (left.size() != path.size() && !ngFileExists(left)) ngMkdirRecurse(left);
    if (!directory_detail::baseName(path).empty()) directory_detail::ensureDirExists(path);
}

inline std::string ngGetCurrentDirectory() {
#ifdef _WIN32
    auto convert = [](const wchar_t* data, std::size_t length) {
        using Cursor = detail::UnicodeCursor<std::span<const wchar_t>, wchar_t>;
        Cursor values(std::span<const wchar_t>{data, length});
        std::string result;
        while (!values.empty()) {
            file_exception_detail::appendScalar(result, values.front());
            values.popFront();
        }
        return result;
    };
    std::array<wchar_t, 4096> buffer;
    const auto n = GetCurrentDirectoryW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (!n) directory_detail::fileError(RetainedString::fromStaticLiteral("getcwd"), GetLastError(), 3422);
    if (n < buffer.size()) return convert(buffer.data(), n);
    // The source checked DWORD count times wchar size fits this native size_t.
    static_assert(sizeof(std::size_t) > sizeof(DWORD));
    std::unique_ptr<wchar_t, decltype(&std::free)> heap(
        static_cast<wchar_t*>(std::malloc(static_cast<std::size_t>(n) * sizeof(wchar_t))), &std::free);
    const auto n2 = GetCurrentDirectoryW(n, heap.get());
    if (!n2 || n2 >= n)
        directory_detail::fileError(RetainedString::fromStaticLiteral("getcwd"), GetLastError(), 3435);
    return convert(heap.get(), n2);
#else
#ifdef __sun
    auto* raw = ::getcwd(nullptr, 4096);
    constexpr std::size_t sourceLine = 3444;
#else
    auto* raw = ::getcwd(nullptr, 0);
    constexpr std::size_t sourceLine = 3451;
#endif
    if (!raw) directory_detail::fileError(RetainedString::fromStaticLiteral("cannot get cwd"),
        static_cast<std::uint32_t>(errno), sourceLine);
    std::unique_ptr<char, decltype(&std::free)> memory(raw, &std::free);
    return std::string(raw, std::strlen(raw));
#endif
}
} // namespace nijigenerate::compat::phobos
