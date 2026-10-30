// Required shallow DirIteratorImpl/std.file.exists path from Phobos (BSL-1.0).
// Copyright The D Language Foundation 2007-. See native/port/licenses/phobos-psd-file.txt.
#include "nijigenerate/platform/locale_scan.hpp"
#include "nijigenerate/platform/locale_catalog_entry_name.hpp"
#include "nijigenerate/platform/path_policy.hpp"
#include "nijigenerate/core/i18n_case.hpp"
#include "nijigenerate/compat/phobos_unicode_exception.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace nijigenerate::platform {
namespace {
template<class Operation> decltype(auto) allocateScanStorage(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class String, class Character> void appendScanUnit(String& text, Character unit) {
    allocateScanStorage([&] { text.push_back(unit); });
}
#if defined(_WIN32)
std::wstring wideName(std::string_view text) {
    std::wstring result;
    core::i18n::detail::LocaleCaseCursor cursor{text};
    while (!cursor.empty()) {
        auto scalar = cursor.take();
        if (scalar <= 0xffff) appendScanUnit(result, static_cast<wchar_t>(scalar));
        else {
            scalar -= 0x10000;
            appendScanUnit(result, static_cast<wchar_t>(0xd800 + (scalar >> 10)));
            appendScanUnit(result, static_cast<wchar_t>(0xdc00 + (scalar & 0x3ff)));
        }
    }
    return result;
}
std::string narrowName(const wchar_t* text, std::size_t length) {
    // std.conv.to!string uses Appender.put/decodeFront with strict UTF-16 decoding.
    const auto fail = [](std::uint32_t unit, const char* message) {
        const auto text = allocateScanStorage([&] { return compat::RetainedString(message); });
        compat::UTFException error{text};
        const std::uint32_t sequence[]{unit};
        error.setSequence(sequence);
        throw error;
    };
    std::string result;
    for (std::size_t i = 0; i < length; ++i) {
        auto scalar = static_cast<std::uint32_t>(text[i]);
        if (scalar >= 0xd800 && scalar <= 0xdbff) {
            if (i + 1 == length) fail(scalar, "surrogate UTF-16 high value past end of string");
            const auto low = static_cast<std::uint32_t>(text[i + 1]);
            if (low < 0xdc00 || low > 0xdfff) fail(scalar, "surrogate UTF-16 low value out of range");
            scalar = ((scalar - 0xd7c0) << 10) + low - 0xdc00;
            ++i;
        } else if (scalar >= 0xdc00 && scalar <= 0xdfff) {
            fail(scalar, "unpaired surrogate UTF-16 value");
        }
        if (scalar < 0x80) appendScanUnit(result, static_cast<char>(scalar));
        else if (scalar < 0x800) {
            appendScanUnit(result, static_cast<char>(0xc0 | (scalar >> 6)));
            appendScanUnit(result, static_cast<char>(0x80 | (scalar & 0x3f)));
        } else if (scalar < 0x10000) {
            appendScanUnit(result, static_cast<char>(0xe0 | (scalar >> 12)));
            appendScanUnit(result, static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
            appendScanUnit(result, static_cast<char>(0x80 | (scalar & 0x3f)));
        } else {
            appendScanUnit(result, static_cast<char>(0xf0 | (scalar >> 18)));
            appendScanUnit(result, static_cast<char>(0x80 | ((scalar >> 12) & 0x3f)));
            appendScanUnit(result, static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
            appendScanUnit(result, static_cast<char>(0x80 | (scalar & 0x3f)));
        }
    }
    return result;
}
bool whitespace(wchar_t value) {
    return (value >= 9 && value <= 13) || value == 32 || value == 0x85 || value == 0xa0
        || value == 0x1680 || (value >= 0x2000 && value <= 0x200a) || value == 0x2028
        || value == 0x2029 || value == 0x202f || value == 0x205f || value == 0x3000;
}
std::string systemMessage(std::uint32_t code, bool* translated = nullptr) {
    wchar_t* text = nullptr;
    const auto count = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
        reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    struct Guard { wchar_t* p; ~Guard() { if (p) LocalFree(p); } } guard{text};
    if (translated) *translated = text != nullptr;
    if (!text) return allocateScanStorage([&] { return "Error " + std::to_string(code); });
    std::size_t first = 0, last = count;
    while (first < last && whitespace(text[first])) ++first;
    while (first < last && whitespace(text[last - 1])) --last;
    return narrowName(text + first, last - first);
}
#else
std::string systemMessage(std::uint32_t code) {
    const auto text = std::strerror(static_cast<int>(code));
    return allocateScanStorage([&] { return std::string(text); });
}
#endif
}

[[noreturn]] void throwLocaleScanFileError(compat::RetainedString name, std::uint32_t code) {
    const auto message = systemMessage(code);
    auto text = allocateScanStorage([&] {
        return name.isNull() ? std::string("(null)") : std::string(name.view());
    });
    if (!message.empty()) allocateScanStorage([&] { text += ": " + message; });
    throw LocaleScanFileError(code, std::move(text));
}

void visitLocaleCatalogEntries(compat::RetainedString path,
    const std::function<void(compat::RetainedString)>& consumer) {
#if defined(_WIN32)
    const auto directory = wideName(path.view());
    if (GetFileAttributesW(path.isNull() ? nullptr : directory.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    const auto searchPath = allocateScanStorage([&] {
        return ngJoinRelativePath(std::string(path.view()), "*.*");
    });
    const auto pattern = wideName(searchPath);
    WIN32_FIND_DATAW data{};
    const auto handle = FindFirstFileW(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) throwLocaleScanFileError(path, GetLastError());
    struct Guard { HANDLE value; ~Guard() { FindClose(value); } } guard{handle};
    do {
        data.cFileName[MAX_PATH - 1] = 0;
        const auto filename = allocateScanStorage([&] { return std::wstring(data.cFileName); });
        if (filename == L"." || filename == L"..") continue;
        const auto leaf = narrowName(filename.data(), filename.size());
        if (!localeMoFilenameMatches(leaf)) continue;
        const auto entry = allocateScanStorage([&] {
            return compat::RetainedString(ngJoinRelativePath(std::string(path.view()), leaf));
        });
        consumer(entry);
        // The source treats any FindNextFile failure as end, without inspecting last error.
    } while (FindNextFileW(handle, &data));
#else
    const auto directory = allocateScanStorage([&] { return std::string(path.view()); });
    struct stat attributes;
    if (::stat(path.isNull() ? nullptr : directory.c_str(), &attributes) != 0) return;
    const auto handle = ::opendir(directory.empty() ? "." : directory.c_str());
    if (!handle) throwLocaleScanFileError(path, static_cast<std::uint32_t>(errno));
    struct Guard { DIR* value; ~Guard() { ::closedir(value); } } guard{handle};
    while (auto* data = ::readdir(handle)) {
        const std::string_view leaf(data->d_name);
        if (leaf == "." || leaf == ".." || !localeMoFilenameMatches(leaf)) continue;
        const auto entry = allocateScanStorage([&] {
            return compat::RetainedString(ngJoinRelativePath(directory, leaf));
        });
        consumer(entry);
    }
#endif
}

compat::RetainedString localeExecutableDirectory() {
    std::string path;
#if defined(_WIN32)
    auto buffer = allocateScanStorage([] { return std::vector<wchar_t>(MAX_PATH); });
    for (;;) {
        const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!length) {
            const auto code = GetLastError();
            bool translated = false;
            auto message = code ? systemMessage(code, &translated) : std::string{};
            if (code && translated) allocateScanStorage([&] {
                message += " (error " + std::to_string(code) + ")";
            });
            const auto text = code ? allocateScanStorage([&] {
                return compat::RetainedString(std::move(message));
            }) : compat::RetainedString{};
            throw LocalePathWindowsError(code, text);
        }
        if (length != buffer.size()) { path = narrowName(buffer.data(), length); break; }
        allocateScanStorage([&] { buffer.resize(buffer.size() * 2); });
    }
#elif defined(__linux__)
    auto buffer = allocateScanStorage([] { return std::vector<char>(2048); });
    for (int attempt = 0; attempt <= 10; ++attempt) {
        const auto length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length == -1) throwLocaleScanFileError(compat::RetainedString::fromStaticLiteral("/proc/self/exe"),
            static_cast<std::uint32_t>(errno));
        if (static_cast<std::size_t>(length) <= buffer.size() - 6) {
            allocateScanStorage([&] { path.assign(buffer.data(), static_cast<std::size_t>(length)); });
            break;
        }
        if (attempt == 10) {
            auto message = allocateScanStorage([] {
                return std::string("/proc/self/exe: Path is too long to read.");
            });
            throw LocaleScanFileError(0, std::move(message));
        }
        allocateScanStorage([&] { buffer.resize(buffer.size() * 3 / 2); });
    }
#elif defined(__APPLE__)
    std::uint32_t size{};
    _NSGetExecutablePath(nullptr, &size);
    auto buffer = allocateScanStorage([&] { return std::vector<char>(size); });
    _NSGetExecutablePath(buffer.data(), &size);
    char* absolute = ::realpath(buffer.data(), nullptr);
    if (!absolute) {
        const auto code = static_cast<std::uint32_t>(errno);
        throw LocalePathErrnoError(code, systemMessage(code));
    }
    struct Guard { char* p; ~Guard() { std::free(p); } } guard{absolute};
    allocateScanStorage([&] { path = absolute; });
#else
#error Map the actual std.file.thisExePath provider for this target.
#endif
    const auto separator = path.find_last_of(
#if defined(_WIN32)
        "/\\"
#else
        "/"
#endif
    );
    const std::size_t end = separator == 0 ? 1 :
#if defined(_WIN32)
        separator == 2 && path.size() >= 3 && path[1] == ':' ? 3 :
#endif
        separator;
    const auto retained = allocateScanStorage([&] { return compat::RetainedString(std::move(path)); });
    return end == std::string::npos ? compat::RetainedString::fromStaticLiteral(".") : retained.subslice(0, end);
}

compat::RetainedString localePlatformScanDirectory() {
#if defined(_WIN32)
    const auto executable = localeExecutableDirectory();
    return allocateScanStorage([&] {
        return compat::RetainedString(ngJoinRelativePath(std::string(executable.view()), "i18n"));
    });
#elif defined(__APPLE__)
    const auto executable = localeExecutableDirectory();
    return allocateScanStorage([&] {
        return compat::RetainedString(ngJoinRelativePath(std::string(executable.view()), "../Resources/i18n"));
    });
#else
    return {};
#endif
}
}
