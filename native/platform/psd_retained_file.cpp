// Copyright The D Language Foundation 2007-.
// CString conversion: Copyright Denis Shelomovskij 2013-2014.
// Required Phobos File behavior adapted under BSL-1.0; see phobos-psd-file.txt.
#include "nijigenerate/io/psd/retained_file.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include <compat/native_access_fault.hpp>
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include "nijigenerate/compat/phobos_conv_exception.hpp"
#include "nijigenerate/compat/scope_exit.hpp"
#include <common/unicode_lowercase.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <limits>
#include <utility>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#else
#include <sys/types.h>
#endif

namespace nijigenerate::io::psd {
namespace {
template<std::size_t N> auto literal(const char (&text)[N]) {
    return compat::RetainedString::fromStaticLiteral(text);
}
template<class Unit> class TemporaryCString {
    Unit stack_[256 / sizeof(Unit)];
    Unit* pointer_ = nullptr;
    std::size_t length_ = 0;
    static Unit* stackTag() { return reinterpret_cast<Unit*>(std::numeric_limits<std::size_t>::max()); }
    template<std::size_t N>
    [[noreturn]] static void outOfMemory(const char (&file)[N], std::size_t line) {
        // A failed malloc must not allocate a retained copy of its source name.
        throw compat::ngStaticError<compat::OutOfMemoryError>(literal(file), line);
    }
public:
    explicit TemporaryCString(compat::RetainedString text) {
        if (text.isNull()) return;
        if constexpr (sizeof(Unit) == 1) {
            if (text.size() < std::size(stack_)) pointer_ = stackTag();
            else {
                pointer_ = static_cast<Unit*>(std::malloc(text.size() + 1));
                if (!pointer_) outOfMemory("std/internal/memory.d", 32);
            }
            auto* output = pointer_ == stackTag() ? stack_ : pointer_;
            std::memcpy(output, text.data(), text.size());
            output[text.size()] = 0;
            length_ = text.size();
        } else {
            Unit* output = stack_;
            Unit* heap = nullptr;
            std::size_t capacity = std::size(stack_), count = 0;
            const auto put = [&](Unit unit) {
                if (count + 1 == capacity) {
                    std::size_t size = capacity * 3 / 2;
                    if (!heap) {
                        if (size <= text.size()) size = text.size() + 1;
                        heap = static_cast<Unit*>(std::malloc(size * sizeof(Unit)));
                        if (!heap) outOfMemory("std/internal/memory.d", 32);
                        std::memcpy(heap, output, capacity * sizeof(Unit));
                    } else {
                        if (capacity >= std::numeric_limits<std::size_t>::max() / (2 * sizeof(Unit)))
                            outOfMemory("std/internal/cstring.d", 287);
                        auto* grown = static_cast<Unit*>(std::realloc(heap, size * sizeof(Unit)));
                        if (!grown) outOfMemory("std/internal/memory.d", 48);
                        heap = grown;
                    }
                    output = heap; capacity = size;
                }
                output[count++] = unit;
            };
            nicxlive::core::common::unicode::ReplacementUtf8Cursor cursor{text.view()};
            while (!cursor.empty()) {
                const auto scalar = cursor.take();
                if (scalar <= 0xffff) put(static_cast<Unit>(scalar));
                else {
                    put(static_cast<Unit>(0xd800 + ((scalar - 0x10000) >> 10)));
                    put(static_cast<Unit>(0xdc00 + ((scalar - 0x10000) & 0x3ff)));
                }
            }
            output[count] = 0;
            pointer_ = heap ? heap : stackTag();
            length_ = count;
        }
    }
    TemporaryCString(const TemporaryCString&) = delete;
    TemporaryCString& operator=(const TemporaryCString&) = delete;
    ~TemporaryCString() { if (pointer_ != stackTag()) std::free(pointer_); }
    const Unit* data() const { return pointer_ == stackTag() ? stack_ : pointer_; }
};

std::FILE* openSourceFile(compat::RetainedString name) {
#if defined(_WIN32)
    TemporaryCString<wchar_t> namez(name), modez(literal("rb"));
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
    auto* result = ::_wfopen(namez.data(), modez.data());
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#else
    TemporaryCString<char> namez(name), modez(literal("rb"));
    auto* result = std::fopen(namez.data(), modez.data());
#endif
    return result;
}
} // namespace

struct RetainedFile::Control {
    enum class Orientation : std::int32_t { unknown, narrow, wide };
    std::FILE* handle{};
    bool isPopened = false;
    Orientation orientation = Orientation::unknown;
    ~Control() noexcept { if (handle) (void)std::fclose(handle); }
};

RetainedFile::RetainedFile(compat::RetainedString name) {
    // tempCString preserves embedded NULs and the CRT consumes the first one.
    // Windows transcodes the full descriptor, including bytes after a NUL.
    auto* handle = openSourceFile(name);
    const int error = errno;
    if (!handle) throw RetainedFileError(error,
        "Cannot open file `" + std::string(name.view()) + "' in mode `rb'", literal("std/stdio.d"), 529);
    std::unique_ptr<std::FILE, decltype(&std::fclose)> pending(handle, &std::fclose);
    try { owner_ = std::make_shared<Control>(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(literal("std/stdio.d"), 492);
    }
    owner_->handle = pending.release();
    name_ = std::move(name);
}

RetainedFile::RetainedFile(const RetainedFile& other) : owner_(other.owner_), name_(other.name_) {}
RetainedFile::RetainedFile(RetainedFile&& other) noexcept
    : owner_(std::move(other.owner_)), name_(std::move(other.name_)) {}
RetainedFile& RetainedFile::operator=(RetainedFile other) {
    std::swap(owner_, other.owner_);
    std::swap(name_, other.name_);
    return *this;
}
RetainedFile::~RetainedFile() noexcept(false) { detach(); }

RetainedFile RetainedFile::temporary() {
    // Preserve the source CRT API rather than substituting tmpfile_s semantics.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
    auto* handle = std::tmpfile();
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    if (!handle) throw RetainedFileError(errno, "Could not create temporary file with tmpfile()",
        literal("std/stdio.d"), 2276);
    RetainedFile result;
    std::unique_ptr<std::FILE, decltype(&std::fclose)> pending(handle, &std::fclose);
    try { result.owner_ = std::make_shared<Control>(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(literal("std/stdio.d"), 492);
    }
    result.owner_->handle = pending.release();
    return result;
}
bool RetainedFile::isOpen() const noexcept { return owner_ && owner_->handle; }

void RetainedFile::detach() {
    if (!owner_) return;
    compat::ngRunWithScopeExit([&] {
        if (owner_.use_count() == 1) closeHandles();
    }, [&] { owner_.reset(); });
}
void RetainedFile::close() {
    if (!owner_) return;
    compat::ngRunWithScopeExit([&] {
        if (!owner_->handle) return;
        compat::ngRunWithScopeExit([&] { closeHandles(); }, [&] { owner_->handle = nullptr; });
    }, [&] { owner_.reset(); });
}
void RetainedFile::closeHandles() {
#ifndef NDEBUG
    if (!owner_) throw compat::ngStaticError<compat::AssertError>(literal("Assertion failure"), literal("std/stdio.d"), 671);
#endif
    if (owner_->handle) {
        // fclose disassociates the FILE even on failure. Invalidate aliases
        // before entering the CRT, retaining Control until the caller exits.
        auto* handle = std::exchange(owner_->handle, nullptr);
        const int result = std::fclose(handle);
        const int error = errno;
        if (result != 0)
            throw RetainedFileError(error, "Could not close file `" + std::string(name_.view()) + "'",
                literal("std/stdio.d"), 693);
    }
}

void RetainedFile::flush() {
    if (!isOpen()) throw compat::ThrowableException(literal("Attempting to flush() in an unopened file"),
        literal("std/stdio.d"), 1028);
    if (std::fflush(owner_->handle) != 0)
        throw RetainedFileError(errno, "Enforcement failed", literal("std/stdio.d"), 1029);
}

void RetainedFile::seek(std::int64_t offset, int origin) {
    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END)
        throw compat::ThrowableException(literal(
            "Invalid `origin` argument passed to `seek`, must be one of: SEEK_SET, SEEK_CUR, SEEK_END"),
            literal("std/stdio.d"), 1261);
    if (!isOpen()) throw compat::ThrowableException(literal("Attempting to seek() in an unopened file"),
        literal("std/stdio.d"), 1264);
#if defined(_WIN32)
    const int result = _fseeki64(owner_->handle, offset, origin);
#else
    if constexpr (sizeof(off_t) < sizeof(offset)) {
        if (offset < std::numeric_limits<off_t>::min())
            throw compat::PhobosConvOverflowException(literal("Conversion negative overflow"),
                literal("std/conv.d"), 1581);
        if (offset > std::numeric_limits<off_t>::max())
            throw compat::PhobosConvOverflowException(literal("Conversion positive overflow"),
                literal("std/conv.d"), 1587);
    }
    const int result = ::fseeko(owner_->handle, static_cast<off_t>(offset), origin);
#endif
    if (result != 0) throw RetainedFileError(errno, "Could not seek in file `" + std::string(name_.view()) + "'",
        literal("std/stdio.d"), 1283);
}
std::uint64_t RetainedFile::tell() const {
    if (!isOpen()) throw compat::ThrowableException(literal("Attempting to tell() in an unopened file"),
        literal("std/stdio.d"), 1327);
#if defined(_WIN32)
    const auto result = _ftelli64(owner_->handle);
#else
    const auto result = ::ftello(owner_->handle);
#endif
    if (result == -1) throw RetainedFileError(errno, "Query ftell() failed for file `" + std::string(name_.view()) + "'",
        literal("std/stdio.d"), 1340);
    return static_cast<std::uint64_t>(result);
}

std::uint64_t RetainedFile::size() {
    // Current Phobos File.size uses tell/seek with unconditional restoration.
    std::uint64_t position;
    try { position = tell(); }
    catch (const compat::ThrowableException&) { return std::numeric_limits<std::uint64_t>::max(); }
    std::uint64_t result;
    compat::ngRunWithScopeExit([&] {
        seek(0, SEEK_END);
        result = tell();
    }, [&] { seek(std::bit_cast<std::int64_t>(position)); });
    return result;
}

std::span<std::uint8_t> RetainedFile::rawRead(std::span<std::uint8_t> buffer) {
    if (buffer.empty()) return buffer;
    if (!isOpen()) throw compat::ThrowableException(literal("Attempting to read from an unopened file"),
        literal("std/stdio.d"), 1104);
#if defined(_WIN32)
    const int descriptor = _fileno(owner_->handle);
    const int mode = _setmode(descriptor, _O_BINARY);
#endif
    std::span<std::uint8_t> received;
    const auto read = [&] {
        const auto count = std::fread(buffer.data(), 1, buffer.size(), owner_->handle);
#ifndef NDEBUG
        if (count > buffer.size())
            throw compat::ngStaticError<compat::AssertError>(literal("Assertion failure"), literal("std/stdio.d"), 1112);
#endif
        const bool failed = count != buffer.size() && std::ferror(owner_->handle);
        const int error = errno;
        if (failed) throw RetainedFileError(error, "Enforcement failed", literal("std/stdio.d"), 1115);
        received = buffer.first(count);
    };
#if defined(_WIN32)
    compat::ngRunWithScopeExit(read, [&] { _setmode(descriptor, mode); });
#else
    read();
#endif
    return received;
}
void RetainedFile::rawWrite(std::span<const std::uint8_t> buffer) {
    // Like the original byte rawWrite, the caller must supply an open stream.
    // Reading the absent Impl is the source access fault even for empty input.
    if (!owner_) nicxlive::core::d_runtime::ngFailNullAccess(false);
#if defined(_WIN32)
    const int descriptor = _fileno(owner_->handle);
    const int oldMode = _setmode(descriptor, _O_BINARY);
    if (oldMode != _O_BINARY) {
        _setmode(descriptor, oldMode);
        flush();
        _setmode(descriptor, _O_BINARY);
    }
    const auto restore = [&] {
        if (oldMode != _O_BINARY) { flush(); _setmode(descriptor, oldMode); }
    };
#endif
    const auto write = [&] {
        const auto count = std::fwrite(buffer.data(), 1, buffer.size(), owner_->handle);
        const int error = errno;
        const auto actual = count == std::numeric_limits<std::size_t>::max() ? 0 : count;
        if (actual != buffer.size()) throw RetainedFileError(error,
            "Wrote " + std::to_string(actual) + " instead of " + std::to_string(buffer.size())
            + " objects of type ubyte to file `" + std::string(name_.view()) + "'",
            literal("std/stdio.d"), 1214);
    };
#if defined(_WIN32)
    compat::ngRunWithScopeExit(write, restore);
#else
    write();
#endif
}
} // namespace nijigenerate::io::psd
