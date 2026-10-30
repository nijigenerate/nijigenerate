#pragma once
// Used const(char)[] std.file.rename body, D Language Foundation 2007-2011.
// BSL-1.0: native/port/licenses/phobos-psd-file.txt.
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif
#include "retained_string_slice.hpp"
#include "phobos_file_exception.hpp"
#include "phobos_temp_cstring.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <string>
#include <utility>
#if defined(_WIN32)
#include <Windows.h>
#endif

namespace nijigenerate::compat::phobos {
inline void ngFileRename(RetainedStringSlice from, RetainedStringSlice to) {
#if defined(_WIN32)
    const TempCStringBuffer<wchar_t> source(from.view());
    const TempCStringBuffer<wchar_t> destination(to.view());
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        // Narrow descriptors alias the original counted names. Null C paths use
        // the original diagnostic fallback; nonnull empty paths stay empty.
        std::string diagnostic("Attempting to rename file ");
        if (from.isNull()) diagnostic.append("(null)");
        else if (!from.empty()) diagnostic.append(from.data(), from.size());
        diagnostic.append(" to ");
        if (to.isNull()) diagnostic.append("(null)");
        else if (!to.empty()) diagnostic.append(to.data(), to.size());
        const RetainedString name(std::move(diagnostic));
        // The source obtains the OS error after its failure-only diagnostic.
        throw FileException(name);
    }
#else
    const TempCStringBuffer<char> source(from.view());
    const TempCStringBuffer<char> destination(to.view());
    if (std::rename(source.c_str(), destination.c_str()) != 0) {
        const auto code = static_cast<std::uint32_t>(errno);
        // cenforce's empty-name fallback idup yields a null empty descriptor.
        const auto name = to.empty() ? RetainedString{} : to.retainedString();
        throw FileException(name, code);
    }
#endif
}
} // namespace nijigenerate::compat::phobos
