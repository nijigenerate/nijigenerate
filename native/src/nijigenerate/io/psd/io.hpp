#pragma once
// Copyright 2021, Inochi2D Project. BSD-2-Clause: native/port/licenses/psd-d.txt.
#include "nijigenerate/io/psd/retained_file.hpp"
#include "nijigenerate/io/psd/utils.hpp"
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace nijigenerate::io::psd::utils {

void skip(RetainedFile& file, std::ptrdiff_t length);
compat::RetainedString readStr(RetainedFile& file, std::uint32_t length);
compat::RetainedString peekStr(RetainedFile& file, std::uint32_t length);
compat::RetainedString readPascalStr(RetainedFile& file, std::uint32_t readTo = 0);
compat::Slice<std::uint8_t> read(RetainedFile& file, std::size_t length);
compat::Slice<std::uint8_t> peek(RetainedFile& file, std::ptrdiff_t length);

template<class T> concept EndianValue = std::integral<detail::BaseType<T>> ||
    std::same_as<T, float> || std::same_as<T, double>;

template<EndianValue T>
T readValue(RetainedFile& file) {
    // The original performs tell even though the returned position is unused.
    [[maybe_unused]] const auto position = file.tell();
    // Allocate the original retained byte buffer before reading the stream.
    const auto received = read(file, sizeof(T));
    if (received.size() != sizeof(T)) throw compat::ngStaticError<compat::ArraySliceError>(
        0, sizeof(T), received.size(), std::source_location::current());
    std::array<std::uint8_t, sizeof(T)> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = received[i];
    // Match bigEndianToNative's bit-preserving conversion, including NaNs.
    if constexpr (std::endian::native == std::endian::little) {
        for (std::size_t i = 0; i < bytes.size() / 2; ++i) {
            const auto temporary = bytes[i];
            bytes[i] = bytes[bytes.size() - 1 - i];
            bytes[bytes.size() - 1 - i] = temporary;
        }
    }
    static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);
    if constexpr (std::same_as<detail::BaseType<T>, bool>) return static_cast<T>(bytes[0] != 0);
    else return std::bit_cast<T>(bytes);
}

template<EndianValue T>
T peekValue(RetainedFile& file) {
    const T result = readValue<T>(file);
    skip(file, -static_cast<std::ptrdiff_t>(sizeof(T)));
    return result;
}
} // namespace nijigenerate::io::psd::utils
