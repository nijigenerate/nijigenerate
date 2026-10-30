#include "depthmap_psd_enum_functions.hpp"
#include "detail/psd_depth_allocation.hpp"
#include <string_view>

namespace nijigenerate::io::depthmap_psd {
PsdDepthEnumParseError::PsdDepthEnumParseError(std::string text, std::source_location source)
    : ThrowableException(detail::allocateImportBytes([&] {
        return compat::RetainedString(std::move(text));
    }), {}, source) {}

PsdDepthEnumParseError::PsdDepthEnumParseError(std::string text,
        compat::RetainedString sourceFile, std::size_t sourceLine)
    : ThrowableException(detail::allocateImportBytes([&] {
        return compat::RetainedString(std::move(text));
    }), sourceFile, sourceLine) {}

namespace {
[[noreturn]] void enumParseFailure(std::string_view prefix, const compat::RetainedString& value,
        std::size_t sourceLine) {
    auto message = detail::allocateImportBytes([&] {
        std::string text(prefix);
        if (!value.empty()) text.append(value.data(), value.size());
        return text;
    });
    throw PsdDepthEnumParseError(std::move(message),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), sourceLine);
}
}

PsdDepthConvolution ngPsdDepthConvolutionFromString(compat::RetainedString value) {
    const auto text = value.view();
    if (text == "Nearest") return PsdDepthConvolution::Nearest;
    if (text == "Box3x3") return PsdDepthConvolution::Box3x3;
    if (text == "Box5x5") return PsdDepthConvolution::Box5x5;
    if (text == "Gaussian3x3") return PsdDepthConvolution::Gaussian3x3;
    if (text == "Gaussian5x5") return PsdDepthConvolution::Gaussian5x5;
    if (text == "Median3x3") return PsdDepthConvolution::Median3x3;
    if (text == "Frontmost3x3") return PsdDepthConvolution::Frontmost3x3;
    if (text == "Backmost3x3") return PsdDepthConvolution::Backmost3x3;
    if (text == "BoxCustom") return PsdDepthConvolution::BoxCustom;
    if (text == "GaussianCustom") return PsdDepthConvolution::GaussianCustom;
    if (text == "MedianCustom") return PsdDepthConvolution::MedianCustom;
    if (text == "FrontmostCustom") return PsdDepthConvolution::FrontmostCustom;
    if (text == "BackmostCustom") return PsdDepthConvolution::BackmostCustom;
    enumParseFailure("Unknown PSD depth convolution: ", value, 429);
}

PsdDepthChannel ngPsdDepthChannelFromString(compat::RetainedString value) {
    const auto text = value.view();
    if (text == "AverageRGB") return PsdDepthChannel::AverageRGB;
    if (text == "R") return PsdDepthChannel::R;
    if (text == "G") return PsdDepthChannel::G;
    if (text == "B") return PsdDepthChannel::B;
    if (text == "Luminance") return PsdDepthChannel::Luminance;
    enumParseFailure("Unknown PSD depth channel: ", value, 440);
}

PsdDepthMissingPolicy ngPsdDepthMissingPolicyFromString(compat::RetainedString value) {
    const auto text = value.view();
    if (text == "KeepExisting") return PsdDepthMissingPolicy::KeepExisting;
    if (text == "SetZero") return PsdDepthMissingPolicy::SetZero;
    if (text == "SetBack") return PsdDepthMissingPolicy::SetBack;
    if (text == "SkipGrid") return PsdDepthMissingPolicy::SkipGrid;
    enumParseFailure("Unknown PSD depth missing policy: ", value, 450);
}

#define PSD_NAME_CASE(Type, Member, Text) \
    case Type::Member: return compat::RetainedString::fromStaticLiteral(Text)

compat::RetainedString ngPsdDepthConvolutionName(PsdDepthConvolution value) {
    switch (value) {
        PSD_NAME_CASE(PsdDepthConvolution, Nearest, "Nearest");
        PSD_NAME_CASE(PsdDepthConvolution, Box3x3, "Box3x3");
        PSD_NAME_CASE(PsdDepthConvolution, Box5x5, "Box5x5");
        PSD_NAME_CASE(PsdDepthConvolution, Gaussian3x3, "Gaussian3x3");
        PSD_NAME_CASE(PsdDepthConvolution, Gaussian5x5, "Gaussian5x5");
        PSD_NAME_CASE(PsdDepthConvolution, Median3x3, "Median3x3");
        PSD_NAME_CASE(PsdDepthConvolution, Frontmost3x3, "Frontmost3x3");
        PSD_NAME_CASE(PsdDepthConvolution, Backmost3x3, "Backmost3x3");
        PSD_NAME_CASE(PsdDepthConvolution, BoxCustom, "BoxCustom");
        PSD_NAME_CASE(PsdDepthConvolution, GaussianCustom, "GaussianCustom");
        PSD_NAME_CASE(PsdDepthConvolution, MedianCustom, "MedianCustom");
        PSD_NAME_CASE(PsdDepthConvolution, FrontmostCustom, "FrontmostCustom");
        PSD_NAME_CASE(PsdDepthConvolution, BackmostCustom, "BackmostCustom");
    }
    throw compat::ngStaticError<PsdDepthEnumSwitchError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 455);
}

compat::RetainedString ngPsdDepthChannelName(PsdDepthChannel value) {
    switch (value) {
        PSD_NAME_CASE(PsdDepthChannel, AverageRGB, "AverageRGB");
        PSD_NAME_CASE(PsdDepthChannel, R, "R");
        PSD_NAME_CASE(PsdDepthChannel, G, "G");
        PSD_NAME_CASE(PsdDepthChannel, B, "B");
        PSD_NAME_CASE(PsdDepthChannel, Luminance, "Luminance");
    }
    throw compat::ngStaticError<PsdDepthEnumSwitchError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 473);
}

compat::RetainedString ngPsdDepthMissingPolicyName(PsdDepthMissingPolicy value) {
    switch (value) {
        PSD_NAME_CASE(PsdDepthMissingPolicy, KeepExisting, "KeepExisting");
        PSD_NAME_CASE(PsdDepthMissingPolicy, SetZero, "SetZero");
        PSD_NAME_CASE(PsdDepthMissingPolicy, SetBack, "SetBack");
        PSD_NAME_CASE(PsdDepthMissingPolicy, SkipGrid, "SkipGrid");
    }
    throw compat::ngStaticError<PsdDepthEnumSwitchError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 483);
}

compat::RetainedString ngPsdDepthCompositionModeName(PsdDepthCompositionMode value) {
    switch (value) {
        PSD_NAME_CASE(PsdDepthCompositionMode, Unknown, "Unknown");
        PSD_NAME_CASE(PsdDepthCompositionMode, OneToOne, "1:1");
        PSD_NAME_CASE(PsdDepthCompositionMode, NToOne, "N:1");
        PSD_NAME_CASE(PsdDepthCompositionMode, NToN, "N:N");
        PSD_NAME_CASE(PsdDepthCompositionMode, UnsupportedOneToN, "1:N");
    }
    throw compat::ngStaticError<PsdDepthEnumSwitchError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 492);
}

compat::RetainedString ngPsdDepthCompositeSourceKindName(PsdDepthCompositeSourceKind value) {
    switch (value) {
        PSD_NAME_CASE(PsdDepthCompositeSourceKind, Unknown, "Unknown");
        PSD_NAME_CASE(PsdDepthCompositeSourceKind, ActiveArtTargets, "ActiveArtTargets");
        PSD_NAME_CASE(PsdDepthCompositeSourceKind, FlatImage, "FlatImage");
        PSD_NAME_CASE(PsdDepthCompositeSourceKind, PsdLayers, "PsdLayers");
    }
    throw compat::ngStaticError<PsdDepthEnumSwitchError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 502);
}

#undef PSD_NAME_CASE
}
