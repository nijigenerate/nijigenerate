#include "crashdump.hpp"
#include "crashdump_internal.hpp"
#include "crashdump_windows_internal.hpp"
#include <nijigenerate/compat/phobos_windows_exception.hpp>
#include <nijigenerate/compat/phobos_temp_cstring.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace nijigenerate::utils::crashdump {

compat::RetainedStringSlice getCrashDumpDir() {
    if (!detail::crashDumpDirOverride.empty()) return detail::crashDumpDirOverride;
    return detail::getDesktopDir();
}

void mkdirCrashDumpDir() {
    const auto directory = getCrashDumpDir();
    if (compat::phobos::ngFileExists(directory.view())) return;
    const compat::phobos::TempCStringBuffer<wchar_t> path{directory.view()};
    if (!::CreateDirectoryW(path.c_str(), nullptr)) {
        const auto error = ::GetLastError();
        throw compat::WindowsException(error, directory.retainedString(),
            compat::RetainedString::fromStaticLiteral("std/file.d"), 2971);
    }
}

} // namespace nijigenerate::utils::crashdump
