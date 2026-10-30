#pragma once
// Used std.file.exists body. Copyright The D Language Foundation 2006-2013.
// BSL-1.0; see port/licenses/phobos-psd-file.txt.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include "phobos_temp_cstring.hpp"
#if defined(_WIN32)
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

namespace nijigenerate::compat::phobos {
inline bool ngFileExists(std::string_view path) {
#if defined(_WIN32)
    const TempCStringBuffer<wchar_t> name(path);
    return GetFileAttributesW(name.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    const TempCStringBuffer<char> name(path);
    struct stat attributes;
    return ::lstat(name.c_str(), &attributes) == 0;
#endif
}
} // namespace nijigenerate::compat::phobos
