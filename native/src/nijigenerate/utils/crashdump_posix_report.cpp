// Phobos std.file.writeImpl (std/file.d 830-858), BSL-1.0.
// Copyright The D Language Foundation 2007-2011.
// See port/licenses/phobos-psd-file.txt for the shared file-provider license.
#include "crashdump_report.hpp"
#include "crashdump_internal.hpp"
#include "crashdump_file_error.hpp"
#include "crashdump_file_name.hpp"
#include <nijigenerate/platform/environment.hpp>
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace nijigenerate::utils::crashdump::detail {
void ngWriteCrashReport(compat::RetainedStringSlice path, std::string_view content, bool append) {
    // The actual UTF-8 tempCString boundary preserves counted source bytes and
    // a null pointer; POSIX open consumes the first native terminator.
    const CrashFileName<char> native(path);
    const auto mode = O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC);
    const auto fd = ::open(native.c_str(), mode, 0666);
    if (fd == -1) failCrashFile(path, static_cast<std::uint32_t>(errno), 840);
    try {
        std::size_t sum = 0;
        while (sum != content.size()) {
            const auto count = std::min(content.size() - sum, std::size_t{1} << 30);
            const auto written = ::write(fd, content.data() + sum, count);
            if (written != static_cast<ssize_t>(count)) break;
            sum += static_cast<std::size_t>(written);
        }
        if (sum != content.size()) failCrashFile(path, static_cast<std::uint32_t>(errno), 854);
    } catch (...) {
        // Source scope(failure) closes after constructing the actual failure.
        (void)::close(fd);
        throw;
    }
    if (::close(fd) != 0) failCrashFile(path, static_cast<std::uint32_t>(errno), 856);
}

void ngAppendFallbackReport(const std::function<compat::RetainedString()>& report) {
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
} // namespace nijigenerate::utils::crashdump::detail
