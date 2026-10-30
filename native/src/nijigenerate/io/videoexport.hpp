#pragma once
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/compat/slice.hpp"
#include <common/shared_array.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <tuple>

namespace nijigenerate::io::videoexport {
using VideoString = compat::RetainedString;
struct VideoCodec { VideoString tag; VideoString name; };
struct VideoExportSettings {
    VideoString codec;
    std::int32_t framerate = 0;
    VideoString file;
    std::int32_t frames = 0;
    float width = std::numeric_limits<float>::quiet_NaN();
    float height = std::numeric_limits<float>::quiet_NaN();
    bool transparency = false;
    VideoString ffmpegOptions;
};
namespace detail { struct ProcessPipes; }
class VideoEncodingContext : public std::enable_shared_from_this<VideoEncodingContext> {
public:
    explicit VideoEncodingContext(VideoExportSettings settings);
    virtual ~VideoEncodingContext() = default;
    VideoEncodingContext(const VideoEncodingContext&) = delete;
    VideoEncodingContext& operator=(const VideoEncodingContext&) = delete;
    virtual VideoString errors();
    virtual bool checkState();
    virtual void encodeFrame(nicxlive::core::common::SharedArray<std::uint8_t> rgba);
    // Explicit native adapter; the Source entry retains its mutable array above.
    virtual void encodeFrame(std::span<const std::uint8_t> rgba);
    virtual void end();
    virtual bool hasTerminated();
    virtual float progress();
    virtual VideoString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<VideoEncodingContext> other);
    virtual int opCmp(std::shared_ptr<VideoEncodingContext> other);
protected:
    virtual void prepareSourceClassInfo();
private:
    void encodeFrameBytes(std::span<const std::uint8_t> rgba);
    VideoExportSettings settings;
    compat::Slice<VideoString> ffmpegLaunchOptions;
    std::shared_ptr<detail::ProcessPipes> ffmpegPipes;
    bool isAlive = false;
    VideoString errors_;
    std::int32_t encoded = 0;
};
bool ngVideoEncodingContextObjectEquals(std::shared_ptr<VideoEncodingContext> left,
    std::shared_ptr<VideoEncodingContext> right);
std::shared_ptr<VideoEncodingContext> incVideoExport(VideoExportSettings settings);
bool incVideoCanExport();
compat::Slice<VideoCodec>& incVideoCodecs();
void incInitVideoExport();
}

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::io::videoexport::VideoCodec> {
    using Value = ::nijigenerate::io::videoexport::VideoCodec;
    inline static constexpr std::string_view typeName = "VideoCodec";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("tag", offsetof(Value, tag), &Value::tag),
            ngFormatAggregateMember("name", offsetof(Value, name), &Value::name)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::io::videoexport::VideoExportSettings> {
    using Value = ::nijigenerate::io::videoexport::VideoExportSettings;
    inline static constexpr std::string_view typeName = "VideoExportSettings";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("codec", offsetof(Value, codec), &Value::codec),
            ngFormatAggregateMember("framerate", offsetof(Value, framerate), &Value::framerate),
            ngFormatAggregateMember("file", offsetof(Value, file), &Value::file),
            ngFormatAggregateMember("frames", offsetof(Value, frames), &Value::frames),
            ngFormatAggregateMember("width", offsetof(Value, width), &Value::width),
            ngFormatAggregateMember("height", offsetof(Value, height), &Value::height),
            ngFormatAggregateMember("transparency", offsetof(Value, transparency), &Value::transparency),
            ngFormatAggregateMember("ffmpegOptions", offsetof(Value, ffmpegOptions), &Value::ffmpegOptions)};
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::io::videoexport::VideoCodec> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.videoexport.VideoCodec)";
    inline static constexpr char unqualifiedArgument[] = "(VideoCodec)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.io.videoexport.VideoCodec";
    inline static constexpr char unqualifiedElement[] = "VideoCodec";
};
template<> struct SourceTypeSpellings<::nijigenerate::io::videoexport::VideoExportSettings> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.videoexport.VideoExportSettings)";
    inline static constexpr char unqualifiedArgument[] = "(VideoExportSettings)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.io.videoexport.VideoExportSettings";
    inline static constexpr char unqualifiedElement[] = "VideoExportSettings";
};
template<> struct SourceTypeSpellings<::nijigenerate::io::videoexport::VideoEncodingContext> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.videoexport.VideoEncodingContext)";
    inline static constexpr char unqualifiedArgument[] = "(VideoEncodingContext)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.io.videoexport.VideoEncodingContext";
    inline static constexpr char unqualifiedElement[] = "VideoEncodingContext";
};
template<> struct SourceObjectTypeNames<::nijigenerate::io::videoexport::VideoEncodingContext> {
    inline static constexpr char className[] = "nijigenerate.io.videoexport.VideoEncodingContext";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.io.videoexport.VideoEncodingContext)";
};
}
