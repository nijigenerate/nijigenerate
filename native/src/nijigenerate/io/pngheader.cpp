#include "pngheader.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include "psd/retained_file.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <limits>
#include <span>
#include <exception>

namespace nijigenerate::io::pngheader {
namespace {
compat::RetainedString copyBytes(std::string_view value) {
    try { return compat::RetainedString(value); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
std::uint32_t pngHeaderUint32(std::span<const std::uint8_t> bytes) {
    if (bytes.size() != 4) throw compat::ThrowableException(
        copyBytes("PNG header integer must contain four bytes"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/pngheader.d"), 7);
    return (std::uint32_t(bytes[0]) << 24) | (std::uint32_t(bytes[1]) << 16) |
        (std::uint32_t(bytes[2]) << 8) | std::uint32_t(bytes[3]);
}
}

void ngInspectPngDimensions(compat::RetainedStringSlice path, int& width, int& height) {
    // D out parameters are initialized before entering the function, including on failure.
    width = 0;
    height = 0;
    psd::RetainedFile file{path.retainedString()};
    // End the stream outside exception unwinding: File.detach can itself fail.
    std::exception_ptr failure;
    try {
        std::array<std::uint8_t, 24> header{};
        const auto readHeader = file.rawRead(header);
        if (readHeader.size() != header.size())
            throw compat::ThrowableException(
                copyBytes("PNG source is too short to contain an IHDR header"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/pngheader.d"), 18);
        constexpr std::array<std::uint8_t, 8> signature{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        if (!std::equal(signature.begin(), signature.end(), header.begin()))
            throw compat::ThrowableException(
                copyBytes("PNG source has an invalid signature"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/pngheader.d"), 20);
        const std::span<const std::uint8_t> bytes(header);
        if (pngHeaderUint32(bytes.subspan(8, 4)) != 13 ||
            !std::equal(header.begin() + 12, header.begin() + 16, "IHDR"))
            throw compat::ThrowableException(
                copyBytes("PNG source does not begin with an IHDR chunk"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/pngheader.d"), 23);
        auto headerWidth = pngHeaderUint32(bytes.subspan(16, 4));
        auto headerHeight = pngHeaderUint32(bytes.subspan(20, 4));
        constexpr auto maxDimension = static_cast<std::uint32_t>(std::numeric_limits<int>::max());
        if (headerWidth == 0 || headerHeight == 0 || headerWidth > maxDimension || headerHeight > maxDimension)
            throw compat::ThrowableException(
                copyBytes("PNG source dimensions are invalid"),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/pngheader.d"), 28);
        width = static_cast<int>(headerWidth);
        height = static_cast<int>(headerHeight);
    } catch (...) {
        failure = std::current_exception();
    }
    try {
        file.detach();
    } catch (psd::RetainedFileError& closeError) {
        if (failure) {
            auto closeFailure = ::nicxlive::core::d_runtime::ngRetainCurrentThrowable(closeError);
            try {
                std::rethrow_exception(failure);
            } catch (compat::Throwable& original) {
                // LDC preserves the original primary Throwable and appends a
                // collateral Exception after its existing tail.
                compat::Throwable::chainTogether(
                    ::nicxlive::core::d_runtime::ngRetainCurrentThrowable(original),
                    std::move(closeFailure));
            } catch (...) {
                // Native Error failures must not be converted to Exception.
                std::rethrow_exception(failure);
            }
            std::rethrow_exception(failure);
        }
        throw;
    }
    if (failure) std::rethrow_exception(failure);

}

void ngInspectPngDimensions(std::string_view path, int& width, int& height) {
    // Preserve the existing out-parameter failure prefix at this native boundary.
    width = 0;
    height = 0;
    const auto retained = copyBytes(path);
    ngInspectPngDimensions(compat::RetainedStringSlice(retained), width, height);
}
}
