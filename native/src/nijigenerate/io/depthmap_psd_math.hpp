#pragma once

#include <nijigenerate/io/depthmap_psd_types.hpp>
#include <cstddef>

namespace nijigenerate::io::depthmap_psd {

PsdDepthCompositionMode ngPsdDepthCompositionModeForCounts(std::size_t colorLayerCount, std::size_t depthLayerCount);

float ngPsdDepthRectOverlapScore(float leftA, float topA, float widthA, float heightA,
                               float leftB, float topB, float widthB, float heightB);

}
