#pragma once

// Adapted from independent LDC 1.41 Phobos std.stdio/std.exception/std.utf.
// Copyright The D Language Foundation 2000-2012.
// Boost Software License 1.0: phobos.LICENSE.
#include <nijigenerate/compat/exception_message.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cwchar>
#include <initializer_list>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace njc::detail {

// Only passive byte/backing operations belong here; errno formatters and I/O do not.
template<class Operation> auto allocateStdioStorage(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
            nijigenerate::compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
            nijigenerate::compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}

class StdioErrnoException : public nijigenerate::compat::ThrowableException {
    static std::string errnoText(int code) {
#if defined(_WIN32)
        const auto* text = std::strerror(code);
        return allocateStdioStorage([&] { return std::string(text); });
#elif defined(__GLIBC__) && defined(_GNU_SOURCE)
        std::array<char, 1024> buffer;
        const auto* text = ::strerror_r(code, buffer.data(), buffer.size());
        return allocateStdioStorage([&] { return std::string(text); });
#else
        std::array<char, 1024> buffer;
        const auto result = ::strerror_r(code, buffer.data(), buffer.size());
        const auto* text = result == 0 ? buffer.data() : "Unknown error";
        return allocateStdioStorage([&] { return std::string(text); });
#endif
    }
    static nijigenerate::compat::RetainedString enforcementMessage(const std::string& text) {
        // errnoEnforce(0) selects enforce's default "Enforcement failed" message.
        return allocateStdioStorage([&] {
            return nijigenerate::compat::RetainedString("Enforcement failed (" + text + ')');
        });
    }
    StdioErrnoException(int code, std::string text,
        nijigenerate::compat::RetainedString sourceFile, std::size_t sourceLine)
        : ThrowableException(enforcementMessage(text), std::move(sourceFile), sourceLine),
          errorCode(static_cast<unsigned>(code)), errnoMessage(std::move(text)) {}
public:
    const unsigned errorCode;
    const std::string errnoMessage;
    explicit StdioErrnoException(int code, nijigenerate::compat::RetainedString sourceFile = {},
        std::size_t sourceLine = 0)
        : StdioErrnoException(code, errnoText(code), std::move(sourceFile), sourceLine) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "std.exception.ErrnoException"; }
};

class StdioWriter {
    std::FILE* stream_;
    bool wide_ = false;
    std::array<unsigned char, 4> pending_{0xff, 0xff, 0xff, 0xff};
    std::size_t filled_ = 0;

    void putWide(char32_t scalar) {
#ifdef _WIN32
        if (scalar > 0xffff) {
            (void)std::fputwc(static_cast<wchar_t>(0xd800 + ((scalar - 0x10000) >> 10)), stream_);
            (void)std::fputwc(static_cast<wchar_t>(0xdc00 + ((scalar - 0x10000) & 0x3ff)), stream_);
            return;
        }
#endif
        (void)std::fputwc(static_cast<wchar_t>(scalar), stream_);
    }
    std::size_t stride() const {
        const auto first = pending_[0];
        if (first < 0x80) return 1;
        if (first < 0xc0 || first >= 0xf8)
            throw nijigenerate::compat::UTFException(
                nijigenerate::compat::RetainedString::fromStaticLiteral("Invalid UTF-8 sequence"), std::size_t{0});
        return first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
    }
    char32_t decode() const {
        using Cursor = nijigenerate::compat::phobos::detail::UnicodeCursor<
            std::span<const unsigned char>, unsigned char>;
        return Cursor(std::span<const unsigned char>(pending_.data(), filled_)).front();
    }
    void putByte(unsigned char byte) {
        if (!wide_) {
            (void)std::fputc(byte, stream_);
        } else if (byte <= 0x7f) {
            putWide(byte);
        } else if (byte >= 0xc0) {
            pending_[0] = byte;
            filled_ = 1;
        } else {
            if (filled_ >= pending_.size())
                throw nijigenerate::compat::ngStaticError<nijigenerate::compat::ArrayIndexError>(
                    filled_, pending_.size(),
                    nijigenerate::compat::RetainedString::fromStaticLiteral("std/stdio.d"), 3246);
            pending_[filled_++] = byte;
            if (stride() == filled_) {
                putWide(decode());
                filled_ = 0;
            }
        }
    }
public:
    explicit StdioWriter(std::FILE* stream) : stream_(stream) {
#ifdef _WIN32
        const auto descriptor = ::_fileno(stream_);
        const int mode = ::_setmode(descriptor, _O_TEXT);
        if (mode != -1) {
            (void)::_setmode(descriptor, mode);
            wide_ = (mode & (_O_WTEXT | _O_U16TEXT | _O_U8TEXT)) != 0;
        }
        ::_lock_file(stream_);
#else
        wide_ = std::fwide(stream_, 0) > 0;
        ::flockfile(stream_);
#endif
    }
    ~StdioWriter() {
#ifdef _WIN32
        ::_unlock_file(stream_);
#else
        ::funlockfile(stream_);
#endif
    }
    StdioWriter(const StdioWriter&) = delete;
    StdioWriter& operator=(const StdioWriter&) = delete;

    void put(std::string_view text) {
        if (!wide_) {
            if (std::fwrite(text.data(), 1, text.size(), stream_) != text.size())
                throw StdioErrnoException(errno,
                    nijigenerate::compat::RetainedString::fromStaticLiteral("std/stdio.d"), 3216);
            return;
        }
        for (unsigned char byte : text) putByte(byte);
    }
    void newline() { putByte('\n'); }
};

inline void ngWriteStdioLine(std::FILE* stream, std::initializer_list<std::string_view> pieces) {
    StdioWriter writer(stream);
    for (auto piece : pieces) writer.put(piece);
    writer.newline();
}

} // namespace njc::detail
