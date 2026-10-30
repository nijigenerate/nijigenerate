// Phobos std.zlib.UnCompress, Copyright The D Language Foundation 2000-2011.
// BSL-1.0; full license and source notice: port/licenses/phobos-kra-zip.txt.
#include "uncompress.hpp"
#include <nijigenerate/io/kra/zip.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/phobos_conv_exception.hpp>
#include <compat/source_allocator.hpp>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::io::psd::detail {
namespace {
using Byte = std::uint8_t;

std::shared_ptr<Byte[]> allocateOutput(std::size_t length) {
    try {
        return std::allocate_shared_for_overwrite<Byte[]>(
            nicxlive::core::d_runtime::SourceAllocator<Byte>{}, length);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

std::uint32_t toUint(std::size_t value) {
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw nicxlive::core::d_runtime::phobos::conv::ConvOverflowException(
            compat::RetainedString::fromStaticLiteral("Conversion positive overflow"),
            compat::RetainedString::fromStaticLiteral("std/conv.d"), 1587);
    return static_cast<std::uint32_t>(value);
}
}

UnCompress::UnCompress(std::uint32_t destinationBufferSize)
    : destinationBufferSize_(destinationBufferSize) {}

UnCompress::UnCompress(HeaderFormat format) : format_(format) {}

UnCompress::~UnCompress() noexcept {
    if (initialized_) {
        initialized_ = 0;
        inflateEnd(&stream_);
    }
    done_ = 1;
}

[[noreturn]] void UnCompress::error(int status) {
    if (initialized_) {
        inflateEnd(&stream_);
        initialized_ = 0;
    }
    throw kra::ZlibException(status);
}

compat::ConstSlice<std::uint8_t> UnCompress::uncompress(std::span<const std::uint8_t> input) {
#ifndef NDEBUG
    if (done_) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Buffer has been flushed."),
        compat::RetainedString::fromStaticLiteral("std/zlib.d"), 583);
#endif
    if (inputEnded_ || input.empty()) return {};
    if (!initialized_) {
        int windowBits = 15;
        if (format_ == HeaderFormat::gzip) windowBits += 16;
        else if (format_ == HeaderFormat::determineFromData) windowBits += 32;
        const int status = inflateInit2(&stream_, windowBits);
        if (status) error(status);
        initialized_ = 1;
    }
    if (!destinationBufferSize_)
        destinationBufferSize_ = static_cast<std::uint32_t>(toUint(input.size()) * std::uint32_t{2});
    auto destinationLength = destinationBufferSize_;
    auto capacity = destinationLength;
    auto destination = allocateOutput(destinationLength);
    std::size_t destinationFill = 0;
    stream_.next_in = const_cast<Bytef*>(input.data());
    stream_.avail_in = toUint(input.size());
    for (;;) {
        if (destinationFill > destinationLength)
            throw compat::ngStaticError<compat::ArraySliceError>(
                destinationFill, destinationLength, destinationLength,
                compat::RetainedString::fromStaticLiteral("std/zlib.d"), 620);
        stream_.next_out = destinationFill ? destination.get() + destinationFill : destination.get();
        stream_.avail_out = toUint(destinationLength - destinationFill);
        const int status = inflate(&stream_, Z_NO_FLUSH);
        if (status == Z_STREAM_END) {
            inputEnded_ = true;
            break;
        }
        if (status != Z_OK) {
            // Release the failed output before ending the stream and creating the exception.
            destination.reset();
            error(status);
        }
        if (!stream_.avail_in) break;
        destinationFill = destinationLength;
        // Ordinary allocation capacity replaces the original collector's growth policy.
        // The output remains one owned contiguous buffer for this whole call.
        destinationLength += destinationBufferSize_;
        if (destinationLength > capacity) {
            auto replacement = allocateOutput(destinationLength);
            if (destinationFill) std::memcpy(replacement.get(), destination.get(), destinationFill);
            destination = std::move(replacement);
            capacity = destinationLength;
        }
    }
    const auto produced = destinationLength - stream_.avail_out;
    auto* bytes = destination.get();
    std::shared_ptr<const void> owner(destination, static_cast<const void*>(bytes));
    return compat::ConstSlice<Byte>::retainedView(std::move(owner), bytes, produced);
}

compat::Slice<std::uint8_t> UnCompress::flush() {
#ifndef NDEBUG
    if (done_) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Buffer has been flushed before."),
        compat::RetainedString::fromStaticLiteral("std/zlib.d"), 765);
#endif
    done_ = 1;
    return {};
}

bool UnCompress::empty() const { return inputEnded_; }

} // namespace nijigenerate::io::psd::detail
