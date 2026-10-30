#pragma once
// Array-cast failure program: Digital Mars 2000-2019, Boost Software License 1.0.
// Full retained notice: native/port/licenses/druntime-array-casting.txt.

#include "slice.hpp"
#include "source_reference.hpp"
#include "assert_error.hpp"
#include "retained_string.hpp"
#include <concepts>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

namespace nijigenerate::compat {

template<class T> concept ReinterpretedByteElement =
    std::same_as<T, std::uint16_t> || std::same_as<T, float>;

// A typed descriptor over canonical byte ownership, without unaligned T references.
template<ReinterpretedByteElement T> class ByteView {
    Slice<std::uint8_t> bytes_;

    [[noreturn]] static void arrayCastError(std::size_t length, std::string_view sourceType) {
        // core.internal.array.casting.onArrayCastError deliberately retains
        // this C allocation forever and does not check a failed pureMalloc.
        constexpr std::size_t capacity = 2048;
        const auto savedErrno = errno;
        auto* message = static_cast<char*>(std::malloc(capacity));
        errno = savedErrno;
        std::size_t index = 0;
        const auto add = [&](std::string_view value) {
            auto count = capacity - 1 - index;
            if (count > value.size()) count = value.size();
            std::memcpy(message + index, value.data(), count);
            index += count;
        };
        const auto addNumber = [&](std::size_t value) {
            std::array<char, std::numeric_limits<std::size_t>::digits10 + 1> buffer;
            const auto end = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            add({buffer.data(), static_cast<std::size_t>(end.ptr - buffer.data())});
        };
        constexpr std::string_view target = std::same_as<T, float> ? "float" : "ushort";
        add("`");
        add(sourceType);
        add("[]` of length ");
        addNumber(length);
        add(" cannot be cast to `");
        add(target);
        add("[]` as its length in bytes (");
        addNumber(length); // The actual source element is ubyte, of size one.
        add(") is not a multiple of `");
        add(target);
        add(".sizeof` (");
        addNumber(sizeof(T));
        add(").");
        message[index] = '\0';
#ifdef NDEBUG
        ngAssertUnreachable(message, "core/internal/array/casting.d", 68);
#else
        throw ngStaticError<AssertError>(RetainedString::fromBorrowedView({message, index}),
            RetainedString::fromStaticLiteral("core/internal/array/casting.d"), 68);
#endif
    }

    static T read(const Slice<std::uint8_t>& bytes, std::size_t offset) {
        T value;
        std::memcpy(&value, bytes.data() + offset, sizeof(T));
        return value;
    }
    static void write(Slice<std::uint8_t>& bytes, std::size_t offset, T value) {
        std::memcpy(bytes.data() + offset, &value, sizeof(T));
    }
    std::size_t offset(std::size_t index) const {
        if (index >= size()) throw ngStaticError<ArrayIndexError>(index, size(),
            std::source_location::current());
        return index * sizeof(T);
    }

public:
    using value_type = T;

    using Reference = SourceReference<T>;

    ByteView() = default;
    explicit ByteView(Slice<std::uint8_t> bytes, std::string_view sourceType = "ubyte")
        : bytes_(std::move(bytes)) {
        if (bytes_.size() % sizeof(T)) arrayCastError(bytes_.size(), sourceType);
    }
    bool isNull() const noexcept { return bytes_.isNull(); }
    bool empty() const noexcept { return size() == 0; }
    void clear() { bytes_.clear(); }
    std::size_t size() const noexcept { return bytes_.size() / sizeof(T); }
    std::size_t byteSize() const noexcept { return bytes_.size(); }
    const void* address() const noexcept { return bytes_.data(); }
    T get(std::size_t index) const { return read(bytes_, offset(index)); }
    void set(std::size_t index, T value) { write(bytes_, offset(index), value); }
    Reference operator[](std::size_t index) { return Reference::fromBytesUnchecked(bytes_, offset(index)); }
    SourceReference<const T> operator[](std::size_t index) const {
        return SourceReference<const T>::fromBytesUnchecked(bytes_, offset(index));
    }
    // For the genuine pointer-backed MIR iterator after its own checks.
    Reference unchecked(std::size_t index) { return Reference::fromBytesUnchecked(bytes_, index * sizeof(T)); }
    SourceReference<const T> unchecked(std::size_t index) const {
        return SourceReference<const T>::fromBytesUnchecked(bytes_, index * sizeof(T));
    }
    ByteView slice(std::size_t first, std::size_t last) const {
        if (first > last || last > size()) throw ngStaticError<ArraySliceError>(first, last, size(),
            std::source_location::current());
        return ByteView(bytes_.slice(first * sizeof(T), last * sizeof(T)));
    }
};

} // namespace nijigenerate::compat
