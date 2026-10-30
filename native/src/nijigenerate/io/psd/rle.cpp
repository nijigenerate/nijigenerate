// Copyright 2021, Inochi2D Project. BSD-2-Clause: native/port/licenses/psd-d.txt.
#include "nijigenerate/io/psd/rle.hpp"
#include "uncompress.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace nijigenerate::io::psd {
namespace {
template<class Operation> auto allocateBytes(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<std::size_t N> void require(bool condition, const char (&message)[N], std::size_t line) {
    if (condition) return;
    const auto copiedMessage = allocateBytes([&] {
        return compat::RetainedString(std::string_view(message, N - 1));
    });
    throw compat::ThrowableException(copiedMessage,
        compat::RetainedString::fromStaticLiteral("vendor/psd-d/source/psd/rle.d"), line);
}
}
void decodeRLE(std::span<const std::uint8_t> source, std::span<std::uint8_t> destination) {
    std::size_t sourceOffset{}, destinationOffset{};
    while (sourceOffset < source.size()) {
        const auto tag = source[sourceOffset++];
        if (tag == 0x80) continue;
        if (tag > 0x80) {
            const std::size_t count = 257 - tag;
            require(sourceOffset < source.size(), "Truncated PSD RLE stream: missing replicated byte", 26);
            require(count <= destination.size() - destinationOffset,
                "Invalid PSD RLE stream: replicated run exceeds destination", 27);
            std::fill_n(destination.data() + destinationOffset, count, source[sourceOffset++]);
            destinationOffset += count;
        } else {
            const std::size_t count = tag + 1;
            require(count <= source.size() - sourceOffset, "Truncated PSD RLE stream: literal run exceeds source", 36);
            require(count <= destination.size() - destinationOffset,
                "Invalid PSD RLE stream: literal run exceeds destination", 38);
            std::copy_n(source.data() + sourceOffset, count, destination.data() + destinationOffset);
            sourceOffset += count;
            destinationOffset += count;
        }
    }
    require(destinationOffset == destination.size(),
        "Truncated PSD RLE stream: scanline is shorter than its destination", 46);
}

compat::Slice<std::uint8_t> decodeZip(std::span<const std::uint8_t> source,
    std::uint32_t width, std::uint32_t height, bool prediction) {
    const auto decodedLength = static_cast<std::size_t>(width) * height;
    auto data = allocateBytes([&] {
        return compat::Slice<std::uint8_t>{std::vector<std::uint8_t>(decodedLength)};
    });
    const auto decoder = std::allocate_shared<detail::UnCompress>(
        nicxlive::core::d_runtime::SourceAllocator<detail::UnCompress>{}, std::uint32_t{64 * 1024});
    std::size_t sourceOffset{}, decodedOffset{};
    while (sourceOffset < source.size() && !decoder->empty()) {
        const auto sourceEnd = std::min(source.size(), sourceOffset + 1024);
        const auto chunk = decoder->uncompress(source.subspan(sourceOffset, sourceEnd - sourceOffset));
        require(chunk.size() <= decodedLength - decodedOffset,
            "ZIP-compressed PSD channel exceeds its declared dimensions", 62);
        if (!chunk.empty()) std::copy_n(chunk.data(), chunk.size(), data.data() + decodedOffset);
        decodedOffset += chunk.size();
        sourceOffset = sourceEnd;
    }
    const auto tail = decoder->flush();
    require(tail.size() <= decodedLength - decodedOffset,
        "ZIP-compressed PSD channel exceeds its declared dimensions", 69);
    if (!tail.empty()) std::copy_n(tail.data(), tail.size(), data.data() + decodedOffset);
    decodedOffset += tail.size();
    require(decoder->empty() && decodedOffset == decodedLength, "Invalid ZIP-compressed PSD channel length", 73);
    if (prediction) {
        for (std::uint32_t y = 0; y < height; ++y) {
            const auto row = static_cast<std::size_t>(y) * width;
            for (std::uint32_t x = 1; x < width; ++x) {
                const auto index = row + x;
                data[index] = static_cast<std::uint8_t>(static_cast<std::uint32_t>(data[index]) + data[index - 1]);
            }
        }
    }
    return data;
}
} // namespace nijigenerate::io::psd
