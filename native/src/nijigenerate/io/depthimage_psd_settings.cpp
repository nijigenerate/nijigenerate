#include "depthimage.hpp"

#include <nijigenerate/compat/switch_error.hpp>

namespace nijigenerate::io::depthimage {
namespace {

depthmap_psd::PsdDepthChannel toPsdChannel(DepthImageChannel channel) {
    using Target = depthmap_psd::PsdDepthChannel;
    switch (channel) {
        case DepthImageChannel::AverageRGB: return Target::AverageRGB;
        case DepthImageChannel::R: return Target::R;
        case DepthImageChannel::G: return Target::G;
        case DepthImageChannel::B: return Target::B;
        case DepthImageChannel::Luminance: return Target::Luminance;
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 47);
}

depthmap_psd::PsdDepthConvolution toPsdConvolution(DepthImageConvolution convolution) {
    using Target = depthmap_psd::PsdDepthConvolution;
    switch (convolution) {
        case DepthImageConvolution::Nearest: return Target::Nearest;
        case DepthImageConvolution::Box3x3: return Target::Box3x3;
        case DepthImageConvolution::Box5x5: return Target::Box5x5;
        case DepthImageConvolution::Gaussian3x3: return Target::Gaussian3x3;
        case DepthImageConvolution::Gaussian5x5: return Target::Gaussian5x5;
        case DepthImageConvolution::Median3x3: return Target::Median3x3;
        case DepthImageConvolution::Frontmost3x3: return Target::Frontmost3x3;
        case DepthImageConvolution::Backmost3x3: return Target::Backmost3x3;
        case DepthImageConvolution::BoxCustom: return Target::BoxCustom;
        case DepthImageConvolution::GaussianCustom: return Target::GaussianCustom;
        case DepthImageConvolution::MedianCustom: return Target::MedianCustom;
        case DepthImageConvolution::FrontmostCustom: return Target::FrontmostCustom;
        case DepthImageConvolution::BackmostCustom: return Target::BackmostCustom;
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthimage.d"), 62);
}

} // namespace

depthmap_psd::PsdDepthImportSettings ngDepthImageToPsdSettings(DepthImageSampleSettings settings) {
    depthmap_psd::PsdDepthImportSettings result;
    result.invert = settings.invert;
    result.backDepth = settings.backDepth;
    result.frontDepth = settings.frontDepth;
    result.depthScale = settings.depthScale;
    result.alphaThreshold = settings.alphaThreshold;
    result.customRadius = settings.customRadius;
    result.convolution = toPsdConvolution(settings.convolution);
    result.channel = toPsdChannel(settings.channel);
    return result;
}

} // namespace nijigenerate::io::depthimage
