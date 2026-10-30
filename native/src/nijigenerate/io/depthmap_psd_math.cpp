#include "depthmap_psd_math.hpp"
#include <algorithm>

namespace nijigenerate::io::depthmap_psd {

PsdDepthCompositionMode ngPsdDepthCompositionModeForCounts(std::size_t colorLayerCount, std::size_t depthLayerCount) {
    if (colorLayerCount == 1 && depthLayerCount > 1) return PsdDepthCompositionMode::UnsupportedOneToN;
    if (colorLayerCount == 1 && depthLayerCount == 1) return PsdDepthCompositionMode::OneToOne;
    if (colorLayerCount > 1 && depthLayerCount == 1) return PsdDepthCompositionMode::NToOne;
    if (colorLayerCount > 1 && depthLayerCount > 1) return PsdDepthCompositionMode::NToN;
    return PsdDepthCompositionMode::Unknown;
}

float ngPsdDepthRectOverlapScore(float leftA, float topA, float widthA, float heightA,
                               float leftB, float topB, float widthB, float heightB) {
    if (widthA <= 0 || heightA <= 0 || widthB <= 0 || heightB <= 0) return 0.0f;
    const float rightA = leftA + widthA;
    const float bottomA = topA + heightA;
    const float rightB = leftB + widthB;
    const float bottomB = topB + heightB;
    // Phobos's two-float min/max select the first operand on unordered/equal
    // comparisons, as std::min/max do. fmin/fmax would change the NaN behavior.
    const float minimumRight = std::min(rightA, rightB);
    const float maximumLeft = std::max(leftA, leftB);
    const float overlapWidth = minimumRight - maximumLeft;
    const float minimumBottom = std::min(bottomA, bottomB);
    const float maximumTop = std::max(topA, topB);
    const float overlapHeight = minimumBottom - maximumTop;
    if (overlapWidth <= 0 || overlapHeight <= 0) return 0.0f;
    const float overlapArea = overlapWidth * overlapHeight;
    const float areaA = widthA * heightA;
    const float areaB = widthB * heightB;
    const float combinedArea = areaA + areaB;
    const float unionArea = combinedArea - overlapArea;
    return unionArea > 0 ? overlapArea / unionArea : 0.0f;
}

}
