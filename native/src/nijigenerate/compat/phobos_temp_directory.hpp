#pragma once
// Used std.file.tempDir body, selected Phobos std/file.d5356-5417. BSL-1.0.
#include "phobos_file_directory.hpp"
#include "phobos_file_exists.hpp"
#include "array_range_error.hpp"
#include "out_of_memory_error.hpp"
#include <array>
#include <cstdlib>
#include <initializer_list>
#include <new>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::compat::phobos {
namespace temp_directory_detail {
template<class Operation> auto allocate(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}
inline RetainedString addSeparator(RetainedString value) {
#if defined(_WIN32)
    constexpr char separator = '\\';
#else
    constexpr char separator = '/';
#endif
    if (!value.view().ends_with(separator)) return allocate([&] {
        return RetainedString(std::string(value.view()) + separator);
    });
    return value;
}
}
inline RetainedString ngTempDirectory() {
    static thread_local RetainedString cache;
    if (cache.isNull()) {
#if defined(_WIN32)
        std::array<wchar_t, MAX_PATH + 2> buffer;
        buffer.fill(static_cast<wchar_t>(0xffff));
        const auto length = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
        if (length) {
            if (length > buffer.size()) throw ngStaticError<ArraySliceError>(0, length, buffer.size(),
                RetainedString::fromStaticLiteral("std/file.d"), 5382);
            detail::UnicodeCursor<std::span<const wchar_t>, wchar_t> cursor(
                std::span<const wchar_t>{buffer.data(), length});
            std::string bytes;
            while (!cursor.empty()) {
                const auto scalar = cursor.front();
                temp_directory_detail::allocate([&] { file_exception_detail::appendScalar(bytes, scalar); });
                cursor.popFront();
            }
            cache = temp_directory_detail::allocate([&] { return RetainedString(std::move(bytes)); });
        }
#else
        for (const auto* name : {"TMPDIR", "TEMP", "TMP"}) {
            const auto* value = std::getenv(name);
            if (value && *value && ngFileExists(value)) {
                cache = temp_directory_detail::addSeparator(
                    temp_directory_detail::allocate([&] { return RetainedString(value); }));
                break;
            }
        }
        if (cache.isNull()) for (const auto* value : {"/tmp", "/var/tmp", "/usr/tmp"}) {
            if (ngFileExists(value)) {
                cache = temp_directory_detail::addSeparator(RetainedString::fromBorrowedView(value));
                break;
            }
        }
#endif
        if (cache.isNull()) {
            auto path = ngGetCurrentDirectory();
            auto retained = temp_directory_detail::allocate([&] { return RetainedString(std::move(path)); });
            cache = temp_directory_detail::addSeparator(std::move(retained));
        }
    }
    return cache;
}
} // namespace nijigenerate::compat::phobos
