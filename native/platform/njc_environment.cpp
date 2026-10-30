// Adapted from installed LDC 1.41 Phobos std.process and std.windows.syserror.
// Copyright (c) 2013 Lars Tandle Kyllingstad, Steven Schveighoffer and Vladimir
// Panteleev; Copyright The D Language Foundation 2006-2013 (syserror),
// 2000-2012 (std.utf). isWhiteGen data by Dmitry Olshansky.
// Boost Software License 1.0; see ../src/njc/detail/phobos.LICENSE.
#include <njc/detail/environment_windows.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <array>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace njc::config::detail {
using nijigenerate::compat::RetainedString;

#if defined(_WIN32)
namespace {

std::uint32_t decodeWide(std::wstring_view value, std::size_t& index) {
    const auto first = static_cast<std::uint16_t>(value[index++]);
    if (first >= 0xd800 && first <= 0xdbff) {
        if (index == value.size())
            throw ConversionUtfException(first, "surrogate UTF-16 high value past end of string");
        const auto second = static_cast<std::uint16_t>(value[index]);
        if (second < 0xdc00 || second > 0xdfff)
            throw ConversionUtfException(first, "surrogate UTF-16 low value out of range");
        ++index;
        return ((std::uint32_t(first) - 0xd7c0) << 10) + second - 0xdc00;
    }
    if (first >= 0xdc00 && first <= 0xdfff)
        throw ConversionUtfException(first, "unpaired surrogate UTF-16 value");
    return first;
}

std::size_t encodeUtf8(std::uint32_t scalar, std::array<char, 4>& bytes) {
    if (scalar < 0x80) { bytes[0] = static_cast<char>(scalar); return 1; }
    if (scalar < 0x800) {
        bytes[0] = static_cast<char>(0xc0 | (scalar >> 6));
        bytes[1] = static_cast<char>(0x80 | (scalar & 0x3f));
        return 2;
    }
    if (scalar < 0x10000) {
        bytes[0] = static_cast<char>(0xe0 | (scalar >> 12));
        bytes[1] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3f));
        bytes[2] = static_cast<char>(0x80 | (scalar & 0x3f));
        return 3;
    }
    bytes[0] = static_cast<char>(0xf0 | (scalar >> 18));
    bytes[1] = static_cast<char>(0x80 | ((scalar >> 12) & 0x3f));
    bytes[2] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3f));
    bytes[3] = static_cast<char>(0x80 | (scalar & 0x3f));
    return 4;
}

std::string wideToUtf8(std::wstring_view value) {
    std::string result;
    for (std::size_t index = 0; index < value.size();) {
        std::array<char, 4> bytes;
        const auto length = encodeUtf8(decodeWide(value, index), bytes);
        ngConfigAllocation([&] { result.append(bytes.data(), length); });
    }
    return result;
}

bool sameWideValue(std::wstring_view value, std::string_view cached) {
    std::size_t offset = 0;
    for (std::size_t index = 0; index < value.size();) {
        if (offset == cached.size()) return false;
        std::array<char, 4> bytes;
        const auto length = encodeUtf8(decodeWide(value, index), bytes);
        for (std::size_t i = 0; i < length; ++i) {
            if (offset == cached.size() || bytes[i] != cached[offset++]) return false;
        }
    }
    return offset == cached.size();
}

Text cachedEnvironmentValue(std::wstring_view value) {
    static thread_local Text lastResult;
    if (value.empty()) lastResult = RetainedString::emptyLiteral();
    else if (!sameWideValue(value, lastResult.view())) {
        auto bytes = wideToUtf8(value);
        lastResult = ngConfigAllocation([&] { return RetainedString(std::move(bytes)); });
    }
    return lastResult;
}

// Exact current Phobos isWhiteGen set; all its members fit one UTF-16 unit.
bool unicodeWhite(std::uint16_t value) {
    return (value >= 9 && value < 14) || value == 32 || value == 133 || value == 160 || value == 5760 ||
        (value >= 8192 && value < 8203) || (value >= 8232 && value < 8234) ||
        value == 8239 || value == 8287 || value == 12288;
}

std::string windowsErrorMessage(std::uint32_t code) {
    if (code == 0) return {};
    wchar_t* raw = nullptr;
    const auto length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, reinterpret_cast<LPWSTR>(&raw), 0, nullptr);
    struct Release {
        wchar_t* buffer;
        ~Release() { if (buffer) LocalFree(buffer); }
    } release{raw};
    if (raw) {
        auto text = std::wstring_view(raw, length);
        while (!text.empty() && unicodeWhite(static_cast<std::uint16_t>(text.front()))) text.remove_prefix(1);
        while (!text.empty() && unicodeWhite(static_cast<std::uint16_t>(text.back()))) text.remove_suffix(1);
        try {
            auto message = wideToUtf8(text);
            return ngConfigAllocation([&] { return message + " (error " + std::to_string(code) + ")"; });
        }
        catch (const ConversionUtfException&) {}
    }
    return ngConfigAllocation([&] { return "Error " + std::to_string(code); });
}

} // namespace
#endif

Text ngEnvironmentValue(const char* name) {
    if (!name) return RetainedString::emptyLiteral();
#if defined(_WIN32)
    // The authored consumers use nonnull ASCII literal keys only.
    const auto key = ngConfigAllocation([&] {
        return std::wstring(name, name + std::char_traits<char>::length(name));
    });
    SetLastError(ERROR_SUCCESS);
    const auto required = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
    if (required == 0) {
        const auto error = GetLastError();
        if (error == ERROR_ENVVAR_NOT_FOUND) return RetainedString::emptyLiteral();
        if (error != ERROR_SUCCESS) throw EnvironmentWindowsException(error);
    }
    if (required <= 1) return cachedEnvironmentValue({});
    auto buffer = ngConfigAllocation([&] { return std::vector<wchar_t>(required); });
    for (;;) {
        const auto length = GetEnvironmentVariableW(key.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            const auto error = GetLastError();
            if (error == ERROR_SUCCESS) return cachedEnvironmentValue({});
            if (error == ERROR_ENVVAR_NOT_FOUND) return RetainedString::emptyLiteral();
            throw EnvironmentWindowsException(error);
        }
#ifndef NDEBUG
        if (length == buffer.size())
            throw nijigenerate::compat::AssertError{"impossible according to msft docs", __FILE__, __LINE__};
#endif
        if (length < buffer.size()) return cachedEnvironmentValue(std::wstring_view(buffer.data(), length));
        ngConfigAllocation([&] { buffer.resize(length); });
    }
#else
    const auto value = std::getenv(name);
    if (!value) return RetainedString::emptyLiteral();
    // Phobos compares same-width char arrays as bytes and copies only changes.
    // Missing values leave the last result intact; present empty values reset it.
    static thread_local Text lastResult;
    const auto bytes = std::string_view(value);
    if (bytes.empty()) lastResult = RetainedString::emptyLiteral();
    else if (bytes != lastResult.view()) {
        lastResult = ngConfigAllocation([&] { return RetainedString(bytes); });
    }
    return lastResult;
#endif
}

} // namespace njc::config::detail

#if defined(_WIN32)
namespace njc::config {
EnvironmentWindowsException::EnvironmentWindowsException(std::uint32_t code)
    : ConfigException([&] {
        auto message = detail::windowsErrorMessage(code);
        return detail::ngConfigAllocation([&] {
            return nijigenerate::compat::RetainedString(std::move(message));
        });
    }(), {}, 0), code_(code) {}
} // namespace njc::config
#endif
