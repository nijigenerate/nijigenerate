#pragma once

#include "colorbleed_detail.hpp"
#include <texture.hpp>
#include <span>
#include <type_traits>
#include <utility>

namespace nijigenerate::core::colorbleed::detail {

// The D union overlays slice descriptors, not byte and Pixel elements. Its
// apparent Pixel length is the unchanged numeric byte length. Actual accesses
// use the existing packed-byte kernel with that same apparent length and owner.
class TexDataPixelView {
public:
    explicit TexDataPixelView(nicxlive::core::SourceTextureBytes bytes) : bytes_(std::move(bytes)) {}
    std::uint8_t* data() const { return bytes_.data(); }
    std::size_t size() const { return bytes_.size(); }
    Pixel get(std::size_t index) const { return PixelBytes(bytes_).get(index); }
    void set(std::size_t index, Pixel value) const { PixelBytes(bytes_).set(index, value); }
    nicxlive::core::SourceTextureBytes byteDescriptor() const { return bytes_; }
    operator PixelBytes() const { return PixelBytes(bytes_); }

private:
    // A const descriptor view does not const-qualify the source Pixel elements.
    mutable nicxlive::core::SourceTextureBytes bytes_;
};

template<class Scope>
class TexData {
public:
    TexData() = default;
    explicit TexData(nicxlive::core::SourceTextureBytes bytes) : descriptor_(std::move(bytes)) {}
    nicxlive::core::SourceTextureBytes& bytes() { return descriptor_; }
    const nicxlive::core::SourceTextureBytes& bytes() const { return descriptor_; }
    TexDataPixelView pixels() const { return TexDataPixelView(descriptor_); }
    void pixels(TexDataPixelView value) { descriptor_ = value.byteDescriptor(); }

private:
    nicxlive::core::SourceTextureBytes descriptor_;
};

struct ShallowTexDataScope {};
struct TextureTexDataScope {};
using ShallowTexData = TexData<ShallowTexDataScope>;
using TextureTexData = TexData<TextureTexDataScope>;
static_assert(!std::is_same_v<ShallowTexData, TextureTexData>);

} // namespace nijigenerate::core::colorbleed::detail

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::colorbleed::detail::ShallowTexData> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.colorbleed.incColorBleedPixels.TexData)";
    inline static constexpr char unqualifiedArgument[] = "(TexData)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.colorbleed.incColorBleedPixels.TexData";
    inline static constexpr char unqualifiedElement[] = "TexData";
};
template<> struct SourceTypeSpellings<::nijigenerate::core::colorbleed::detail::TextureTexData> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.colorbleed.incColorBleedPixels.TexData)";
    inline static constexpr char unqualifiedArgument[] = "(TexData)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.colorbleed.incColorBleedPixels.TexData";
    inline static constexpr char unqualifiedElement[] = "TexData";
};
} // namespace nicxlive::core::d_runtime
