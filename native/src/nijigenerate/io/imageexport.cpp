#include "imageexport.hpp"
#include "imageexport_case.hpp"
#include "imagefmt/writer.hpp"
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/phobos_file_write.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::io::imageexport {
namespace {
void writeFile(const imagefmt::EncodedImage& encoded,
        const compat::RetainedStringSlice& name) {
    compat::phobos::ngFileWrite(name,
        std::string_view(reinterpret_cast<const char*>(encoded.buffer.data()), encoded.length));
}

int imageFormat(compat::RetainedStringSlice input) {
    const auto file = input.view();
#ifdef _WIN32
    const auto separator = file.find_last_of("/\\:");
#else
    const auto separator = file.find_last_of('/');
#endif
    const auto basename = separator == std::string_view::npos ? 0 : separator + 1;
    const auto dot = file.find_last_of('.');
    if (dot == std::string_view::npos || dot <= basename) return -1;
    const auto loweredExtension = detail::lowerExtension(input.slice(dot, input.size()));
    const auto extension = loweredExtension.view();
    if (extension == ".bmp") return imagefmt::bmp;
    if (extension == ".tga") return imagefmt::tga;
    if (extension == ".png") return imagefmt::png;
    if (extension == ".jpg" || extension == ".jpeg") return imagefmt::jpg;
    return -1;
}
void status(std::string message) {
    compat::RetainedString retained;
    try {
        retained = compat::RetainedString(std::move(message));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    const auto translated = core::translation::ngTranslateRetained(retained);
    core::tasks::ngSetStatus(compat::RetainedStringSlice(translated));
}
void failure(compat::RetainedStringSlice file, imagefmt::Error error, std::size_t sourceLine) {
    const auto errors = std::span(imagefmt::errorText);
    const auto filename = file;
    const auto index = static_cast<std::size_t>(error);
    if (index >= errors.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, errors.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/imageexport.d"), sourceLine);
    const auto text = compat::RetainedString::fromStaticView(std::string_view(errors[index]));
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(filename), core::translation::ngFormatArgument(text)};
    status(core::translation::ngFormatTyped("%s failed to export with error (%s)...", arguments));
}
} // namespace

void incExportImage(compat::RetainedStringSlice file, compat::Slice<std::uint8_t> data,
    std::int32_t width, std::int32_t height, std::int32_t channels) {
    // Encode before the write/status exception boundary and success-only free guard.
    auto encoded = imagefmt::writeImageMemory(imageFormat(file), width, height,
        {data.data(), data.size()}, channels);
    if (encoded.error != imagefmt::Error::none) {
        failure(file, encoded.error, 16);
        return;
    }
    compat::ngRunWithScopeExit([&] {
        try {
            writeFile(encoded, file);
            const std::array<core::translation::TypedFormatArgument, 1> arguments{
                core::translation::ngFormatArgument(file)};
            status(core::translation::ngFormatTyped("%s was exported...", arguments));
        } catch (const compat::ThrowableException&) {
            failure(file, imagefmt::Error::fopen, 27);
        }
    }, [&] { encoded.buffer.sourceFree(); });
}
} // namespace nijigenerate::io::imageexport
