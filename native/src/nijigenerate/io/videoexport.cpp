// Copyright 2022 Inochi2D Project, 2024 nijigenerate Project. BSD-2-Clause.
#include "videoexport.hpp"
#include "videoexport_process.hpp"
#include "nijigenerate/core/selector/phobos_sort.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include "nijigenerate/core/i18n_case.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/core/typed_format.hpp"
#include <array>
#include <bit>
#include <stdexcept>
#include <vector>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <common/object_key_policy.hpp>
#if !defined(_WIN32)
#include <csignal>
#endif

namespace nijigenerate::io::videoexport {
namespace {
thread_local bool hasffmpeg = false;
thread_local compat::Slice<VideoCodec> codecs;
VideoString retainVideoBytes(std::string value) {
    return detail::allocateVideoStorage([&] { return VideoString(std::move(value)); });
}
VideoString copyVideoBytes(std::string_view value) {
    return detail::allocateVideoStorage([&] { return VideoString(value); });
}
VideoString decimalVideoValue(std::int32_t value) {
    return detail::allocateVideoStorage([&] { return VideoString(std::to_string(value)); });
}
bool white(std::uint32_t c) {
    return (c >= 9 && c <= 13) || c == 32 || c == 0x85 || c == 0xa0 || c == 0x1680 ||
        (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
        c == 0x205f || c == 0x3000;
}
VideoString strip(VideoString value) {
    core::i18n::detail::LocaleCaseCursor cursor{value.view()};
    std::size_t first = value.size();
    while (!cursor.empty()) {
        const auto begin = cursor.offset;
        if (!white(cursor.take())) {
            first = begin;
            break;
        }
    }
    // std.string.stripRight independently walks the trailing code units. It
    // deliberately recognizes these two/three-byte patterns without validating
    // the interior string or applying the forward replacement decoder to them.
    const auto bytes = value.view();
    std::size_t last = value.size();
    const auto unit = [&](std::size_t i) { return static_cast<unsigned char>(bytes[i]); };
    while (last > first) {
        const auto i = last - 1;
        const auto tail = unit(i);
        if (tail <= 0x7f) {
            if (!white(tail)) break;
            --last;
        } else {
            if (i == first || (tail & 0xc0) != 0x80) break;
            const auto low = tail & 0x3f;
            const auto previous = unit(i - 1);
            if ((previous & 0xe0) == 0xc0) {
                if (!white(low + ((previous & 0x1f) << 6))) break;
                last -= 2;
                continue;
            }
            if (i - first == 1 || (previous & 0xc0) != 0x80) break;
            const auto head = unit(i - 2);
            if ((head & 0xf0) != 0xe0 ||
                !white(low + ((previous & 0x3f) << 6) + ((head & 0x0f) << 12))) break;
            last -= 3;
        }
    }
    return value.subslice(first, last);
}
compat::Slice<VideoString> splitLines(VideoString value) {
    compat::Slice<VideoString> result;
    const auto bytes = value.view();
    std::size_t begin = 0;
    for (std::size_t i = 0; i < bytes.size();) {
        std::size_t count = 0;
        const auto c = static_cast<unsigned char>(bytes[i]);
        if (c == '\r') count = i + 1 < bytes.size() && bytes[i + 1] == '\n' ? 2 : 1;
        else if (c == '\n' || c == '\v' || c == '\f') count = 1;
        else if (c == 0xc2 && i + 1 < bytes.size() && static_cast<unsigned char>(bytes[i + 1]) == 0x85) count = 2;
        else if (c == 0xe2 && i + 2 < bytes.size() && static_cast<unsigned char>(bytes[i + 1]) == 0x80 &&
            (static_cast<unsigned char>(bytes[i + 2]) == 0xa8 || static_cast<unsigned char>(bytes[i + 2]) == 0xa9)) count = 3;
        if (count) { result.append(value.subslice(begin, i)); i += count; begin = i; }
        else ++i;
    }
    if (begin < bytes.size()) result.append(value.subslice(begin, bytes.size()));
    return result;
}
bool parseEncoders(VideoString output) {
    const auto start = output.view().find(" -------");
    if (start == std::string_view::npos) return false;
    for (auto line : splitLines(strip(output.subslice(start + 8, output.size())))) {
        auto value = strip(line);
        if (value.size() < 3) throw compat::ngStaticError<compat::ArrayIndexError>(2, value.size(),
            VideoString::fromStaticLiteral("nijigenerate/io/videoexport.d"), 41);
        if (value.view()[2] != 'V' || value.view()[1] != 'E') continue;
        if (value.size() < 7) throw compat::ngStaticError<compat::ArraySliceError>(7, value.size(), value.size(),
            VideoString::fromStaticLiteral("nijigenerate/io/videoexport.d"), 47);
        value = value.subslice(7, value.size());
        std::size_t i = 0;
        while (i < value.size() && !white(static_cast<unsigned char>(value.view()[i]))) ++i;
        if (i == value.size()) continue;
        VideoCodec codec;
        if (i) codec.tag = copyVideoBytes(value.view().substr(0, i));
        codec.name = strip(value.subslice(i, value.size()));
        codecs.append(std::move(codec));
    }
    const auto before = [](const VideoCodec& a, const VideoCodec& b) { return a.name.view() < b.name.view(); };
    // Each source VideoCodec contains two D string descriptors, independently of native owner layout.
    constexpr auto shortThreshold = std::max(std::size_t{32}, std::size_t{1024} / (sizeof(void*) * 4));
    core::selector::detail::phobosQuickSort(std::span<VideoCodec>(codecs.data(), codecs.size()),
        before, codecs.size(), shortThreshold);
#ifndef NDEBUG
    for (std::size_t i = 1; i < codecs.size(); ++i)
        if (before(codecs[i], codecs[i - 1]))
            throw compat::ngStaticError<compat::AssertError>(VideoString::fromStaticLiteral("Failed to sort range of type VideoCodec[]"),
                VideoString::fromStaticLiteral("std/algorithm/sorting.d"), 1973);
#endif
    compat::Slice<VideoCodec> result{VideoCodec{VideoString::fromStaticLiteral("auto"),
        core::translation::ngTranslateRetained(VideoString::fromStaticLiteral("Automatic"))}};
    result.append(codecs);
    codecs = std::move(result);
    return true;
}
compat::Slice<VideoString> incBuildFFmpegCommand(VideoExportSettings settings) {
    auto file = settings.file;
    const auto codec = settings.codec.view();
    if (codec == "png" || codec == "jpeg" || codec == "jpg" || codec == "tga") {
        const auto bytes = file.view();
#if defined(_WIN32)
        const auto separator = bytes.find_last_of("/\\:");
#else
        const auto separator = bytes.find_last_of('/');
#endif
        const auto basename = separator == std::string_view::npos ? 0 : separator + 1;
        const auto dot = bytes.find_last_of('.');
        const auto extension = dot != std::string_view::npos && dot > basename ? dot : bytes.size();
        file = detail::allocateVideoStorage([&] {
            return VideoString(std::string(bytes.substr(0, extension)) + "-%d" + std::string(bytes.substr(extension)));
        });
    }
    using namespace core::translation;
    const std::array<TypedFormatArgument, 2> dimensions{ngFormatArgument(settings.width), ngFormatArgument(settings.height)};
    compat::Slice<VideoString> result{
        VideoString::fromStaticLiteral("ffmpeg"), VideoString::fromStaticLiteral("-y"),
        VideoString::fromStaticLiteral("-f"), VideoString::fromStaticLiteral("rawvideo"),
        VideoString::fromStaticLiteral("-vcodec"), VideoString::fromStaticLiteral("rawvideo"),
        VideoString::fromStaticLiteral("-pix_fmt"), VideoString::fromStaticLiteral("rgba"),
        VideoString::fromStaticLiteral("-s"), retainVideoBytes(ngFormatTyped("%sx%s", dimensions)),
        VideoString::fromStaticLiteral("-r"), decimalVideoValue(settings.framerate),
        VideoString::fromStaticLiteral("-i"), VideoString::fromStaticLiteral("-"),
        VideoString::fromStaticLiteral("-vframes"), decimalVideoValue(settings.frames),
        VideoString::fromStaticLiteral("-an"), VideoString::fromStaticLiteral("-pix_fmt"),
        settings.transparency ? VideoString::fromStaticLiteral("yuva420p")
            : VideoString::fromStaticLiteral("yuv420p")};
    if (codec != "auto") {
        result.append(VideoString::fromStaticLiteral("-vcodec")); result.append(settings.codec);
    }
    const auto options = strip(settings.ffmpegOptions);
    if (!options.empty()) {
        std::size_t begin = 0;
        for (;;) {
            const auto end = options.view().find(' ', begin);
            result.append(options.subslice(begin, end == std::string_view::npos ? options.size() : end));
            if (end == std::string_view::npos) break;
            begin = end + 1;
        }
    }
    result.append(file);
    return result;
}
void appendError(VideoString& value, const compat::ThrowableException& error) {
    if (error.msg.empty()) return;
    const auto previous = value;
    const auto message = error.msg;
    value = detail::allocateVideoStorage([&] {
        return VideoString(std::string(previous.view()) + std::string(message.view()));
    });
}
}
VideoEncodingContext::VideoEncodingContext(VideoExportSettings value) : settings(std::move(value)) {
    VideoEncodingContext::prepareSourceClassInfo();
    ffmpegLaunchOptions = incBuildFFmpegCommand(settings);
    try {
        ffmpegPipes = detail::launch(ffmpegLaunchOptions, false, true);
#if !defined(_WIN32)
        std::signal(SIGPIPE, SIG_IGN);
#endif
        isAlive = true;
    }
    catch (const compat::ThrowableException& error) { appendError(errors_, error); isAlive = false; }
}
VideoString VideoEncodingContext::errors() { return errors_; }
bool VideoEncodingContext::checkState() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    return isAlive && ffmpegPipes && !ffmpegPipes->inputError();
}
void VideoEncodingContext::encodeFrame(nicxlive::core::common::SharedArray<std::uint8_t> rgba) {
    encodeFrameBytes(std::span<const std::uint8_t>(rgba.data(), rgba.size()));
}
void VideoEncodingContext::encodeFrame(std::span<const std::uint8_t> rgba) {
    encodeFrameBytes(rgba);
}
void VideoEncodingContext::encodeFrameBytes(std::span<const std::uint8_t> rgba) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    try {
        if (!ffmpegPipes) throw compat::ThrowableError{
            VideoString::fromStaticLiteral("Child process' standard input stream hasn't been redirected."),
            VideoString::fromStaticLiteral("std/process.d"), 3330};
        ffmpegPipes->writeFrame(rgba);
        encoded = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(encoded) + 1u);
    } catch (const compat::ThrowableException& error) { appendError(errors_, error); isAlive = false; }
}
void VideoEncodingContext::end() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!ffmpegPipes) throw compat::ThrowableError{
        VideoString::fromStaticLiteral("Child process' standard input stream hasn't been redirected."),
        VideoString::fromStaticLiteral("std/process.d"), 3330};
    ffmpegPipes->closeInput();
}
bool VideoEncodingContext::hasTerminated() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!ffmpegPipes) throw compat::ngStaticError<compat::AssertError>(
        VideoString::fromStaticLiteral("Called tryWait on a null Pid."),
        VideoString::fromStaticLiteral("std/process.d"), 2708);
    return ffmpegPipes->poll();
}
float VideoEncodingContext::progress() { return static_cast<float>(encoded) / static_cast<float>(settings.frames); }

// object.Object defaults (Digital Mars 2000-2011), BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void VideoEncodingContext::prepareSourceClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        detail::allocateVideoStorage([] {
            ngPrepareSourceTypeSpellings<VideoEncodingContext>();
            (void)ngObjectTemplateArgumentName<VideoEncodingContext>();
            ngRegisterObjectTypeNames(typeid(VideoEncodingContext),
                VideoString::fromStaticLiteral(SourceObjectTypeNames<VideoEncodingContext>::className),
                VideoString::fromStaticLiteral(SourceObjectTypeNames<VideoEncodingContext>::templateArgumentName));
        });
        return true;
    }();
    (void)registered;
}
VideoString VideoEncodingContext::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t VideoEncodingContext::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool VideoEncodingContext::opEquals(std::shared_ptr<VideoEncodingContext> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int VideoEncodingContext::opCmp(std::shared_ptr<VideoEncodingContext>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = detail::allocateVideoStorage([&] {
        return VideoString(std::string("need opCmp for class ") + std::string(name.view()));
    });
    throw compat::ThrowableException(message,
        VideoString::fromStaticLiteral("object.d"), 195);
}
bool ngVideoEncodingContextObjectEquals(std::shared_ptr<VideoEncodingContext> left,
        std::shared_ptr<VideoEncodingContext> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right));
}

std::shared_ptr<VideoEncodingContext> incVideoExport(VideoExportSettings settings) {
    return std::allocate_shared<VideoEncodingContext>(
        nicxlive::core::d_runtime::SourceAllocator<VideoEncodingContext>{}, std::move(settings));
}
bool incVideoCanExport() { return hasffmpeg && !codecs.empty(); }
compat::Slice<VideoCodec>& incVideoCodecs() { return codecs; }
void incInitVideoExport() {
    try {
        auto output = detail::execute(compat::Slice<VideoString>{
            VideoString::fromStaticLiteral("ffmpeg"), VideoString::fromStaticLiteral("-codecs")});
        if (output.first == 0) hasffmpeg = parseEncoders(retainVideoBytes(std::move(output.second)));
    } catch (const compat::ThrowableException&) { hasffmpeg = false; }
}
}
