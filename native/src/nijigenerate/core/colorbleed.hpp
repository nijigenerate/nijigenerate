#pragma once
#include <texture.hpp>
#include "colorbleed_detail.hpp"
#include <memory>

namespace nijigenerate::core::colorbleed {
using detail::Pixel;
using detail::Mask;
using detail::MaskIterator;
void incColorBleedPixels(nicxlive::core::ShallowTexture* texture, int maxIterations = 32);
void incColorBleedPixels(nicxlive::core::Texture& texture, int maxIterations = 32);
void incColorBleedPixels(std::shared_ptr<nicxlive::core::Texture> texture, int maxIterations = 32);
}
