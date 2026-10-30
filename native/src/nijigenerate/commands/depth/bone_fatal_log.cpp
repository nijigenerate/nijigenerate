// Independent Phobos behavior adaptation; notices and BSL-1.0 are preserved
// in native/port/licenses/phobos-depthbone-fatal-log.txt.
#include "detail/bone_flush.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>
#include <algorithm>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace nijigenerate::commands::depth::bone::detail {
namespace {

[[noreturn]] void fatalLogFailure() {
    throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral("Depth bone fatal log failure"));
}

void appendDecimal(std::string& text, unsigned value, std::size_t width) {
    char buffer[32];
    auto end = std::to_chars(buffer, buffer + sizeof(buffer), value).ptr;
    auto length = static_cast<std::size_t>(end - buffer);
    if (length < width) text.append(width - length, '0');
    text.append(buffer, end);
}

std::string localTimestamp() {
    std::int64_t year;
    unsigned month, day, hour, minute, second, fraction;
#if defined(_WIN32)
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);
    SYSTEMTIME utc, local;
    if (!FileTimeToSystemTime(&fileTime, &utc)
            || !SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) fatalLogFailure();
    year = local.wYear; month = local.wMonth; day = local.wDay;
    hour = local.wHour; minute = local.wMinute; second = local.wSecond;
    const auto ticks = (static_cast<std::uint64_t>(fileTime.dwHighDateTime) << 32)
        | fileTime.dwLowDateTime;
    fraction = static_cast<unsigned>(ticks % 10000000);
#else
    timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) fatalLogFailure();
    std::tm local;
    if (!localtime_r(&now.tv_sec, &local)) fatalLogFailure();
    year = static_cast<std::int64_t>(local.tm_year) + 1900;
    month = static_cast<unsigned>(local.tm_mon + 1); day = static_cast<unsigned>(local.tm_mday);
    hour = static_cast<unsigned>(local.tm_hour); minute = static_cast<unsigned>(local.tm_min);
    second = static_cast<unsigned>(local.tm_sec);
    fraction = static_cast<unsigned>(now.tv_nsec / 100);
#endif
    std::string result;
    // SysTime's ISO year keeps its sign; the POSIX local clock can precede year zero.
    if (year < 0) {
        result.push_back('-');
        appendDecimal(result, static_cast<unsigned>(-year), 4);
    } else {
        appendDecimal(result, static_cast<unsigned>(year), 4);
    }
    appendDecimal(result, month, 2); appendDecimal(result, day, 2);
    result.push_back('T');
    appendDecimal(result, hour, 2); appendDecimal(result, minute, 2); appendDecimal(result, second, 2);
    if (fraction) {
        result.push_back('.');
        appendDecimal(result, fraction, 7);
        while (result.back() == '0') result.pop_back();
    }
    return result;
}

std::filesystem::path fatalLogPath() {
#if defined(_WIN32)
    SetLastError(NO_ERROR);
    const auto required = GetEnvironmentVariableW(L"TEMP", nullptr, 0);
    std::wstring directory;
    if (required == 0) {
        const auto error = GetLastError();
        if (error == ERROR_ENVVAR_NOT_FOUND) directory = L".";
        else if (error != NO_ERROR) fatalLogFailure();
    } else if (required > 1) {
        directory.resize(required);
        for (;;) {
            const auto read = GetEnvironmentVariableW(L"TEMP", directory.data(),
                static_cast<DWORD>(directory.size()));
            if (read == 0) {
                const auto error = GetLastError();
                if (error == ERROR_ENVVAR_NOT_FOUND) directory = L".";
                else if (error == NO_ERROR) directory.clear();
                else fatalLogFailure();
                break;
            }
            if (read < directory.size()) { directory.resize(read); break; }
            directory.resize(read);
        }
    }
    // The original environment provider converts UTF16 to UTF8 and rejects
    // malformed pairs before building a path; wide native paths must too.
    for (std::size_t i = 0; i < directory.size(); ++i) {
        const auto unit = static_cast<unsigned>(directory[i]);
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            if (++i == directory.size()) fatalLogFailure();
            const auto low = static_cast<unsigned>(directory[i]);
            if (low < 0xDC00 || low > 0xDFFF) fatalLogFailure();
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) fatalLogFailure();
    }
    return std::filesystem::path(directory) / L"nijigenerate-depthbone-gpu.log";
#else
    const auto directory = std::getenv("TEMP");
    return std::filesystem::path(directory ? directory : ".") / "nijigenerate-depthbone-gpu.log";
#endif
}

void appendFatalLog(const std::filesystem::path& path, std::string_view bytes) {
#if defined(_WIN32)
    auto handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) fatalLogFailure();
    if (SetFilePointer(handle, 0, nullptr, FILE_END) == INVALID_SET_FILE_POINTER) fatalLogFailure();
    std::size_t written = 0;
    while (written != bytes.size()) {
        const auto count = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - written, 1u << 30));
        DWORD actual = 0;
        WriteFile(handle, bytes.data() + written, count, &actual, nullptr);
        if (actual != count) break;
        written += actual;
    }
    // Selected Phobos uses this short-circuit close, including failure paths.
    if (!(written == bytes.size() && CloseHandle(handle))) fatalLogFailure();
#else
    const auto descriptor = open(path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666);
    if (descriptor == -1) fatalLogFailure();
    std::size_t written = 0;
    while (written != bytes.size()) {
        const auto count = std::min<std::size_t>(bytes.size() - written, 1u << 30);
        const auto actual = write(descriptor, bytes.data() + written, count);
        if (actual != static_cast<ssize_t>(count)) {
            close(descriptor);
            fatalLogFailure();
        }
        written += count;
    }
    if (close(descriptor) != 0) fatalLogFailure();
#endif
}

} // namespace

void writeDepthBoneGpuFatalLog(compat::RetainedStringSlice message) {
    try {
        auto path = fatalLogPath();
        auto stamp = localTimestamp();
        std::array<core::translation::TypedFormatArgument, 2> arguments{
            core::translation::ngFormatArgument(std::move(stamp)),
            core::translation::ngFormatArgument(message)};
        auto bytes = core::translation::ngFormatTyped("[%s] %s\n", arguments);
        appendFatalLog(path, bytes);
    } catch (compat::ThrowableException&) {
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}

} // namespace nijigenerate::commands::depth::bone::detail
