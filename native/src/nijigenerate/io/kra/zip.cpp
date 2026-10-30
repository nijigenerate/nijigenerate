// Required constructor/expand behavior of independent Phobos std.zip (LDC 1.41).
#include "zip.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <zlib.h>
#include <algorithm>
#include <array>
#include <limits>
#include <vector>
namespace nijigenerate::io::kra {
namespace {
compat::RetainedString zlibMessage(int error) {
    switch (error) {
    case Z_STREAM_END: return compat::RetainedString::fromStaticLiteral("stream end");
    case Z_NEED_DICT: return compat::RetainedString::fromStaticLiteral("need dict");
    case Z_ERRNO: return compat::RetainedString::fromStaticLiteral("errno");
    case Z_STREAM_ERROR: return compat::RetainedString::fromStaticLiteral("stream error");
    case Z_DATA_ERROR: return compat::RetainedString::fromStaticLiteral("data error");
    case Z_MEM_ERROR: return compat::RetainedString::fromStaticLiteral("mem error");
    case Z_BUF_ERROR: return compat::RetainedString::fromStaticLiteral("buf error");
    case Z_VERSION_ERROR: return compat::RetainedString::fromStaticLiteral("version error");
    default: return compat::RetainedString::fromStaticLiteral("unknown error");
    }
}
void require(bool condition, const char* message, std::size_t line) {
    if (condition) return;
    throw ZipException{compat::RetainedString::fromBorrowedView(message),
        compat::RetainedString::fromStaticLiteral("std/zip.d"), line};
}
}
ZlibException::ZlibException(int error)
    : ThrowableException(zlibMessage(error), compat::RetainedString::fromStaticLiteral("std/zlib.d"), 98) {}
void ZipArchive::bounds(std::uint64_t offset, std::uint64_t count, std::size_t sourceLine) const {
    const auto lower = static_cast<std::uint32_t>(offset);
    const auto upper = lower + static_cast<std::uint32_t>(count);
    if (lower > upper || upper > bytes_.size())
        throw compat::ngStaticError<compat::ArraySliceError>(lower, upper, bytes_.size(),
            compat::RetainedString::fromStaticLiteral("std/zip.d"), sourceLine);
}
std::uint16_t ZipArchive::u16(std::uint64_t offset) const {
    offset = static_cast<std::uint32_t>(offset);
    bounds(offset, 2, 1305);
    return static_cast<std::uint16_t>(bytes_[offset] | (std::uint16_t(bytes_[offset + 1]) << 8));
}
std::uint32_t ZipArchive::u32(std::uint64_t offset) const {
    offset = static_cast<std::uint32_t>(offset);
    bounds(offset, 4, 1311);
    return std::uint32_t(bytes_[offset]) | (std::uint32_t(bytes_[offset + 1]) << 8) |
        (std::uint32_t(bytes_[offset + 2]) << 16) | (std::uint32_t(bytes_[offset + 3]) << 24);
}
std::uint64_t ZipArchive::u64(std::uint64_t offset) const {
    offset = static_cast<std::uint32_t>(offset);
    bounds(offset, 8, 1317);
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= std::uint64_t(bytes_[offset + i]) << (i * 8);
    return value;
}
ZipArchive::ZipArchive(compat::Slice<std::uint8_t> bytes) : bytes_(bytes) {
    require(bytes_.size() <= std::numeric_limits<std::uint32_t>::max() - 2,
        "zip files bigger than 4 GB are unsupported", 789);
    std::uint64_t end = bytes_.size();
    for (std::uint32_t extra = 0; extra < 65536 && 22ULL + extra <= bytes_.size(); ++extra) {
        const auto start = bytes_.size() - 22 - extra;
        if (u32(start) != 0x06054b50 || u16(start + 4) != 0 || u16(start + 6) != 0) continue;
        const bool zip64 = start >= 20 && u32(start - 20) == 0x07064b50;
        const auto localCount = u16(start + 8), allCount = u16(start + 10);
        if (localCount > allCount && (!zip64 || localCount < 0xffff)) continue;
        const auto size = u32(start + 12), offset = u32(start + 16);
        if (size > start && (!zip64 || size < 0xffff)) continue;
        const auto remaining = static_cast<std::uint32_t>(start) - size;
        if (offset > remaining && (!zip64 || offset < 0xffff)) continue;
        if (start + u16(start + 20) + 22 != bytes_.size()) continue;
        require(end == bytes_.size(), "found more than one valid 'end of central dir record'", 1157);
        end = start;
    }
    require(end != bytes_.size(), "found no valid 'end of central dir record'", 1163);
    struct Segment { std::uint64_t first, last; };
    std::vector<Segment> free{{0, bytes_.size()}};
    const auto remove = [&](std::uint64_t firstArgument, std::uint64_t lastArgument) {
        const auto first = static_cast<std::uint32_t>(firstArgument);
        const auto last = static_cast<std::uint32_t>(lastArgument);
        if (first >= last) {
#ifndef NDEBUG
            throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral("segment invalid"),
                compat::RetainedString::fromStaticLiteral("std/zip.d"), 1350);
#else
            // Keep the existing safety guard outside the source function's contract.
            require(false, "segment invalid", 1350);
#endif
        }
        const auto found = std::find_if(free.begin(), free.end(), [&](Segment value) {
            return value.first <= first && value.last >= last;
        });
        require(found != free.end(), "overlapping data detected", 1362);
        const auto segment = *found;
        free.erase(found);
        if (segment.first < first) free.push_back({segment.first, first});
        if (last < segment.last) free.push_back({last, segment.last});
    };
    remove(end, end + 22 + u16(end + 20));
    std::uint64_t count = u16(end + 10), size = u32(end + 12), offset = u32(end + 16);
    if (end >= 20 && u32(end - 20) == 0x07064b50) {
        const auto locator = end - 20;
        remove(locator, end);
        const auto recordOffset = u64(locator + 8);
        require(recordOffset + std::uint64_t{56} <= bytes_.size(), "corrupted directory", 819);
        if (recordOffset > std::numeric_limits<std::uint32_t>::max())
            throw compat::PhobosConvOverflowException{
                compat::RetainedString::fromStaticLiteral("Conversion positive overflow"),
                compat::RetainedString::fromStaticLiteral("std/conv.d"), 1587};
        const auto record = static_cast<std::uint32_t>(recordOffset);
        require(u32(record) == 0x06064b50, "invalid Zip EOCD64 signature", 823);
        const auto recordSize = u64(record + 4);
        // Preserve source unsigned arithmetic and conversion order. Every later
        // byte access and segment removal still has its own checked boundary.
        require(recordSize + record - 12 <= bytes_.size(), "invalid Zip EOCD64 size", 827);
        remove(record, record + 12 + recordSize);
        const auto onDisk = u64(record + 24);
        count = u64(record + 32); size = u64(record + 40); offset = u64(record + 48);
        require(onDisk <= std::numeric_limits<std::uint32_t>::max(),
            "supposedly more than 4294967296 files in archive", 838);
        require(onDisk == count, "multiple disk zips not supported", 841);
        require(size <= record && offset <= record && size + offset <= record, "corrupted directory", 844);
    }
    const auto directoryEnd = static_cast<std::uint32_t>(offset + size);
    std::uint32_t position = static_cast<std::uint32_t>(offset);
    for (std::uint64_t i = 0; i < count; ++i) {
        bounds(position, 4, 875);
        require(u32(position) == 0x02014b50, "wrong central file header signature found", 875);
        auto entry = std::make_shared<ZipEntry>();
        (void)u16(position + 4);
        (void)u16(position + 6);
        entry->flags = u16(position + 8); entry->method = u16(position + 10);
        (void)u32(position + 12);
        entry->crc = u32(position + 16); entry->compressedSize = u32(position + 20);
        entry->expandedSize = u32(position + 24);
        const auto nameSize = u16(position + 28), extraSize = u16(position + 30), commentSize = u16(position + 32);
        (void)u16(position + 36);
        (void)u32(position + 38);
        entry->offset = u32(position + 42);
        const auto next = static_cast<std::uint32_t>(position + 46u + nameSize + extraSize + commentSize);
        remove(position, next);
        require(next <= directoryEnd, "invalid field lengths in file header found", 899);
        bounds(position + 46, nameSize, 902);
        const std::string name(reinterpret_cast<const char*>(bytes_.data() + position + 46), nameSize);
        bounds(position + 46 + nameSize, extraSize, 904);
        bounds(position + 46 + nameSize + extraSize, commentSize, 906);
        const std::uint32_t dataOffset = entry->offset + 30u +
            u16(std::uint64_t(entry->offset) + 26) + u16(std::uint64_t(entry->offset) + 28);
        remove(entry->offset, dataOffset + entry->compressedSize);
        bounds(dataOffset, entry->compressedSize, 918);
        entry->compressedData = bytes_.slice(dataOffset, dataOffset + entry->compressedSize);
        directory[name] = entry;
        position = next;
    }
    require(position == directoryEnd, "invalid directory entry 3", 923);
}
void ZipArchive::expand(std::shared_ptr<ZipEntry> entry) {
    const std::uint64_t offset = compat::ngRequireObject(entry)->offset;
    bounds(offset, 4, 1189);
    require(u32(offset) == 0x04034b50, "wrong local file header signature found", 1189);
    (void)u16(offset + 4);
    entry->flags = u16(offset + 6); entry->method = u16(offset + 8);
    (void)u32(offset + 10);
    entry->crc = u32(offset + 14);
    entry->compressedSize = std::max(u32(offset + 18), entry->compressedSize);
    entry->expandedSize = std::max(u32(offset + 22), entry->expandedSize);
    (void)u16(offset + 26);
    (void)u16(offset + 28);
    require((entry->flags & 1) == 0, "encryption not supported", 1211);
    if (entry->method == 0) { entry->expandedData = entry->compressedData; return; }
    require(entry->method == 8, "unsupported compression method", 1228);
    z_stream stream{};
    stream.next_in = entry->compressedData.data();
    stream.avail_in = static_cast<uInt>(entry->compressedData.size());
    const auto initialized = inflateInit2(&stream, -15);
    if (initialized != Z_OK) throw ZlibException(initialized);
    struct Cleanup {
        z_stream* stream;
        bool active = true;
        ~Cleanup() { if (active) inflateEnd(stream); }
    } cleanup{&stream};
    compat::Slice<std::uint8_t> output;
    std::size_t length = entry->expandedSize;
    if (!length) length = entry->compressedData.size() * 2 + 1;
    std::size_t previousLength = 0;
    for (;;) {
        output.resize(length);
        if (previousLength >= output.size())
            throw compat::ngStaticError<compat::ArrayIndexError>(previousLength, output.size(),
                compat::RetainedString::fromStaticLiteral("std/zlib.d"), 262);
        stream.next_out = output.data() + previousLength;
        if (length - previousLength > std::numeric_limits<uInt>::max())
            throw compat::PhobosConvOverflowException{
                compat::RetainedString::fromStaticLiteral("Conversion positive overflow"),
                compat::RetainedString::fromStaticLiteral("std/conv.d"), 1587};
        stream.avail_out = static_cast<uInt>(length - previousLength);
        previousLength = length;
        const auto result = inflate(&stream, Z_NO_FLUSH);
        if (result == Z_OK) { length = output.size() * 2; continue; }
        if (result != Z_STREAM_END) {
            (void)inflateEnd(&stream);
            cleanup.active = false;
            throw ZlibException(result);
        }
        output.resize(stream.total_out);
        const auto ended = inflateEnd(&stream);
        cleanup.active = false;
        if (ended != Z_OK) throw ZlibException(ended);
        entry->expandedData = output;
        return;
    }
}
}
