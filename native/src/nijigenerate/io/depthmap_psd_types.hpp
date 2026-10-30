#pragma once

#include "depthmap_psd_names.hpp"
#include "depthmap_psd_composite_aliases.hpp"

#include <cstdint>
#include <cstddef>
#include <nijigenerate/compat/retained_string.hpp>
#include <common/string_key_map.hpp>
#include <limits>
#include <array>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::io::depthmap_psd {

enum class PsdDepthConvolution : std::int32_t {
    Nearest, Box3x3, Box5x5, Gaussian3x3, Gaussian5x5, Median3x3,
    Frontmost3x3, Backmost3x3, BoxCustom, GaussianCustom, MedianCustom,
    FrontmostCustom, BackmostCustom,
};

enum class PsdDepthChannel : std::int32_t { AverageRGB, R, G, B, Luminance };
enum class PsdDepthMissingPolicy : std::int32_t { KeepExisting, SetZero, SetBack, SkipGrid };
enum class PsdDepthCompositionMode : std::int32_t { Unknown, OneToOne, NToOne, NToN, UnsupportedOneToN };
enum class PsdDepthCompositeSourceKind : std::int32_t { Unknown, ActiveArtTargets, FlatImage, PsdLayers };

inline constexpr std::uint64_t PsdDepthMaxRetainedBytes = 256ULL * 1024ULL * 1024ULL;
inline constexpr std::uint64_t PsdDepthRetainedDepthLayerBytesPerPixel = 40;
inline constexpr std::uint64_t PsdDepthRetainedColorLayerBytesPerPixel = 24;

struct PsdDepthImportSettings {
    bool invert = false;
    float backDepth = -1.0f;
    float frontDepth = 1.0f;
    float depthScale = 1.0f;
    float alphaThreshold = 0.01f;
    bool matchDirectGridName = true;
    std::size_t explicitColorLayerCount = 0;
    int customRadius = 3;
    PsdDepthConvolution convolution = PsdDepthConvolution::Gaussian3x3;
    PsdDepthChannel channel = PsdDepthChannel::AverageRGB;
    PsdDepthMissingPolicy missingPolicy = PsdDepthMissingPolicy::KeepExisting;
    bool zeroDepthIsMissing = false;
    bool repairContourBand = false;
    bool smoothWavySurface = false;
    bool useGpuComposition = false;
    compat::RetainedString colorSourcePath;
    nicxlive::core::common::StringKeyMap<compat::RetainedString> layerTargetGridUuidOverrides;
    nicxlive::core::common::StringKeyMap<bool> ignoredLayerPaths;
    nicxlive::core::common::StringKeyMap<bool> disabledGridUuids;
    nicxlive::core::common::StringKeyMap<bool> disabledGridLayerKeys;
};

inline bool ngReservePsdDepthRetainedLayer(std::int64_t width, std::int64_t height,
        std::uint64_t bytesPerPixel, std::uint64_t& retainedBytes) {
    if (width <= 0 || height <= 0 || bytesPerPixel == 0) return false;
    const auto unsignedWidth = static_cast<std::uint64_t>(width);
    const auto unsignedHeight = static_cast<std::uint64_t>(height);
    if (unsignedWidth > std::numeric_limits<std::uint64_t>::max() / unsignedHeight) return false;
    const auto pixelCount = unsignedWidth * unsignedHeight;
    if (pixelCount > std::numeric_limits<std::uint64_t>::max() / bytesPerPixel) return false;
    const auto requiredBytes = pixelCount * bytesPerPixel;
    if (retainedBytes > PsdDepthMaxRetainedBytes ||
        requiredBytes > PsdDepthMaxRetainedBytes - retainedBytes) return false;
    retainedBytes += requiredBytes;
    return true;
}

} // namespace nijigenerate::io::depthmap_psd

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<io::depthmap_psd::PsdDepthConvolution> {
    using Value = io::depthmap_psd::PsdDepthConvolution;
    inline static constexpr std::string_view name = "PsdDepthConvolution";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::Nearest, "Nearest"},
        FormatEnumMember<Value>{Value::Box3x3, "Box3x3"},
        FormatEnumMember<Value>{Value::Box5x5, "Box5x5"},
        FormatEnumMember<Value>{Value::Gaussian3x3, "Gaussian3x3"},
        FormatEnumMember<Value>{Value::Gaussian5x5, "Gaussian5x5"},
        FormatEnumMember<Value>{Value::Median3x3, "Median3x3"},
        FormatEnumMember<Value>{Value::Frontmost3x3, "Frontmost3x3"},
        FormatEnumMember<Value>{Value::Backmost3x3, "Backmost3x3"},
        FormatEnumMember<Value>{Value::BoxCustom, "BoxCustom"},
        FormatEnumMember<Value>{Value::GaussianCustom, "GaussianCustom"},
        FormatEnumMember<Value>{Value::MedianCustom, "MedianCustom"},
        FormatEnumMember<Value>{Value::FrontmostCustom, "FrontmostCustom"},
        FormatEnumMember<Value>{Value::BackmostCustom, "BackmostCustom"}};
};

template<> struct FormatEnumTraits<io::depthmap_psd::PsdDepthChannel> {
    using Value = io::depthmap_psd::PsdDepthChannel;
    inline static constexpr std::string_view name = "PsdDepthChannel";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::AverageRGB, "AverageRGB"},
        FormatEnumMember<Value>{Value::R, "R"},
        FormatEnumMember<Value>{Value::G, "G"},
        FormatEnumMember<Value>{Value::B, "B"},
        FormatEnumMember<Value>{Value::Luminance, "Luminance"}};
};

template<> struct FormatEnumTraits<io::depthmap_psd::PsdDepthMissingPolicy> {
    using Value = io::depthmap_psd::PsdDepthMissingPolicy;
    inline static constexpr std::string_view name = "PsdDepthMissingPolicy";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::KeepExisting, "KeepExisting"},
        FormatEnumMember<Value>{Value::SetZero, "SetZero"},
        FormatEnumMember<Value>{Value::SetBack, "SetBack"},
        FormatEnumMember<Value>{Value::SkipGrid, "SkipGrid"}};
};

template<> struct FormatEnumTraits<io::depthmap_psd::PsdDepthCompositionMode> {
    using Value = io::depthmap_psd::PsdDepthCompositionMode;
    inline static constexpr std::string_view name = "PsdDepthCompositionMode";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::Unknown, "Unknown"},
        FormatEnumMember<Value>{Value::OneToOne, "OneToOne"},
        FormatEnumMember<Value>{Value::NToOne, "NToOne"},
        FormatEnumMember<Value>{Value::NToN, "NToN"},
        FormatEnumMember<Value>{Value::UnsupportedOneToN, "UnsupportedOneToN"}};
};

template<> struct FormatEnumTraits<io::depthmap_psd::PsdDepthCompositeSourceKind> {
    using Value = io::depthmap_psd::PsdDepthCompositeSourceKind;
    inline static constexpr std::string_view name = "PsdDepthCompositeSourceKind";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::Unknown, "Unknown"},
        FormatEnumMember<Value>{Value::ActiveArtTargets, "ActiveArtTargets"},
        FormatEnumMember<Value>{Value::FlatImage, "FlatImage"},
        FormatEnumMember<Value>{Value::PsdLayers, "PsdLayers"}};
};

} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
#define NG_PSD_DEPTH_ENUM_SOURCE_NAMES(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::io::depthmap_psd::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.depthmap_psd." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.io.depthmap_psd." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_PSD_DEPTH_ENUM_SOURCE_NAMES(PsdDepthConvolution)
NG_PSD_DEPTH_ENUM_SOURCE_NAMES(PsdDepthChannel)
NG_PSD_DEPTH_ENUM_SOURCE_NAMES(PsdDepthMissingPolicy)
NG_PSD_DEPTH_ENUM_SOURCE_NAMES(PsdDepthCompositionMode)
NG_PSD_DEPTH_ENUM_SOURCE_NAMES(PsdDepthCompositeSourceKind)
#undef NG_PSD_DEPTH_ENUM_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime
