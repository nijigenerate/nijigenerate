#include "environment.hpp"
#include "detail/path_allocation.hpp"
// Used std.process.environment get/getImpl/cachedToString bodies, BSL-1.0.
// Copyright (c) 2013 Lars Tandle Kyllingstad, Steven Schveighoffer and
// Vladimir Panteleev. See port/licenses/phobos-psd-file.txt.
#include <nijigenerate/compat/phobos_temp_cstring.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/compat/phobos_windows_exception.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>
#include <utility>

namespace nijigenerate::platform {
namespace {
// The actual cachedToString instantiation retains the last result per OS thread.
thread_local compat::RetainedString lastEnvironmentResult;
#ifdef _WIN32
compat::RetainedString cachedResult(std::span<const wchar_t> value) {
    if (value.empty()) {
        lastEnvironmentResult = compat::RetainedString::emptyLiteral();
        return lastEnvironmentResult;
    }
    using WideCursor = compat::phobos::detail::UnicodeCursor<std::span<const wchar_t>, wchar_t>;
    using ByteCursor = compat::phobos::detail::UnicodeCursor<std::span<const char>, char>;
    WideCursor left(value);
    ByteCursor right(std::span<const char>{lastEnvironmentResult.data(), lastEnvironmentResult.size()});
    bool equal = true;
    // Match std.algorithm.equal's differing-character-width equalLoop order.
    while (!left.empty()) {
        if (right.empty()) { equal = false; break; }
        const auto leftValue = left.front();
        const auto rightValue = right.front();
        if (leftValue != rightValue) { equal = false; break; }
        right.popFront();
        left.popFront();
    }
    if (equal && !right.empty()) equal = false;
    if (!equal) {
        WideCursor cursor(value);
        std::string bytes;
        while (!cursor.empty()) {
            const auto scalar = cursor.front();
            detail::ngPathAllocation([&] { compat::file_exception_detail::appendScalar(bytes, scalar); });
            cursor.popFront();
        }
        lastEnvironmentResult = detail::ngPathAllocation([&] { return compat::RetainedString(std::move(bytes)); });
    }
    return lastEnvironmentResult;
}
#else
compat::RetainedString cachedResult(std::string_view value) {
    if (value.empty()) lastEnvironmentResult = compat::RetainedString::emptyLiteral();
    else if (value != lastEnvironmentResult.view())
        lastEnvironmentResult = detail::ngPathAllocation([&] { return compat::RetainedString(value); });
    return lastEnvironmentResult;
}
#endif
}

compat::RetainedString ngGetEnvironmentVariableRetained(const char* name) {
    if (!name) return {};
#ifdef _WIN32
    const compat::phobos::TempCStringBuffer<wchar_t> nativeName{std::string_view(name)};
    SetLastError(NO_ERROR);
    const auto length = GetEnvironmentVariableW(nativeName.c_str(), nullptr, 0);
    if (!length) {
        const auto error = GetLastError();
        if (error == ERROR_ENVVAR_NOT_FOUND) return {};
        if (error != NO_ERROR) throw compat::WindowsException(error);
    }
    if (length <= 1) return cachedResult({});
    auto buffer = detail::ngPathAllocation([&] { return std::vector<wchar_t>(length); });
    for (;;) {
        // The source clears the error once, before the first size query.
        const auto read = GetEnvironmentVariableW(nativeName.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!read) {
            const auto error = GetLastError();
            if (error == NO_ERROR) return cachedResult({});
            if (error == ERROR_ENVVAR_NOT_FOUND) return {};
            throw compat::WindowsException(error);
        }
        if (read == buffer.size())
            throw compat::ngStaticError<::nicxlive::core::d_runtime::AssertError>(
                "impossible according to msft docs", "std/process.d", 479);
        if (read < buffer.size()) return cachedResult(std::span<const wchar_t>{buffer.data(), read});
        detail::ngPathAllocation([&] { buffer.resize(read); });
    }
#else
    const compat::phobos::TempCStringBuffer<char> nativeName{std::string_view(name)};
    const auto* value = std::getenv(nativeName.c_str());
    if (!value) return {};
    return cachedResult(std::string_view(value, std::strlen(value)));
#endif
}

std::optional<std::string> ngGetEnvironmentVariable(const char* name) {
    const auto value = ngGetEnvironmentVariableRetained(name);
    if (value.isNull()) return std::nullopt;
    return detail::ngPathAllocation([&] { return std::string(value.view()); });
}

compat::RetainedString ngSetEnvironmentVariableRetained(
    compat::RetainedString value, compat::RetainedString name) {
    // Used std.process.environment.opIndexAssign and its remove branch.
#ifdef _WIN32
    const compat::phobos::TempCStringBuffer<wchar_t> nativeName{name.view()};
    const compat::phobos::TempCStringBuffer<wchar_t> nativeValue{value.view()};
    if (!SetEnvironmentVariableW(nativeName.c_str(), nativeValue.c_str()))
        throw compat::WindowsException(GetLastError(), {},
            compat::RetainedString::fromStaticLiteral("std/process.d"), 280);
#else
    if (value.isNull()) {
        const compat::phobos::TempCStringBuffer<char> nativeName{name.view()};
        (void)::unsetenv(nativeName.c_str());
        return value;
    }
    {
        const compat::phobos::TempCStringBuffer<char> nativeName{name.view()};
        const compat::phobos::TempCStringBuffer<char> nativeValue{value.view()};
        if (::setenv(nativeName.c_str(), nativeValue.c_str(), 1) != -1) return value;
    }
    if (errno == EINVAL) {
        const auto message = detail::ngPathAllocation([&] {
            std::string bytes("Invalid environment variable name: '");
            if (!name.empty()) bytes.append(name.data(), name.size());
            bytes.push_back('\'');
            return compat::RetainedString(std::move(bytes));
        });
        throw compat::ThrowableException(message,
            compat::RetainedString::fromStaticLiteral("std/process.d"), 271);
    }
    // enforce duplicates its lazy message before ErrnoException reads errno.
    const auto message = detail::ngPathAllocation([] {
        return compat::RetainedString(std::string("Failed to add environment variable"));
    });
    throw ::nijigenerate::providers::mcp::stdio::ErrnoException(message, errno,
        compat::RetainedString::fromStaticLiteral("std/process.d"), 273);
#endif
    return value;
}
} // namespace nijigenerate::platform
