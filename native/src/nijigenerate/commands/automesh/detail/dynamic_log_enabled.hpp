#pragma once

#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/io/psd/retained_file.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <array>
#include <cerrno>
#include <cstdio>
#include <string_view>
#include <cwchar>
#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif

namespace nijigenerate::commands::automesh::dynamic::detail {

class DynamicLogStreamLock {
    std::FILE* stream;
    bool wide = false;
    std::array<char,4> pending{};
    std::size_t filled = 0;
public:
    explicit DynamicLogStreamLock(std::FILE* value) : stream(value) {
#if defined(_WIN32)
        const auto fd = _fileno(stream);
        const auto mode = _setmode(fd, _O_TEXT);
        if (mode != -1) {
            _setmode(fd, mode);
            wide = (mode & (_O_WTEXT | _O_U16TEXT | _O_U8TEXT)) != 0;
        }
        _lock_file(stream);
#else
        wide = std::fwide(stream,0) > 0;
        flockfile(stream);
#endif
    }
    ~DynamicLogStreamLock() {
#if defined(_WIN32)
        _unlock_file(stream);
#else
        funlockfile(stream);
#endif
    }
    DynamicLogStreamLock(const DynamicLogStreamLock&) = delete;
    DynamicLogStreamLock& operator=(const DynamicLogStreamLock&) = delete;
    void put(char byte) {
        if (!wide) { std::fputc(static_cast<unsigned char>(byte),stream); return; }
        const auto value = static_cast<unsigned char>(byte);
        if (value <= 0x7f) { std::fputwc(value,stream); return; }
        if (value >= 0xc0) { pending[0]=byte; filled=1; return; }
        if (filled >= pending.size()) throw compat::ArrayIndexError(filled,pending.size(),
            compat::RetainedString::fromStaticLiteral("std/stdio.d"),3246);
        pending[filled++]=byte;
        const auto head = static_cast<unsigned char>(pending[0]);
        const auto width = head < 192 ? 1u : head < 224 ? 2u : head < 240 ? 3u
            : head < 248 ? 4u : head < 252 ? 5u : head < 254 ? 6u : 1u;
        if (filled != width) return;
        using Cursor = compat::phobos::detail::UnicodeCursor<std::string_view,char>;
        const auto scalar = Cursor(std::string_view(pending.data(),filled)).front();
#if defined(_WIN32)
        if (scalar > 0xffff) {
            std::fputwc(static_cast<wchar_t>(((scalar-0x10000)>>10)+0xd800),stream);
            std::fputwc(static_cast<wchar_t>(((scalar-0x10000)&0x3ff)+0xdc00),stream);
        } else
#endif
        std::fputwc(static_cast<wchar_t>(scalar),stream);
        filled=0;
    }
    void put(std::string_view bytes) {
        if (!wide) {
            if (std::fwrite(bytes.data(),1,bytes.size(),stream) != bytes.size())
                throw io::psd::RetainedFileError(errno,"Enforcement failed");
        } else for (char byte:bytes) put(byte);
    }
};

// The CMD_LOG branch is separate from the existing disabled variadic helper.
// Current editor callers pass the format string and before/after size values.
template<class... T>
void cmdLogEnabled(std::string_view pattern, const T&... values) {
    // Like Phobos lockingTextWriter, hold the actual FILE lock over formatting
    // and the trailing newline, including exceptional unwinding.
    auto* const stream = stdout;
    DynamicLogStreamLock streamLock(stream);
    const std::array<core::translation::TypedFormatArgument, sizeof...(T)> arguments{
        core::translation::ngFormatArgument(values)...};
    core::translation::ngFormatTypedTo([&](std::string_view bytes) { streamLock.put(bytes); },pattern,arguments);
    // The source byte-oriented LockingTextWriter.put(char) ignores FPUTC's
    // result, unlike its checked bulk put(string) path above.
    streamLock.put('\n');
}

} // namespace nijigenerate::commands::automesh::dynamic::detail
