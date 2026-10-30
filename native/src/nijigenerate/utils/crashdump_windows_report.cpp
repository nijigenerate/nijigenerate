// Phobos std.file.writeImpl; Copyright The D Language Foundation 2007-2011.
// BSL-1.0; see port/licenses/phobos-psd-file.txt.
#include "crashdump_report.hpp"
#include "crashdump_internal.hpp"
#include "crashdump_windows_internal.hpp"
#include "crashdump_file_error.hpp"
#include "crashdump_file_name.hpp"
#include <nijigenerate/platform/environment.hpp>
#include <common/unicode_lowercase.hpp>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>

namespace nijigenerate::utils::crashdump::detail {
void ngWriteCrashReport(compat::RetainedStringSlice path, std::string_view content, bool append) {
    const CrashFileName<wchar_t> encoded(path);
    const auto file = ::CreateFileW(encoded.c_str(), GENERIC_WRITE, 0, nullptr,
        append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        failCrashFile(path, ::GetLastError(), append ? 873 : 885);
    if (append && ::SetFilePointer(file, 0, nullptr, FILE_END) == INVALID_SET_FILE_POINTER)
        failCrashFile(path, ::GetLastError(), 874);
    std::size_t sum = 0;
    while (sum != content.size()) {
        const auto count = static_cast<DWORD>(std::min<std::size_t>(content.size() - sum, std::size_t{1} << 30));
        DWORD written;
        ::WriteFile(file, content.data() + sum, count, &written, nullptr);
        if (written != count) break;
        sum += written;
    }
    // Match std.file.writeImpl's short-circuit close and exception sequence.
    if (!(sum == content.size() && ::CloseHandle(file)))
        failCrashFile(path, ::GetLastError(), 898);
}

void ngAppendFallbackReport(const std::function<compat::RetainedString()>& report) {
    // The nested default expression is evaluated before the outer TEMP lookup.
    const auto temporary = platform::ngGetEnvironmentVariable("TMP");
    auto directory = allocateCrashText([&] { return temporary ? *temporary : std::string("."); });
    const auto preferred = platform::ngGetEnvironmentVariable("TEMP");
    if (preferred) allocateCrashText([&] { directory = *preferred; });
    const auto path = ngJoinCrashPath(directory, "nijigenerate-crashdump.log");
    const auto timestamp = ngCurrentCrashTimestamp();
    const auto body = report();
    const auto formatted = allocateCrashText([&] {
        return std::string("\n=== ") + timestamp + " ===\n" + std::string(body.view());
    });
    const auto retainedPath = allocateCrashText([&] { return compat::RetainedString(path); });
    ngWriteCrashReport(retainedPath, formatted, true);
}
}
