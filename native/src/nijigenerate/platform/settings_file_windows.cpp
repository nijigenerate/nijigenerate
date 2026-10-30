// Used Phobos std.file read body, D Language Foundation 2007-2011.
// BSL-1.0: native/port/licenses/phobos-psd-file.txt.
#include "settings_file.hpp"
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_file_exception.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace nijigenerate::platform {
bool ngSettingsFileExists(std::string_view name) {
    return compat::phobos::ngFileExists(name);
}
std::string ngSettingsReadFileBytes(std::string_view name) {
    const compat::phobos::TempCStringBuffer<wchar_t> path(name);
    std::string data;
    const auto fail = [&] {
        const auto code = GetLastError();
        throw compat::FileException(compat::RetainedString(name), code);
    };
    const auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) fail();
    // Close failure has the same scope-exit priority as the original std.file.read.
    compat::ngRunWithScopeExit([&] {
        DWORD high = 0;
        const DWORD low = GetFileSize(handle, &high);
        if (low == INVALID_FILE_SIZE) fail();
        const auto fileSize = (static_cast<std::uint64_t>(high) << 32) | low;
        const auto size = (std::min<std::uint64_t>)(fileSize, (std::numeric_limits<std::size_t>::max)());
        data.resize(static_cast<std::size_t>(size));
        for (std::size_t offset = 0; offset < data.size();) {
            const auto chunk = static_cast<DWORD>(std::min<std::size_t>(data.size() - offset, 0xffff0000));
            DWORD actual = 0;
            if (!ReadFile(handle, data.data() + offset, chunk, &actual, nullptr) || actual != chunk) fail();
            offset += actual;
        }
    }, [&] { if (!CloseHandle(handle)) fail(); });
    return data;
}
}
