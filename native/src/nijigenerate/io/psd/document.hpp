#pragma once
// Copyright 2021, Inochi2D Project. BSD-2-Clause: native/port/licenses/psd-d.txt.
#include "nijigenerate/io/psd/layer.hpp"
#include "nijigenerate/io/psd/image_resources.hpp"

namespace nijigenerate::io::psd {
enum class ColorMode : std::uint16_t { Bitmap, Grayscale, Indexed, RGB, CMYK, Multichannel, Duotone, Lab };
struct PSD {
    std::size_t colorModeDataSectionOffset{}, colorModeDataSectionLength{};
    std::size_t imageResourceSectionOffset{}, imageResourceSectionLength{};
    std::size_t layerMaskInfoSectionOffset{}, layerMaskInfoSectionLength{};
    std::size_t imageDataSectionOffset{}, imageDataSectionLength{};
    std::int16_t channels{};
    std::int32_t width{}, height{};
    std::uint16_t bitsPerChannel{};
    ColorMode colorMode{ColorMode::Bitmap};
    compat::Slice<std::uint8_t> colorData;
    bool mergedAlpha{};
    compat::Slice<Layer> layers;
    ImageResourcesData imageResourcesData;
    compat::Slice<std::uint8_t> fullImage;
};
PSD parseDocument(compat::RetainedString fileName);
PSD parseDocument(RetainedFile& file);
} // namespace nijigenerate::io::psd
