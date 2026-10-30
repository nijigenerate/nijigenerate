#include <nijigenerate/viewport/depth/draw/layer.hpp>

namespace nijigenerate::viewport::depth::draw {

io::depthimage::DepthImageSampleSettings DepthDrawLayer::sampleSettings() const {
    io::depthimage::DepthImageSampleSettings settings;
    settings.invert = invert;
    settings.backDepth = backDepth;
    settings.frontDepth = frontDepth;
    settings.depthScale = sampleDepthScale;
    settings.alphaThreshold = alphaThreshold;
    settings.customRadius = io::depthimage::ngNormalizeDepthImageCustomRadius(customRadius);
    settings.convolution = convolution;
    settings.channel = channel;
    return settings;
}

float DepthDrawLayer::applyZTransform(float rawDepth) const {
    return rawDepth * zScale + zOffset;
}

std::int32_t ngNormalizeDepthDrawContourThickness(std::int32_t thickness) {
    if (thickness < 1) return 1;
    if (thickness > DepthDrawMaxContourThickness) return DepthDrawMaxContourThickness;
    return thickness;
}

io::depthimage::DepthDrawAlphaDepthFocusedRule ngNormalizeDepthDrawFocusedRule(
    io::depthimage::DepthDrawAlphaDepthFocusedRule rule, std::int32_t width, std::int32_t height) {
    width = width < 0 ? 0 : width;
    height = height < 0 ? 0 : height;
    if (rule.x < 0) rule.x = 0;
    if (rule.x > width) rule.x = width;
    if (rule.y < 0) rule.y = 0;
    if (rule.y > height) rule.y = height;
    if (rule.w < 0) rule.w = 0;
    if (rule.w > width - rule.x) rule.w = width - rule.x;
    if (rule.h < 0) rule.h = 0;
    if (rule.h > height - rule.y) rule.h = height - rule.y;
    if (rule.lift < 0) rule.lift = 0;
    if (rule.lift > 255) rule.lift = 255;
    if (rule.radius < 0) rule.radius = 0;
    if (rule.radius > DepthDrawMaxFocusedRuleRadius) rule.radius = DepthDrawMaxFocusedRuleRadius;
    return rule;
}

} // namespace nijigenerate::viewport::depth::draw
