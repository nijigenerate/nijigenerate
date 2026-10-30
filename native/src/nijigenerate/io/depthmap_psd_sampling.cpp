// Derived from the independent depthmap_psd.d sampling and coverage helpers.
// Copyright 2025 nijigenerate Project. Distributed under BSD-2-Clause.
#include "depthmap_psd_sampling.hpp"
#include "depthmap_psd_helpers.hpp"
#include "depthmap_psd_targets.hpp"
#include "depthmap_psd_loading.hpp"
#include "detail/psd_pixel_copy.hpp"
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <stdexcept>
#include "depthmap_psd_enum_functions.hpp"
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace nijigenerate::io::depthmap_psd::detail {
using namespace depthsample;
using namespace depthimage;
using Vec2 = nicxlive::core::math::Vec2;
using Mat4 = nicxlive::core::math::Mat4;
namespace {
Vec2 transformXY(const Mat4& matrix, Vec2 value) {
    const auto result = core::math::ngTransformHomogeneousPoint(matrix, {value.x, value.y, 0});
    return {result.x, result.y};
}
}
float pixelDepth01(const compat::Slice<std::uint8_t> data, size_t index, PsdDepthImportSettings& settings) {
    if (index + 2 >= data.size()) return 0.0f;
    return ngDepthSamplePixelDepth01({data.data(), data.size()}, index, sampleChannel(settings.channel), settings.invert);
}

float pixelDepth(const compat::Slice<std::uint8_t> data, size_t index, PsdDepthImportSettings& settings) {
    if (index + 2 >= data.size()) return settings.backDepth;
    return ngDepthSamplePixelDepth(
        {data.data(), data.size()},
        index,
        sampleChannel(settings.channel),
        settings.invert,
        settings.backDepth,
        settings.frontDepth,
        settings.depthScale
    );
}

float layerToSourceX(DepthLayerImage& layer, float documentX) {
    return documentX - (float)layer.left;
}

float layerToSourceY(DepthLayerImage& layer, float documentY) {
    return documentY - (float)layer.top;
}

float sampledDepth01(float value, PsdDepthImportSettings& settings) {
    return ngDepthSampleValueToDepth01(value, settings.backDepth, settings.frontDepth, settings.depthScale);
}

float layerOpacity01(std::uint8_t opacity) {
    return ngDepthSampleOpacity01(opacity);
}

float effectiveAlpha(std::span<const std::uint8_t> rgba, size_t index, float opacity) {
    return ngDepthSampleEffectiveAlpha(rgba, index, opacity);
}

float coverageAlpha(DepthLayerImage& layer, int x, int y) {
    auto cached = coverageCacheValue(layer.coverageAlphaCache, layer, x, y);
    if (cached >= 0.0f) return cached;
    if (layer.coverageSources.size() > 0) return coverageSourcesAlpha(layer, x, y, true);
    return coveragePixelAlpha(layer, x, y) * layer.coverageOpacity;
}

bool coverageReliable(DepthLayerImage& layer, int x, int y) {
    auto cached = coverageCacheValue(layer.coverageGateCache, layer, x, y);
    if (cached >= 0.0f) return cached > 0.5f;
    if (layer.coverageSources.size() > 0) return coverageSourcesAlpha(layer, x, y, false) > 0.5f;
    if (layer.coverageData.size() == 0) return true;
    if (layer.coverageUsesMesh) return layer.coverageData.size() == 0 || coveragePixelAlpha(layer, x, y) > 0.5f;
    return ngDepthImageCoverageReliableAt(
        x,
        y,
        layer.width,
        layer.height,
        layer.coverageData,
        layer.coverageWidth,
        layer.coverageHeight,
        layer.coverageChannels
    );
}

float coverageCacheValue(compat::Slice<float>& cache, DepthLayerImage& layer, int x, int y) {
    if (cache.size() != (size_t)layer.width * (size_t)layer.height) return -1.0f;
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return 0.0f;
    return cache[(size_t)y * (size_t)layer.width + (size_t)x];
}

float coverageSourcesAlpha(DepthLayerImage& layer, int x, int y, bool includeOpacity) {
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return 0.0f;

    auto world = Vec2(
        (float)depthsample::detail::add32(layer.left, x) - (float)layer.documentWidth / 2.0f,
        (float)depthsample::detail::add32(layer.top, y) - (float)layer.documentHeight / 2.0f
    );
    float combined = 0.0f;
    for (auto sources = layer.coverageSources; auto& source : sources) {
        if (source.data.size() == 0 || source.width <= 0 || source.height <= 0) continue;
        auto local = transformXY(source.worldToLocal, world);
        auto alpha = sampleCoverageSourceAlpha(source, local);
        if (alpha <= 0.0f) continue;
        if (includeOpacity) alpha *= source.opacity;
        combined = ngDepthImageCompositeAlpha(combined, alpha);
        if (combined >= 1.0f) return 1.0f;
    }
    return combined;
}

float coveragePixelAlpha(DepthLayerImage& layer, int x, int y) {
    if (layer.coverageData.size() == 0 || layer.coverageWidth <= 0 || layer.coverageHeight <= 0) {
        return 1.0f;
    }
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return 0.0f;
    if (layer.coverageUsesMesh) {
        auto world = Vec2(
            (float)depthsample::detail::add32(layer.left, x) - (float)layer.documentWidth / 2.0f,
            (float)depthsample::detail::add32(layer.top, y) - (float)layer.documentHeight / 2.0f
        );
        auto local = transformXY(layer.coverageWorldToLocal, world);
        auto uv = Vec2(
            local.x / (float)layer.coverageWidth + 0.5f,
            local.y / (float)layer.coverageHeight + 0.5f
        );
        if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) return 0.0f;
        return sampleCoverageAlphaAtUv(layer, uv);
    }
    return ngDepthImageCoverageAlphaAt(
        x,
        y,
        layer.width,
        layer.height,
        layer.coverageData,
        layer.coverageWidth,
        layer.coverageHeight,
        layer.coverageChannels
    );
}

bool barycentric(Vec2 p, Vec2 a, Vec2 b, Vec2 c, float& w0, float& w1, float& w2) {
    w0 = w1 = w2 = std::numeric_limits<float>::quiet_NaN();
    auto denom = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    if (denom == 0.0f) return false;
    w0 = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / denom;
    w1 = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / denom;
    w2 = 1.0f - w0 - w1;
    constexpr float Epsilon = -0.0001f;
    return w0 >= Epsilon && w1 >= Epsilon && w2 >= Epsilon;
}

float sampleCoverageSourceAlpha(CoverageSource& source, Vec2 local) {
    if (source.vertices.size() == source.uvs.size() && source.indices.size() >= 3) {
        for (size_t tri = 0; tri + 2 < source.indices.size(); tri += 3) {
            auto i0 = (size_t)compat::ngArrayAt(source.indices, tri + 0);
            auto i1 = (size_t)compat::ngArrayAt(source.indices, tri + 1);
            auto i2 = (size_t)compat::ngArrayAt(source.indices, tri + 2);
            if (i0 >= source.vertices.size() || i1 >= source.vertices.size() || i2 >= source.vertices.size()) continue;

            auto a = source.vertices.at(i0) - source.origin;
            auto b = source.vertices.at(i1) - source.origin;
            auto c = source.vertices.at(i2) - source.origin;
            float w0 = std::numeric_limits<float>::quiet_NaN();
            float w1 = std::numeric_limits<float>::quiet_NaN();
            float w2 = std::numeric_limits<float>::quiet_NaN();
            if (!barycentric(local, a, b, c, w0, w1, w2)) continue;

            auto uv = source.uvs.at(i0) * w0 + source.uvs.at(i1) * w1 +
                source.uvs.at(i2) * w2;
            if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) return 0.0f;
            return sampleCoverageSourceAlphaAtUv(source, uv);
        }
        return 0.0f;
    }

    auto uv = Vec2(
        local.x / (float)source.width + 0.5f,
        local.y / (float)source.height + 0.5f
    );
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) return 0.0f;
    return sampleCoverageSourceAlphaAtUv(source, uv);
}

float sampleCoverageSourceAlphaAtUv(CoverageSource& source, Vec2 uv) {
    return ngDepthImageCoverageAlphaAtUv(uv.x, uv.y, source.data, source.width, source.height, source.channels);
}

void sampleCoverageSourceRgbaAtUv(CoverageSource& source, Vec2 uv, std::array<std::uint8_t, 4>& rgba) {
    rgba.fill(0);
    if (source.data.size() == 0 || source.width <= 0 || source.height <= 0 || source.channels <= 0) return;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) return;

    auto px = std::min(source.width - 1,
        std::max(0, compat::ngFloatToInt32(std::floor(uv.x * (float)source.width))));
    auto py = std::min(source.height - 1,
        std::max(0, compat::ngFloatToInt32(std::floor(uv.y * (float)source.height))));
    auto index = ((size_t)py * (size_t)source.width + (size_t)px) * (size_t)source.channels;
    if (index >= source.data.size()) return;

    rgba[0] = source.data[index];
    rgba[1] = source.channels > 1 && index + 1 < source.data.size() ? source.data[index + 1] : rgba[0];
    rgba[2] = source.channels > 2 && index + 2 < source.data.size() ? source.data[index + 2] : rgba[0];
    rgba[3] = source.channels > 3 && index + 3 < source.data.size() ? source.data[index + 3] : 255;
}

bool sampleCoverageSourceRgba(CoverageSource& source, Vec2 local, std::array<std::uint8_t, 4>& rgba) {
    if (source.vertices.size() == source.uvs.size() && source.indices.size() >= 3) {
        for (size_t tri = 0; tri + 2 < source.indices.size(); tri += 3) {
            auto i0 = (size_t)compat::ngArrayAt(source.indices, tri + 0);
            auto i1 = (size_t)compat::ngArrayAt(source.indices, tri + 1);
            auto i2 = (size_t)compat::ngArrayAt(source.indices, tri + 2);
            if (i0 >= source.vertices.size() || i1 >= source.vertices.size() || i2 >= source.vertices.size()) continue;

            auto a = source.vertices.at(i0) - source.origin;
            auto b = source.vertices.at(i1) - source.origin;
            auto c = source.vertices.at(i2) - source.origin;
            float w0 = std::numeric_limits<float>::quiet_NaN();
            float w1 = std::numeric_limits<float>::quiet_NaN();
            float w2 = std::numeric_limits<float>::quiet_NaN();
            if (!barycentric(local, a, b, c, w0, w1, w2)) continue;

            auto uv = source.uvs.at(i0) * w0 + source.uvs.at(i1) * w1 +
                source.uvs.at(i2) * w2;
            sampleCoverageSourceRgbaAtUv(source, uv, rgba);
            return rgba[3] > 0;
        }
        return false;
    }

    auto uv = Vec2(
        local.x / (float)source.width + 0.5f,
        local.y / (float)source.height + 0.5f
    );
    sampleCoverageSourceRgbaAtUv(source, uv, rgba);
    return rgba[3] > 0;
}

ColorLayerPreviewRgba buildColorLayerPreviewRgba(DepthLayerImage& image) {
    using depthsample::detail::add32;
    using depthsample::detail::sub32;
    ColorLayerPreviewRgba result;
    result.width = image.width;
    result.height = image.height;
    auto expectedLength = (size_t)std::max(0, depthsample::detail::mul32(image.width, image.height)) * 4;
    if (expectedLength == 0) return result;

    if (image.coverageSources.size() > 0) {
        result.rgba.resize(expectedLength);
        bool hasCoverage = false;

        auto compositePixel = [&](int x, int y, std::array<std::uint8_t, 4>& rgba, float opacity) -> void {
            auto srcA = clamp01(((float)rgba[3] / 255.0f) * opacity);
            if (srcA <= 0.0f) return;
            auto index = ((size_t)y * (size_t)image.width + (size_t)x) * 4;
            auto dstA = (float)result.rgba[index + 3] / 255.0f;
            auto outA = srcA + dstA * (1.0f - srcA);
            if (outA <= 0.0f) return;
            auto remaining = 1.0f - srcA;
            auto dstR = ((float)result.rgba[index + 0] / 255.0f) * dstA;
            auto dstG = ((float)result.rgba[index + 1] / 255.0f) * dstA;
            auto dstB = ((float)result.rgba[index + 2] / 255.0f) * dstA;
            auto outR = ((float)rgba[0] / 255.0f) * srcA + dstR * remaining;
            auto outG = ((float)rgba[1] / 255.0f) * srcA + dstG * remaining;
            auto outB = ((float)rgba[2] / 255.0f) * srcA + dstB * remaining;
            result.rgba[index + 0] = (std::uint8_t)std::round(clamp01(outR / outA) * 255.0f);
            result.rgba[index + 1] = (std::uint8_t)std::round(clamp01(outG / outA) * 255.0f);
            result.rgba[index + 2] = (std::uint8_t)std::round(clamp01(outB / outA) * 255.0f);
            result.rgba[index + 3] = (std::uint8_t)std::round(clamp01(outA) * 255.0f);
            hasCoverage = true;
        };

        for (auto sources = image.coverageSources; auto& source : sources) {
            if (source.data.size() == 0 || source.width <= 0 || source.height <= 0 || source.channels != 4) continue;
            auto useMesh = source.vertices.size() == source.uvs.size() && source.indices.size() >= 3;
            if (!useMesh) {
                auto localToWorld = core::math::source_matrix::inverse(source.worldToLocal);
                auto halfWidth = (float)source.width / 2.0f;
                auto halfHeight = (float)source.height / 2.0f;
                const compat::Slice<Vec2> corners{
                    transformXY(localToWorld, Vec2(-halfWidth, -halfHeight)),
                    transformXY(localToWorld, Vec2( halfWidth, -halfHeight)),
                    transformXY(localToWorld, Vec2(-halfWidth,  halfHeight)),
                    transformXY(localToWorld, Vec2( halfWidth,  halfHeight)),
                };
                auto minX = sub32(image.width, 1);
                auto minY = sub32(image.height, 1);
                auto maxX = 0;
                auto maxY = 0;
                for (auto corner : corners) {
                    auto pixel = worldToLayerPixel(image, corner);
                    minX = std::min(minX, std::max(0, compat::ngFloatToInt32(std::floor(pixel.x))));
                    minY = std::min(minY, std::max(0, compat::ngFloatToInt32(std::floor(pixel.y))));
                    maxX = std::max(maxX, std::min(sub32(image.width, 1),
                        compat::ngFloatToInt32(std::ceil(pixel.x))));
                    maxY = std::max(maxY, std::min(sub32(image.height, 1),
                        compat::ngFloatToInt32(std::ceil(pixel.y))));
                }
                if (minX > maxX || minY > maxY) continue;
                const auto yEnd = add32(maxY, 1);
                for (auto y = minY; y < yEnd; ++y) {
                    const auto xEnd = add32(maxX, 1);
                    for (auto x = minX; x < xEnd; ++x) {
                        auto world = Vec2(
                            (float)add32(image.left, x) - (float)image.documentWidth / 2.0f,
                            (float)add32(image.top, y) - (float)image.documentHeight / 2.0f
                        );
                        auto local = transformXY(source.worldToLocal, world);
                        std::array<std::uint8_t, 4> rgba{};
                        if (!sampleCoverageSourceRgba(source, local, rgba)) continue;
                        compositePixel(x, y, rgba, source.opacity);
                    }
                }
                continue;
            }

            auto localToWorld = core::math::source_matrix::inverse(source.worldToLocal);
            for (size_t tri = 0; tri + 2 < source.indices.size(); tri += 3) {
                auto i0 = (size_t)source.indices[tri + 0];
                auto i1 = (size_t)source.indices[tri + 1];
                auto i2 = (size_t)source.indices[tri + 2];
                if (i0 >= source.vertices.size() || i1 >= source.vertices.size() || i2 >= source.vertices.size()) continue;

                auto aLocal = source.vertices.at(i0) - source.origin;
                auto bLocal = source.vertices.at(i1) - source.origin;
                auto cLocal = source.vertices.at(i2) - source.origin;
                auto aPix = worldToLayerPixel(image, transformXY(localToWorld, aLocal));
                auto bPix = worldToLayerPixel(image, transformXY(localToWorld, bLocal));
                auto cPix = worldToLayerPixel(image, transformXY(localToWorld, cLocal));

                auto minX = std::max(0, compat::ngFloatToInt32(
                    std::floor(std::min(aPix.x, std::min(bPix.x, cPix.x)))));
                auto maxX = std::min(sub32(image.width, 1), compat::ngFloatToInt32(
                    std::ceil(std::max(aPix.x, std::max(bPix.x, cPix.x)))));
                auto minY = std::max(0, compat::ngFloatToInt32(
                    std::floor(std::min(aPix.y, std::min(bPix.y, cPix.y)))));
                auto maxY = std::min(sub32(image.height, 1), compat::ngFloatToInt32(
                    std::ceil(std::max(aPix.y, std::max(bPix.y, cPix.y)))));
                if (minX > maxX || minY > maxY) continue;

                const auto yEnd = add32(maxY, 1);
                for (auto y = minY; y < yEnd; ++y) {
                    const auto xEnd = add32(maxX, 1);
                    for (auto x = minX; x < xEnd; ++x) {
                        float w0 = std::numeric_limits<float>::quiet_NaN();
                        float w1 = std::numeric_limits<float>::quiet_NaN();
                        float w2 = std::numeric_limits<float>::quiet_NaN();
                        if (!barycentric(Vec2((float)x + 0.5f, (float)y + 0.5f),
                            aPix, bPix, cPix, w0, w1, w2)) continue;
                        auto uv = source.uvs.at(i0) * w0 + source.uvs.at(i1) * w1 + source.uvs.at(i2) * w2;
                        std::array<std::uint8_t, 4> rgba{};
                        sampleCoverageSourceRgbaAtUv(source, uv, rgba);
                        compositePixel(x, y, rgba, source.opacity);
                    }
                }
            }
        }
        if (hasCoverage) return result;

        for (auto sources = image.coverageSources; auto& source : sources) {
            if (source.data.size() == 0 || source.width <= 0 || source.height <= 0 || source.channels != 4) continue;
            auto sourceLength = (size_t)source.width * (size_t)source.height * 4;
            if (source.data.size() < sourceLength) continue;
            result.rgba = source.data.slice(0, sourceLength).duplicate();
            result.width = source.width;
            result.height = source.height;
            return result;
        }
    }

    if (image.coverageData.size() >= expectedLength && image.coverageWidth == image.width &&
        image.coverageHeight == image.height && image.coverageChannels == 4) {
        result.rgba = image.coverageData.slice(0, expectedLength).duplicate();
        return result;
    }
    if (image.coverageData.size() > 0 && image.coverageWidth == image.documentWidth &&
        image.coverageHeight == image.documentHeight && image.coverageChannels == 4) {
        auto cropped = cropRgbaRect(image.coverageData, image.coverageWidth, image.coverageHeight,
            image.left, image.top, image.width, image.height);
        if (cropped.size() == expectedLength) {
            result.rgba = cropped;
            return result;
        }
    }

    result.rgba = image.data.duplicate();
    return result;
}

void cropDepthLayerImageToRect(DepthLayerImage& image, int left, int top, int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (left == image.left && top == image.top && width == image.width && height == image.height) return;
    auto cropped = cropRgbaRect(image.data, image.width, image.height,
        depthsample::detail::sub32(left, image.left), depthsample::detail::sub32(top, image.top), width, height);
    if (cropped.size() == 0) return;
    image.left = left;
    image.top = top;
    image.width = width;
    image.height = height;
    image.data = cropped;
    image.coverageGateCache = {};
    image.coverageAlphaCache = {};
}

bool tightenDepthLayerImageToCoverageBounds(DepthLayerImage& image) {
    if (image.coverageSources.size() == 0 || image.width <= 0 || image.height <= 0) return false;
    bool hasBounds = false;
    float minX = std::numeric_limits<float>::quiet_NaN();
    float minY = std::numeric_limits<float>::quiet_NaN();
    float maxX = std::numeric_limits<float>::quiet_NaN();
    float maxY = std::numeric_limits<float>::quiet_NaN();

    auto includeWorldPoint = [&](Vec2 world) -> void {
        auto documentX = world.x + (float)image.documentWidth / 2.0f;
        auto documentY = world.y + (float)image.documentHeight / 2.0f;
        if (!hasBounds) {
            minX = maxX = documentX;
            minY = maxY = documentY;
            hasBounds = true;
        } else {
            minX = std::min(minX, documentX);
            minY = std::min(minY, documentY);
            maxX = std::max(maxX, documentX);
            maxY = std::max(maxY, documentY);
        }
    };

    for (auto sources = image.coverageSources; auto& source : sources) {
        if (source.data.size() == 0 || source.width <= 0 || source.height <= 0) continue;
        auto localToWorld = source.localToDocumentWorld;
        if (source.vertices.size() > 0) {
            for (auto vertices = source.vertices; auto vertex : vertices) {
                includeWorldPoint(transformXY(localToWorld, vertex - source.origin));
            }
        } else {
            auto halfWidth = (float)source.width / 2.0f;
            auto halfHeight = (float)source.height / 2.0f;
            includeWorldPoint(transformXY(localToWorld, Vec2(-halfWidth, -halfHeight)));
            includeWorldPoint(transformXY(localToWorld, Vec2( halfWidth, -halfHeight)));
            includeWorldPoint(transformXY(localToWorld, Vec2(-halfWidth,  halfHeight)));
            includeWorldPoint(transformXY(localToWorld, Vec2( halfWidth,  halfHeight)));
        }
    }
    if (!hasBounds) return false;

    using depthsample::detail::add32;
    using depthsample::detail::sub32;
    constexpr int Padding = 2;
    auto cropLeft = std::max(image.left, sub32(compat::ngFloatToInt32(std::floor(minX)), Padding));
    auto cropTop = std::max(image.top, sub32(compat::ngFloatToInt32(std::floor(minY)), Padding));
    auto cropRight = std::min(add32(image.left, image.width),
        add32(add32(compat::ngFloatToInt32(std::ceil(maxX)), Padding), 1));
    auto cropBottom = std::min(add32(image.top, image.height),
        add32(add32(compat::ngFloatToInt32(std::ceil(maxY)), Padding), 1));
    if (cropRight <= cropLeft || cropBottom <= cropTop) return false;
    cropDepthLayerImageToRect(image, cropLeft, cropTop, sub32(cropRight, cropLeft), sub32(cropBottom, cropTop));
    return true;
}

bool setDepthLayerImageToCoverageDocumentBounds(DepthLayerImage& image) {
    if (image.coverageSources.size() == 0 || image.width <= 0 || image.height <= 0) return false;
    bool hasBounds = false;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    auto includeRect = [&](int rectLeft, int rectTop, int rectWidth, int rectHeight) -> void {
        if (rectWidth <= 0 || rectHeight <= 0) return;
        auto rectRight = depthsample::detail::add32(rectLeft, rectWidth);
        auto rectBottom = depthsample::detail::add32(rectTop, rectHeight);
        if (!hasBounds) {
            left = rectLeft;
            top = rectTop;
            right = rectRight;
            bottom = rectBottom;
            hasBounds = true;
        } else {
            left = std::min(left, rectLeft);
            top = std::min(top, rectTop);
            right = std::max(right, rectRight);
            bottom = std::max(bottom, rectBottom);
        }
    };

    auto includeMeshBounds = [&](CoverageSource& source) -> void {
        if (source.vertices.size() == 0) return;
        bool hasMeshBounds = false;
        float minX = std::numeric_limits<float>::quiet_NaN();
        float minY = std::numeric_limits<float>::quiet_NaN();
        float maxX = std::numeric_limits<float>::quiet_NaN();
        float maxY = std::numeric_limits<float>::quiet_NaN();
        for (auto vertex : source.vertices) {
            auto world = transformXY(source.localToDocumentWorld, vertex - source.origin);
            auto documentX = world.x + (float)image.documentWidth / 2.0f;
            auto documentY = world.y + (float)image.documentHeight / 2.0f;
            if (!hasMeshBounds) {
                minX = maxX = documentX;
                minY = maxY = documentY;
                hasMeshBounds = true;
            } else {
                minX = std::min(minX, documentX);
                minY = std::min(minY, documentY);
                maxX = std::max(maxX, documentX);
                maxY = std::max(maxY, documentY);
            }
        }
        if (!hasMeshBounds) return;
        includeRect(
            compat::ngFloatToInt32(std::floor(minX)),
            compat::ngFloatToInt32(std::floor(minY)),
            std::max(1, depthsample::detail::add32(
                depthsample::detail::sub32(compat::ngFloatToInt32(std::ceil(maxX)),
                    compat::ngFloatToInt32(std::floor(minX))), 1)),
            std::max(1, depthsample::detail::add32(
                depthsample::detail::sub32(compat::ngFloatToInt32(std::ceil(maxY)),
                    compat::ngFloatToInt32(std::floor(minY))), 1))
        );
    };

    for (auto& source : image.coverageSources) {
        if (source.vertices.size() > 0) {
            includeMeshBounds(source);
        } else if (source.hasDocumentRect) {
            includeRect(source.documentLeft, source.documentTop, source.documentWidth, source.documentHeight);
        }
    }
    if (!hasBounds) return tightenDepthLayerImageToCoverageBounds(image);

    left = std::max(image.left, left);
    top = std::max(image.top, top);
    right = std::min(depthsample::detail::add32(image.left, image.width), right);
    bottom = std::min(depthsample::detail::add32(image.top, image.height), bottom);
    if (right <= left || bottom <= top) return false;
    cropDepthLayerImageToRect(image, left, top,
        depthsample::detail::sub32(right, left), depthsample::detail::sub32(bottom, top));
    return true;
}

Vec2 worldToLayerPixel(DepthLayerImage& layer, Vec2 world) {
    return Vec2(
        world.x + (float)layer.documentWidth / 2.0f - (float)layer.left,
        world.y + (float)layer.documentHeight / 2.0f - (float)layer.top
    );
}

void buildCoverageCache(DepthLayerImage& layer) {
    if (layer.coverageSources.size() == 0 || layer.width <= 0 || layer.height <= 0) return;
    auto pixelCount = (size_t)layer.width * (size_t)layer.height;
    layer.coverageGateCache.resize(pixelCount);
    layer.coverageAlphaCache.resize(pixelCount);
    std::fill(layer.coverageGateCache.begin(), layer.coverageGateCache.end(), 0.0f);
    std::fill(layer.coverageAlphaCache.begin(), layer.coverageAlphaCache.end(), 0.0f);

    for (auto sources = layer.coverageSources; auto& source : sources) {
        if (source.data.size() == 0 || source.width <= 0 || source.height <= 0) continue;
        auto useMesh = source.vertices.size() == source.uvs.size() && source.indices.size() >= 3;
        if (!useMesh) {
            for (int y = 0; y < layer.height; y++) {
                for (int x = 0; x < layer.width; x++) {
                    auto world = Vec2(
                        (float)depthsample::detail::add32(layer.left, x) - (float)layer.documentWidth / 2.0f,
                        (float)depthsample::detail::add32(layer.top, y) - (float)layer.documentHeight / 2.0f
                    );
                    auto local = transformXY(source.worldToLocal, world);
                    auto alpha = sampleCoverageSourceAlpha(source, local);
                    if (alpha <= 0.0f) continue;
                    auto index = (size_t)y * (size_t)layer.width + (size_t)x;
                    layer.coverageGateCache[index] = ngDepthImageCompositeAlpha(layer.coverageGateCache[index], alpha);
                    layer.coverageAlphaCache[index] = ngDepthImageCompositeAlpha(
                        layer.coverageAlphaCache[index],
                        alpha * source.opacity
                    );
                }
            }
            continue;
        }

        auto localToWorld = source.localToDocumentWorld;
        for (size_t tri = 0; tri + 2 < source.indices.size(); tri += 3) {
            auto i0 = (size_t)source.indices[tri + 0];
            auto i1 = (size_t)source.indices[tri + 1];
            auto i2 = (size_t)source.indices[tri + 2];
            if (i0 >= source.vertices.size() || i1 >= source.vertices.size() || i2 >= source.vertices.size()) continue;

            auto aLocal = source.vertices.at(i0) - source.origin;
            auto bLocal = source.vertices.at(i1) - source.origin;
            auto cLocal = source.vertices.at(i2) - source.origin;
            auto aPix = worldToLayerPixel(layer, transformXY(localToWorld, aLocal));
            auto bPix = worldToLayerPixel(layer, transformXY(localToWorld, bLocal));
            auto cPix = worldToLayerPixel(layer, transformXY(localToWorld, cLocal));

            auto minX = std::max(0,
                compat::ngFloatToInt32(std::floor(std::min(aPix.x, std::min(bPix.x, cPix.x)))));
            auto maxX = std::min(layer.width - 1,
                compat::ngFloatToInt32(std::ceil(std::max(aPix.x, std::max(bPix.x, cPix.x)))));
            auto minY = std::max(0,
                compat::ngFloatToInt32(std::floor(std::min(aPix.y, std::min(bPix.y, cPix.y)))));
            auto maxY = std::min(layer.height - 1,
                compat::ngFloatToInt32(std::ceil(std::max(aPix.y, std::max(bPix.y, cPix.y)))));
            if (minX > maxX || minY > maxY) continue;

            for (auto y = minY; y < maxY + 1; ++y) {
                for (auto x = minX; x < maxX + 1; ++x) {
                    float w0 = std::numeric_limits<float>::quiet_NaN();
                    float w1 = std::numeric_limits<float>::quiet_NaN();
                    float w2 = std::numeric_limits<float>::quiet_NaN();
                    if (!barycentric(Vec2((float)x + 0.5f, (float)y + 0.5f),
                        aPix, bPix, cPix, w0, w1, w2)) continue;
                    auto uv = source.uvs.at(i0) * w0 + source.uvs.at(i1) * w1 + source.uvs.at(i2) * w2;
                    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) continue;
                    auto alpha = sampleCoverageSourceAlphaAtUv(source, uv);
                    if (alpha <= 0.0f) continue;
                    auto index = (size_t)y * (size_t)layer.width + (size_t)x;
                    layer.coverageGateCache[index] = ngDepthImageCompositeAlpha(layer.coverageGateCache[index], alpha);
                    layer.coverageAlphaCache[index] = ngDepthImageCompositeAlpha(
                        layer.coverageAlphaCache[index],
                        alpha * source.opacity
                    );
                }
            }
        }
    }
}

void maskDepthToCoverage(DepthLayerImage& layer, PsdDepthImportSettings& settings) {
    if (layer.width <= 0 || layer.height <= 0 || layer.data.size() == 0) return;
    for (int y = 0; y < layer.height; y++) {
        for (int x = 0; x < layer.width; x++) {
            auto index = ((size_t)y * (size_t)layer.width + (size_t)x) * 4;
            if (index + 3 >= layer.data.size()) continue;
            if (ngDepthSampleAcceptsAlpha(coverageAlpha(layer, x, y), settings.alphaThreshold)) continue;
            compat::ngArrayAt(layer.data, index + 0) = 0;
            compat::ngArrayAt(layer.data, index + 1) = 0;
            compat::ngArrayAt(layer.data, index + 2) = 0;
            compat::ngArrayAt(layer.data, index + 3) = 0;
        }
    }
}

void applyDepthDrawLayerDepthCleanup(DepthLayerImage& layer, PsdDepthImportSettings& settings) {
    if (!settings.repairContourBand) return;
    if (layer.width <= 0 || layer.height <= 0 || layer.data.size() != (size_t)(depthsample::detail::mul32(depthsample::detail::mul32(layer.width, layer.height), 4))) {
        return;
    }
    auto depth = ngDepthDrawDecodeDepthPixelsFromRgbaSource(layer.data, sampleChannel(settings.channel));
    compat::Slice<std::uint8_t> mask;
    if (layer.coverageAlphaCache.size() == depth.size()) {
        mask.resize(depth.size());
        std::size_t maskIndex = 0;
        for (auto values = layer.coverageAlphaCache; auto value : values) {
            compat::ngArrayAt(mask, maskIndex) = value > 0.0f ? 1 : 0;
            ++maskIndex;
        }
    } else if (layer.coverageData.size() == layer.data.size()) {
        mask = ngDepthDrawAlphaMaskFromRgbaSource(layer.coverageData);
    } else {
        mask = ngDepthDrawAlphaMaskFromRgbaSource(layer.data);
    }
    if (mask.size() != depth.size()) return;

    auto contourBand = ngDepthDrawBuildLayerContourBandMaskSource(mask, layer.width, layer.height, 2);
    auto contourSeed = depth.duplicate();
    size_t remainingSeedPixels = 0;
    std::size_t contourIndex = 0;
    for (auto band = contourBand; auto value : band) {
        auto i = contourIndex++;
        if (value) compat::ngArrayAt(contourSeed, i) = 0;
        else if (compat::ngArrayAt(mask, i) && compat::ngArrayAt(contourSeed, i) > 0) remainingSeedPixels++;
    }
    if (remainingSeedPixels == 0) return;
    auto repaired = ngDepthDrawInpaintMaskedLayerDepthSource(contourSeed, mask, layer.width, layer.height);
    std::size_t repairedIndex = 0;
    for (auto pixels = repaired.pixels; auto value : pixels) {
        auto i = repairedIndex++;
        if (!compat::ngArrayAt(repaired.filledMask, i)) continue;
        auto offset = i * 4;
        switch (settings.channel) {
            case PsdDepthChannel::R:
                compat::ngArrayAt(layer.data, offset) = value;
                break;
            case PsdDepthChannel::G:
                compat::ngArrayAt(layer.data, offset + 1) = value;
                break;
            case PsdDepthChannel::B:
                compat::ngArrayAt(layer.data, offset + 2) = value;
                break;
            case PsdDepthChannel::AverageRGB:
            case PsdDepthChannel::Luminance:
                compat::ngArrayAt(layer.data, offset) = value;
                compat::ngArrayAt(layer.data, offset + 1) = value;
                compat::ngArrayAt(layer.data, offset + 2) = value;
                break;
            default:
                throw compat::ngStaticError<compat::SwitchError>(
                    compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 1783);
        }
        compat::ngArrayAt(layer.data, offset + 3) = compat::ngArrayAt(mask, i) && value > 0 ? 255 : 0;
    }
}

DepthDrawSplitLayer depthDrawSplitLayerFromCompositeSource(PsdDepthCompositeSourceLayer sourceLayer) {
    DepthDrawSplitLayer splitLayer;
    splitLayer.left = sourceLayer.left;
    splitLayer.top = sourceLayer.top;
    splitLayer.width = sourceLayer.width;
    splitLayer.height = sourceLayer.height;
    splitLayer.maskPixels = ngDepthDrawAlphaMaskFromRgbaSource(sourceLayer.rgba);
    splitLayer.alphaMask = splitLayer.maskPixels.duplicate();
    return splitLayer;
}

compat::Slice<std::uint8_t> depthDrawMaskFromDepthLayerImage(DepthLayerImage& image) {
    auto sourceAt = [](auto& values, std::size_t index, std::size_t sourceLine) -> decltype(auto) {
        if (index >= values.size())
            throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"),
                sourceLine);
        return values[index];
    };
    auto pixelCount = (size_t)std::max(0, depthsample::detail::mul32(image.width, image.height));
    compat::Slice<std::uint8_t> mask;
    mask.resize(pixelCount);
    if (pixelCount == 0) return mask;

    if (image.coverageAlphaCache.size() == pixelCount) {
        auto values = image.coverageAlphaCache;
        for (std::size_t i = 0; i < values.size(); ++i) {
            auto value = sourceAt(values, i, 1822);
            sourceAt(mask, i, 1822) = value > 0.0f ? 1 : 0;
        }
        return mask;
    }
    if (image.coverageData.size() >= pixelCount * 4 &&
        image.coverageWidth == image.width &&
        image.coverageHeight == image.height &&
        image.coverageChannels == 4) {
        return ngDepthDrawAlphaMaskFromRgbaSource(image.coverageData.slice(0, pixelCount * 4));
    }

    bool hasDocumentRectSource = false;
    for (auto sources = image.coverageSources; auto& source : sources) {
        if (!source.hasDocumentRect ||
            source.data.size() == 0 ||
            source.width <= 0 ||
            source.height <= 0 ||
            source.channels <= 0) {
            continue;
        }
        hasDocumentRectSource = true;
        for (int y = 0; y < image.height; y++) {
            auto sourceY = depthsample::detail::sub32(depthsample::detail::add32(image.top, y), source.documentTop);
            if (sourceY < 0 || sourceY >= source.height) continue;
            for (int x = 0; x < image.width; x++) {
                auto sourceX = depthsample::detail::sub32(
                    depthsample::detail::add32(image.left, x), source.documentLeft);
                if (sourceX < 0 || sourceX >= source.width) continue;
                auto sourceIndex = ((size_t)sourceY * (size_t)source.width + (size_t)sourceX) *
                    (size_t)source.channels;
                if (sourceIndex >= source.data.size()) continue;
                auto alpha = source.channels > 3 ? sourceAt(source.data, sourceIndex + 3, 1851) : 255;
                if (alpha == 0) continue;
                sourceAt(mask, (size_t)y * (size_t)image.width + (size_t)x, 1853) = 1;
            }
        }
    }
    if (hasDocumentRectSource) return mask;

    if (image.data.size() >= pixelCount * 4) {
        return ngDepthDrawAlphaMaskFromRgbaSource(image.data.slice(0, pixelCount * 4));
    }
    return mask;
}

DepthDrawSplitLayer depthDrawSplitLayerFromDepthLayerImage(DepthLayerImage& image) {
    DepthDrawSplitLayer splitLayer;
    splitLayer.left = image.left;
    splitLayer.top = image.top;
    splitLayer.width = image.width;
    splitLayer.height = image.height;
    splitLayer.maskPixels = depthDrawMaskFromDepthLayerImage(image);
    splitLayer.alphaMask = splitLayer.maskPixels.duplicate();
    return splitLayer;
}

void setLayerDepthFromDepthDrawPixels(DepthLayerImage& image, std::span<const std::uint8_t> depthPixels, std::span<const std::uint8_t> alphaMask) {
    if (depthPixels.size() != (size_t)(depthsample::detail::mul32(image.width, image.height))) return;
    if (alphaMask.size() && alphaMask.size() != depthPixels.size()) return;
    // Input spans can alias this backing even when resizing detaches the image.
    [[maybe_unused]] const auto retainedInputBacking = image.data;
    image.data.resize((size_t)image.width * (size_t)image.height * 4);
    for (std::size_t i = 0; i < depthPixels.size(); ++i) {
        auto value = compat::ngArrayAt(depthPixels, i);
        auto offset = i * 4;
        compat::ngArrayAt(image.data, offset + 0) = value;
        compat::ngArrayAt(image.data, offset + 1) = value;
        compat::ngArrayAt(image.data, offset + 2) = value;
        compat::ngArrayAt(image.data, offset + 3) =
            value > 0 && (!alphaMask.size() || compat::ngArrayAt(alphaMask, i)) ? 255 : 0;

    }
}

bool hasVisibleDepthDrawPixels(std::span<const std::uint8_t> depthPixels, std::span<const std::uint8_t> alphaMask) {
    for (std::size_t i = 0; i < depthPixels.size(); ++i) {
        auto value = compat::ngArrayAt(depthPixels, i);
        if (value == 0) continue;
        if (alphaMask.size() && (i >= alphaMask.size() || compat::ngArrayAt(alphaMask, i) == 0)) continue;
        return true;

    }
    return false;
}

void seedLayerDepthFromDepthDrawSplit(
    DepthLayerImage& image,
    DepthDrawSplitLayer splitLayer,
    int layerIndex,
    int documentWidth,
    int documentHeight,
    compat::ConstSlice<std::uint8_t> stableDepthPixels,
    compat::ConstSlice<int> visibleLayerMap,
    compat::ConstSlice<DepthDrawSplitLayer> splitLayers,
    bool repairContourBand
) {
    auto sourceAt = [](auto& values, std::size_t index, std::size_t sourceLine) -> decltype(auto) {
        if (index >= values.size())
            throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"),
                sourceLine);
        return values[index];
    };
    if (!repairContourBand) {
        compat::Slice<std::uint8_t> sampledDepth;
        sampledDepth.resize((size_t)splitLayer.width * (size_t)splitLayer.height);
        for (auto y = 0; y < splitLayer.height; ++y) {
            for (auto x = 0; x < splitLayer.width; ++x) {
                auto localIndex = (size_t)y * (size_t)splitLayer.width + (size_t)x;
                if (!sourceAt(splitLayer.maskPixels, localIndex, 1915)) continue;
                auto globalX = depthsample::detail::add32(splitLayer.left, x);
                auto globalY = depthsample::detail::add32(splitLayer.top, y);
                if (globalX < 0 || globalX >= documentWidth || globalY < 0 || globalY >= documentHeight) continue;
                auto& destination = sourceAt(sampledDepth, localIndex, 1919);
                const auto sampled = sourceAt(stableDepthPixels,
                    (size_t)globalY * (size_t)documentWidth + (size_t)globalX, 1919);
                destination = sampled;
            }
        }
        setLayerDepthFromDepthDrawPixels(image, sampledDepth, splitLayer.maskPixels);
        return;
    }

    compat::Slice<std::uint8_t> contourBand;
    contourBand = ngDepthDrawBuildLayerContourBandMaskSource(
        splitLayer.maskPixels, splitLayer.width, splitLayer.height, 2);
    bool hasInteriorSeed = false;
    {
        const auto masks = splitLayer.maskPixels;
        for (std::size_t i = 0; i < masks.size(); ++i) {
            const auto maskPixel = sourceAt(masks, i, 1931);
            if (maskPixel && !sourceAt(contourBand, i, 1932)) {
                hasInteriorSeed = true;
                break;
            }
        }
    }
    if (!hasInteriorSeed) contourBand = {};
    auto seeded = ngDepthDrawSeedLayerDepthPixelsSource(
        splitLayer,
        layerIndex,
        documentWidth,
        documentHeight,
        stableDepthPixels,
        visibleLayerMap,
        splitLayer.maskPixels,
        splitLayers,
        contourBand,
        2
    );
    DepthDrawPruneLayer pruneLayer;
    pruneLayer.left = splitLayer.left;
    pruneLayer.top = splitLayer.top;
    pruneLayer.width = splitLayer.width;
    pruneLayer.height = splitLayer.height;
    auto pruned = ngDepthDrawPruneForeignDepthSeeds(
        seeded,
        pruneLayer,
        layerIndex,
        documentWidth,
        documentHeight,
        stableDepthPixels,
        visibleLayerMap,
        splitLayer.maskPixels,
        24.0
    );
    if (hasVisibleDepthDrawPixels(pruned.pixels, splitLayer.maskPixels)) {
        setLayerDepthFromDepthDrawPixels(image, pruned.pixels, splitLayer.maskPixels);
    } else if (hasVisibleDepthDrawPixels(seeded, splitLayer.maskPixels)) {
        setLayerDepthFromDepthDrawPixels(image, seeded, splitLayer.maskPixels);
    } else {
        // An empty split is authoritative. Retaining the pre-split flat PNG
        // depth here makes fully occluded layers participate in sampling.
        setLayerDepthFromDepthDrawPixels(image, seeded, splitLayer.maskPixels);
    }
}

float sampleCoverageAlphaAtUv(DepthLayerImage& layer, Vec2 uv) {
    return ngDepthImageCoverageAlphaAtUv(
        uv.x,
        uv.y,
        layer.coverageData,
        layer.coverageWidth,
        layer.coverageHeight,
        layer.coverageChannels
    );
}

float effectiveAlpha(DepthLayerImage& layer, size_t index, int x, int y) {
    auto alpha = effectiveAlpha(layer.data, index, layer.opacity);
    return alpha * coverageAlpha(layer, x, y);
}

float surfaceAlpha(DepthLayerImage& layer, size_t index, int x, int y) {
    if (layer.coverageSources.size() > 0 || layer.coverageData.size() > 0) {
        return coverageAlpha(layer, x, y);
    }
    return effectiveAlpha(layer.data, index, layer.opacity);
}

std::uint8_t effectiveAlphaByte(DepthLayerImage& layer, size_t index, int x, int y) {
    auto alpha = surfaceAlpha(layer, index, x, y);
    return ngDepthSampleAlphaByte(alpha);
}

bool acceptsDepthPixel(DepthLayerImage& layer, size_t index, int x, int y, PsdDepthImportSettings& settings) {
    if (index + 3 >= layer.data.size()) return false;
    if (!coverageReliable(layer, x, y)) return false;
    if (!ngDepthSampleAcceptsAlpha(surfaceAlpha(layer, index, x, y), settings.alphaThreshold)) return false;
    if (settings.zeroDepthIsMissing) {
        auto sampleSettings = settings;
        if (pixelDepth01(layer.data, index, sampleSettings) <= 0.0f) return false;
    }
    return true;
}

ptrdiff_t bestMatchingColorSourceLayer(
    PsdDepthCompositeSourceLayer depthLayer,
    compat::Slice<PsdDepthCompositeSourceLayer> colorLayers,
    const nicxlive::core::common::SharedArray<bool> reserved
) {
    ptrdiff_t bestIndex = -1;
    float bestScore = 0.0f;
    for (std::size_t i = 0; i < colorLayers.size(); ++i) {
        auto colorLayer = colorLayers[i];
        if (i < reserved.size() && reserved[i]) continue;
        auto score = compositeSourceLayerMatchScore(colorLayer, depthLayer);
        if (score > bestScore) {
            bestScore = score;
            bestIndex = (ptrdiff_t)i;
        }

    }
    return bestIndex;
}

void attachCompositeSourceCoverage(
    DepthLayerImage& image,
    PsdDepthCompositeSourceLayer colorLayer
) {
    if (image.width <= 0 || image.height <= 0 || colorLayer.width <= 0 || colorLayer.height <= 0) return;
    auto colorLength = (size_t)colorLayer.width * (size_t)colorLayer.height * 4;
    if (colorLayer.rgba.size() < colorLength) return;

    image.coverageWidth = image.width;
    image.coverageHeight = image.height;
    image.coverageChannels = 4;
    image.coverageOpacity = colorLayer.visible ? colorLayer.opacity : 0.0f;
    image.coverageData.resize((size_t)image.width * (size_t)image.height * 4);
    for (auto y = 0; y < image.height; ++y) {
        for (auto x = 0; x < image.width; ++x) {
            auto sourceX = depthsample::detail::sub32(depthsample::detail::add32(image.left, x), colorLayer.left);
            auto sourceY = depthsample::detail::sub32(depthsample::detail::add32(image.top, y), colorLayer.top);
            if (sourceX < 0 || sourceY < 0 || sourceX >= colorLayer.width || sourceY >= colorLayer.height) continue;
            auto sourceIndex = ((size_t)sourceY * (size_t)colorLayer.width + (size_t)sourceX) * 4;
            auto targetIndex = ((size_t)y * (size_t)image.width + (size_t)x) * 4;
            const auto targetEnd = targetIndex + 4;
            if (targetIndex > targetEnd || targetEnd > image.coverageData.size())
                throw compat::ngStaticError<compat::ArraySliceError>(targetIndex, targetEnd,
                    image.coverageData.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 2053);
            auto target = image.coverageData.slice(targetIndex, targetEnd);
            const auto sourceEnd = sourceIndex + 4;
            if (sourceIndex > sourceEnd || sourceEnd > colorLayer.rgba.size())
                throw compat::ngStaticError<compat::ArraySliceError>(sourceIndex, sourceEnd,
                    colorLayer.rgba.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 2053);
            auto source = colorLayer.rgba.slice(sourceIndex, sourceEnd);
            copyPsdPixel(target.data(), source.data());
        }
    }
}

PsdClippingBaseState psdClippingBaseState(DepthLayerImage& image, bool visible, float opacity) {
    PsdClippingBaseState state;
    state.left = image.left;
    state.top = image.top;
    state.width = image.width;
    state.height = image.height;
    state.visible = visible;
    state.opacity = opacity;
    state.alpha.resize((size_t)std::max(0, depthsample::detail::mul32(image.width, image.height)));
    std::size_t i = 0;
    for (auto alpha = state.alpha; auto& value : alpha) {
        const auto index = i++ * 4 + 3;
        value = compat::ngArrayAt(image.data, index);
    }
    return state;
}

void applyPsdClippingBaseAlpha(DepthLayerImage& image, const PsdClippingBaseState& base) {
    auto clippingAt = [](auto& values, std::size_t index, std::size_t sourceLine) -> decltype(auto) {
        if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), sourceLine);
        return values[index];
    };
    auto pixelCount = (size_t)std::max(0, depthsample::detail::mul32(image.width, image.height));
    if (image.data.size() < pixelCount * 4 || base.alpha.size() != (size_t)(depthsample::detail::mul32(base.width, base.height))) return;
    compat::Slice<std::uint8_t> alpha;
    alpha.resize(pixelCount);
    for (std::size_t i = 0; i < static_cast<std::size_t>(pixelCount); ++i) {
        auto& destination = clippingAt(alpha, i, 2134);
        destination = clippingAt(image.data, i * 4 + 3, 2134);
    }
    const auto baseAlpha = base.alpha;
    auto clippedAlpha = depthimage::ngDepthDrawApplyPsdMaskToAlphaSource(
        alpha,
        image.width,
        image.height,
        image.left,
        image.top,
        baseAlpha,
        base.width,
        base.height,
        base.left,
        base.top,
        false,
        0
    );
    for (std::size_t i = 0; i < clippedAlpha.size(); ++i) {
        auto value = clippedAlpha[i];
        clippingAt(image.data, i * 4 + 3, 2149) = value;
    }
}

compat::Slice<std::uint8_t> buildDepthMaskPreview(DepthLayerImage& layer, PsdDepthImportSettings& settings) {
    compat::Slice<std::uint8_t> result;
    auto resultAt = [&](std::size_t index, std::size_t sourceLine) -> std::uint8_t& {
        if (index >= result.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, result.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), sourceLine);
        return result[index];
    };
    auto pixels = (size_t)std::max(0, depthsample::detail::mul32(layer.width, layer.height));
    result.resize(pixels * 4);
    for (int y = 0; y < layer.height; y++) {
        for (int x = 0; x < layer.width; x++) {
            auto index = ((size_t)y * (size_t)layer.width + (size_t)x) * 4;
            if (!acceptsDepthPixel(layer, index, x, y, settings)) {
                resultAt(index + 0, 2160) = 0;
                resultAt(index + 1, 2161) = 0;
                resultAt(index + 2, 2162) = 0;
                resultAt(index + 3, 2163) = 0;
                continue;
            }
            auto sampleSettings = settings;
            auto gray = (std::uint8_t)std::round(pixelDepth01(layer.data, index, sampleSettings) * 255.0f);
            resultAt(index + 0, 2168) = gray;
            resultAt(index + 1, 2169) = gray;
            resultAt(index + 2, 2170) = gray;
            resultAt(index + 3, 2171) = 255;
        }
    }
    return result;
}

compat::Slice<std::uint8_t> buildDepthMaskPreviewFromSurface(
    DepthLayerImage& layer,
    PsdDepthImportSettings& settings,
    compat::Slice<std::uint8_t> surfaceRgba
) {
    return buildDepthMaskPreviewFromSurface(layer, settings,
        std::span<const std::uint8_t>(surfaceRgba.data(), surfaceRgba.size()));
}

compat::Slice<std::uint8_t> buildDepthMaskPreviewFromSurface(
    DepthLayerImage& layer,
    PsdDepthImportSettings& settings,
    compat::ConstSlice<std::uint8_t> surfaceRgba
) {
    return buildDepthMaskPreviewFromSurface(layer, settings, surfaceRgba.view());
}

compat::Slice<std::uint8_t> buildDepthMaskPreviewFromSurface(
    DepthLayerImage& layer,
    PsdDepthImportSettings& settings,
    std::span<const std::uint8_t> surfaceRgba
) {
    compat::Slice<std::uint8_t> result;
    auto resultAt = [&](std::size_t index, std::size_t sourceLine) -> std::uint8_t& {
        if (index >= result.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            index, result.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), sourceLine);
        return result[index];
    };
    auto pixels = (size_t)std::max(0, depthsample::detail::mul32(layer.width, layer.height));
    result.resize(pixels * 4);
    auto sampleSettings = settings;
    for (int y = 0; y < layer.height; y++) {
        for (int x = 0; x < layer.width; x++) {
            auto index = ((size_t)y * (size_t)layer.width + (size_t)x) * 4;
            if (index + 3 >= layer.data.size() || index + 3 >= surfaceRgba.size()) continue;
            auto surfaceAlpha01 = (float)surfaceRgba[index + 3] / 255.0f;
            auto coverageAlpha01 = coverageAlpha(layer, x, y);
            auto depth01 = pixelDepth01(layer.data, index, sampleSettings);
            if (!ngDepthSampleAcceptsAlpha(surfaceAlpha01, settings.alphaThreshold) ||
                !ngDepthSampleAcceptsAlpha(coverageAlpha01, settings.alphaThreshold) ||
                !coverageReliable(layer, x, y) ||
                depth01 <= 0.0f) {
                continue;
            }
            auto gray = (std::uint8_t)std::round(depth01 * 255.0f);
            resultAt(index + 0, 2200) = gray;
            resultAt(index + 1, 2201) = gray;
            resultAt(index + 2, 2202) = gray;
            resultAt(index + 3, 2203) = 255;
        }
    }
    return result;
}

compat::Slice<std::uint8_t> sampleDepthToSurfaceLayer(
    std::span<const std::uint8_t> depthRgba,
    int depthWidth,
    int depthHeight,
    int layerLeft,
    int layerTop,
    int layerWidth,
    int layerHeight
) {
    compat::Slice<std::uint8_t> result;
    if (layerWidth <= 0 || layerHeight <= 0) return result;
    result.resize((size_t)layerWidth * (size_t)layerHeight * 4);
    for (auto y = 0; y < layerHeight; ++y) {
        auto depthY = depthsample::detail::add32(layerTop, y);
        for (auto x = 0; x < layerWidth; ++x) {
            auto depthX = depthsample::detail::add32(layerLeft, x);
            auto outIndex = ((size_t)y * (size_t)layerWidth + (size_t)x) * 4;
            if (depthX < 0 || depthY < 0 || depthX >= depthWidth || depthY >= depthHeight) continue;
            auto inIndex = ((size_t)depthY * (size_t)depthWidth + (size_t)depthX) * 4;
            if (inIndex + 3 >= depthRgba.size()) continue;
            const auto outEnd = outIndex + 4;
            if (outIndex > outEnd || outEnd > result.size())
                throw compat::ngStaticError<compat::ArraySliceError>(outIndex, outEnd, result.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 2229);
            auto destination = result.slice(outIndex, outEnd);
            const auto inEnd = inIndex + 4;
            if (inIndex > inEnd || inEnd > depthRgba.size())
                throw compat::ngStaticError<compat::ArraySliceError>(inIndex, inEnd, depthRgba.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 2229);
            std::copy_n(depthRgba.data() + inIndex, 4, destination.data());
        }
    }
    return result;
}

    compat::Slice<std::uint8_t> sampleDepthToSurfaceLayerFlippedY(
        std::span<const std::uint8_t> depthRgba,
        int depthWidth,
        int depthHeight,
        int layerLeft,
    int layerTop,
    int layerWidth,
    int layerHeight
) {
    return sampleDepthToSurfaceLayer(
        depthRgba,
        depthWidth,
        depthHeight,
        layerLeft,
        depthsample::detail::sub32(depthsample::detail::sub32(depthHeight, layerTop), layerHeight),
        layerWidth,
        layerHeight
    );
}

size_t depthSurfaceOverlap(
    std::span<const std::uint8_t> depthRgba,
    std::span<const std::uint8_t> surfaceRgba
) {
    size_t result = 0;
    auto pixels = std::min(depthRgba.size(), surfaceRgba.size()) / 4;
    for (std::size_t i = 0; i < static_cast<std::size_t>(pixels); ++i) {
        auto index = i * 4;
        if (compat::ngArrayAt(surfaceRgba, index + 3) >= 3 && compat::ngArrayAt(depthRgba, index) > 0) result++;
    }
    return result;
}

void compositeFlatDepthByCoverageSources(
    DepthLayerImage& image,
    std::span<const std::uint8_t> depthRgba,
    int depthWidth,
    int depthHeight
) {
    if (image.coverageSources.size() == 0 || image.width <= 0 || image.height <= 0) return;
    compat::Slice<std::uint8_t> result;
    result.resize((size_t)image.width * (size_t)image.height * 4);

    for (auto sources = image.coverageSources; auto& source : sources) {
        if (source.data.size() == 0 || source.width <= 0 || source.height <= 0 || source.channels <= 0) continue;
        auto sourceLength = (size_t)source.width * (size_t)source.height * (size_t)source.channels;
        if (source.data.size() < sourceLength) continue;

        auto localToWorld = source.localToDocumentWorld;
        for (int y = 0; y < source.height; y++) {
            for (int x = 0; x < source.width; x++) {
                auto sourceIndex = ((size_t)y * (size_t)source.width + (size_t)x) *
                    (size_t)source.channels;
                auto alpha = source.channels > 3 ? source.data[sourceIndex + 3] : 255;
                if (alpha == 0) continue;

                auto local = Vec2(
                    (float)x + 0.5f - (float)source.width / 2.0f,
                    (float)y + 0.5f - (float)source.height / 2.0f
                );
                auto world = transformXY(localToWorld, local);
                auto documentX = world.x + (float)image.documentWidth / 2.0f;
                auto documentY = world.y + (float)image.documentHeight / 2.0f;
                auto outX = depthsample::detail::sub32(compat::ngFloatToInt32(std::floor(documentX)), image.left);
                auto outY = depthsample::detail::sub32(compat::ngFloatToInt32(std::floor(documentY)), image.top);
                if (outX < 0 || outY < 0 || outX >= image.width || outY >= image.height) continue;

                auto depthX = compat::ngFloatToInt32(std::floor(
                    documentX * (float)depthWidth / (float)std::max(1, image.documentWidth)));
                auto depthY = compat::ngFloatToInt32(std::floor(
                    documentY * (float)depthHeight / (float)std::max(1, image.documentHeight)));
                if (depthX < 0 || depthY < 0 || depthX >= depthWidth || depthY >= depthHeight) continue;

                auto depthIndex = ((size_t)depthY * (size_t)depthWidth + (size_t)depthX) * 4;
                auto outIndex = ((size_t)outY * (size_t)image.width + (size_t)outX) * 4;
                if (depthIndex + 3 >= depthRgba.size() || outIndex + 3 >= result.size()) continue;
                result[outIndex + 0] = compat::ngArrayAt(depthRgba, depthIndex + 0);
                result[outIndex + 1] = compat::ngArrayAt(depthRgba, depthIndex + 1);
                result[outIndex + 2] = compat::ngArrayAt(depthRgba, depthIndex + 2);
                result[outIndex + 3] = 255;
            }
        }
    }

    image.data = result;
}

PsdDepthLayerDepthStats computeLayerDepthStats(DepthLayerImage& layer, PsdDepthImportSettings& settings) {
    PsdDepthLayerDepthStats stats;
    bool hasDepth = false;
    float previousDepth01 = std::numeric_limits<float>::quiet_NaN();
    bool hasPrevious = false;
    double adjacentSum = std::numeric_limits<double>::quiet_NaN();
    size_t adjacentCount = 0;

    for (int y = 0; y < layer.height; y++) {
        for (int x = 0; x < layer.width; x++) {
            auto index = ((size_t)y * (size_t)layer.width + (size_t)x) * 4;
            if (!acceptsDepthPixel(layer, index, x, y, settings)) {
                continue;
            }

            auto sampleSettings = settings;
            auto depth01 = pixelDepth01(layer.data, index, sampleSettings);
            stats.maskedPixels++;
            if (depth01 <= 0.0f) stats.zeroPixels++;
            if (!hasDepth) {
                stats.minDepth01 = depth01;
                stats.maxDepth01 = depth01;
                hasDepth = true;
            } else {
                if (depth01 < stats.minDepth01) stats.minDepth01 = depth01;
                if (depth01 > stats.maxDepth01) stats.maxDepth01 = depth01;
            }
            if (hasPrevious) {
                adjacentSum += std::abs(depth01 - previousDepth01);
                adjacentCount++;
            }
            previousDepth01 = depth01;
            hasPrevious = true;
        }
        hasPrevious = false;
    }

    stats.hasDepth = hasDepth;
    if (hasDepth) stats.rangeDepth01 = stats.maxDepth01 - stats.minDepth01;
    if (adjacentCount > 0) stats.adjacentDelta01 = (float)(adjacentSum / (double)adjacentCount);
    return stats;
}

DepthSample samplePixel(DepthLayerImage& layer, int x, int y, PsdDepthImportSettings& settings) {
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return DepthSample{false, 0, 0};
    auto index = ((size_t)y * (size_t)layer.width + (size_t)x) * 4;
    if (!acceptsDepthPixel(layer, index, x, y, settings)) return DepthSample{false, 0, 0};
    auto alpha = surfaceAlpha(layer, index, x, y);
    auto sampleSettings = settings;
    return DepthSample{true, pixelDepth(layer.data, index, sampleSettings), alpha};
}

DepthSample aggregateToDepthSample(DepthSampleAggregate aggregate) {
    return DepthSample{aggregate.valid, aggregate.value, aggregate.weight};
}

DepthSample sampleLayer(DepthLayerImage& layer, float x, float y, PsdDepthImportSettings& settings) {
    auto cx = compat::ngFloatToInt32(std::round(x));
    auto cy = compat::ngFloatToInt32(std::round(y));
    auto convolution = sampleConvolution(settings.convolution);

    auto sampleAt = [&](int sampleX, int sampleY) -> DepthSamplePoint {
        auto sample = samplePixel(layer, sampleX, sampleY, settings);
        if (!sample.valid) return ngDepthSampleMissingPoint();
        return DepthSamplePoint{sample.valid, sample.value, sample.weight};
    };

    return aggregateToDepthSample(ngDepthSampleConvolve(sampleAt, convolution, settings.customRadius, cx, cy));
}

}
#if defined(NG_COMMAND_BROWSER_DIFFERENTIAL)
namespace nijigenerate::io::depthmap_psd {
using namespace detail;

    compat::Slice<std::uint8_t> ngPsdDepthAttachCompositeSourceCoverageForRegression(
        int depthLeft,
        int depthTop,
        int depthWidth,
        int depthHeight,
        PsdDepthCompositeSourceLayer colorLayer
    ) {
        DepthLayerImage image;
        image.left = depthLeft;
        image.top = depthTop;
        image.width = depthWidth;
        image.height = depthHeight;
        attachCompositeSourceCoverage(image, colorLayer);
        return image.coverageData;
    }

    compat::Slice<ptrdiff_t> ngPsdDepthPairCompositeSourceLayersForRegression(
        compat::Slice<PsdDepthCompositeSourceLayer> colorLayers,
        compat::Slice<PsdDepthCompositeSourceLayer> depthLayers
    ) {
        nicxlive::core::common::SharedArray<bool> reserved;
        reserved.resize(depthLayers.size());
        compat::Slice<ptrdiff_t> result;
        for (auto colorLayer : colorLayers) {
            auto index = bestMatchingDepthSourceLayer(colorLayer, depthLayers, reserved);
            result.append(index);
            if (index >= 0) reserved[(size_t)index] = true;
        }
        return result;
    }

    compat::Slice<ptrdiff_t> ngPsdDepthPairCoverageSourceLayersForRegression(
        compat::Slice<PsdDepthCompositeSourceLayer> depthLayers,
        compat::Slice<PsdDepthCompositeSourceLayer> colorLayers
    ) {
        nicxlive::core::common::SharedArray<bool> reserved;
        reserved.resize(colorLayers.size());
        compat::Slice<ptrdiff_t> result;
        for (auto depthLayer : depthLayers) {
            auto index = bestMatchingColorSourceLayer(depthLayer, colorLayers, reserved);
            result.append(index);
            if (index >= 0) reserved[(size_t)index] = true;
        }
        return result;
    }

}
#endif

namespace nijigenerate::io::depthmap_psd {
using namespace detail;
PsdDepthSampleResult ngPsdDepthSamplePixels(
    compat::Slice<std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    PsdDepthImportSettings settings
) {
    return ngPsdDepthSamplePixelsWithOpacity(std::move(rgba), width, height, x, y, 1.0f, std::move(settings));
}

PsdDepthSampleResult ngPsdDepthSamplePixels(
    compat::ConstSlice<std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    PsdDepthImportSettings settings
) {
    return ngPsdDepthSamplePixelsWithOpacity(std::move(rgba), width, height, x, y, 1.0f, std::move(settings));
}

PsdDepthSampleResult ngPsdDepthSamplePixels(
    std::span<const std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    PsdDepthImportSettings settings
) {
    return ngPsdDepthSamplePixelsWithOpacity(rgba, width, height, x, y, 1.0f, settings);
}

PsdDepthSampleResult ngPsdDepthSamplePixelsWithOpacity(
    compat::Slice<std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    float opacity,
    PsdDepthImportSettings settings
) {
    return ngPsdDepthSamplePixelsWithOpacity(
        std::span<const std::uint8_t>{rgba.data(), rgba.size()}, width, height, x, y, opacity,
        std::move(settings));
}

PsdDepthSampleResult ngPsdDepthSamplePixelsWithOpacity(
    compat::ConstSlice<std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    float opacity,
    PsdDepthImportSettings settings
) {
    return ngPsdDepthSamplePixelsWithOpacity(rgba.view(), width, height, x, y, opacity, std::move(settings));
}

PsdDepthSampleResult ngPsdDepthSamplePixelsWithOpacity(
    std::span<const std::uint8_t> rgba,
    int width,
    int height,
    float x,
    float y,
    float opacity,
    PsdDepthImportSettings settings
) {
    enforce(width >= 0 && height >= 0, "Image dimensions must be non-negative", 2788);
    enforce(rgba.size() >= (size_t)std::max(0, depthsample::detail::mul32(width, height)) * 4,
        "RGBA buffer is smaller than dimensions", 2789);
    DepthLayerImage layer;
    layer.width = width;
    layer.height = height;
    layer.opacity = clamp01(opacity);
    layer.data = rgba.empty() ? compat::Slice<std::uint8_t>{} : compat::Slice<std::uint8_t>(rgba);
    auto sample = sampleLayer(layer, x, y, settings);
    return PsdDepthSampleResult{sample.valid, sample.value};
}

PsdDepthSampleResult ngPsdDepthSamplePixelsWithCoverage(
    compat::Slice<std::uint8_t> rgba,
    int width,
    int height,
    compat::Slice<std::uint8_t> coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    PsdDepthImportSettings settings
) {
    return ngPsdDepthSamplePixelsWithCoverage(
        std::span<const std::uint8_t>{rgba.data(), rgba.size()}, width, height,
        std::span<const std::uint8_t>{coverageRgba.data(), coverageRgba.size()},
        coverageWidth, coverageHeight, coverageOpacity, x, y, std::move(settings));
}

PsdDepthSampleResult ngPsdDepthSamplePixelsWithCoverage(
    compat::ConstSlice<std::uint8_t> rgba,
    int width,
    int height,
    compat::ConstSlice<std::uint8_t> coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    PsdDepthImportSettings settings
) {
    return ngPsdDepthSamplePixelsWithCoverage(rgba.view(), width, height, coverageRgba.view(),
        coverageWidth, coverageHeight, coverageOpacity, x, y, std::move(settings));
}

PsdDepthSampleResult ngPsdDepthSamplePixelsWithCoverage(
    compat::Slice<std::uint8_t> rgba,
    int width,
    int height,
    compat::ConstSlice<std::uint8_t> coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    PsdDepthImportSettings settings
) {
    return ngPsdDepthSamplePixelsWithCoverage(
        std::span<const std::uint8_t>{rgba.data(), rgba.size()}, width, height, coverageRgba.view(),
        coverageWidth, coverageHeight, coverageOpacity, x, y, std::move(settings));
}

PsdDepthSampleResult ngPsdDepthSamplePixelsWithCoverage(
    compat::ConstSlice<std::uint8_t> rgba,
    int width,
    int height,
    compat::Slice<std::uint8_t> coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    PsdDepthImportSettings settings
) {
    return ngPsdDepthSamplePixelsWithCoverage(rgba.view(), width, height,
        std::span<const std::uint8_t>{coverageRgba.data(), coverageRgba.size()},
        coverageWidth, coverageHeight, coverageOpacity, x, y, std::move(settings));
}

PsdDepthSampleResult ngPsdDepthSamplePixelsWithCoverage(
    std::span<const std::uint8_t> rgba,
    int width,
    int height,
    std::span<const std::uint8_t> coverageRgba,
    int coverageWidth,
    int coverageHeight,
    float coverageOpacity,
    float x,
    float y,
    PsdDepthImportSettings settings
) {
    enforce(width >= 0 && height >= 0, "Image dimensions must be non-negative", 2811);
    enforce(coverageWidth >= 0 && coverageHeight >= 0, "Coverage dimensions must be non-negative", 2812);
    enforce(rgba.size() >= (size_t)std::max(0, depthsample::detail::mul32(width, height)) * 4,
        "RGBA buffer is smaller than dimensions", 2813);
    enforce(coverageRgba.size() >= (size_t)std::max(0,
        depthsample::detail::mul32(coverageWidth, coverageHeight)) * 4,
        "Coverage RGBA buffer is smaller than dimensions", 2814);
    DepthLayerImage layer;
    layer.width = width;
    layer.height = height;
    layer.data = rgba.empty() ? compat::Slice<std::uint8_t>{} : compat::Slice<std::uint8_t>(rgba);
    layer.coverageWidth = coverageWidth;
    layer.coverageHeight = coverageHeight;
    layer.coverageChannels = 4;
    layer.coverageOpacity = clamp01(coverageOpacity);
    layer.coverageData = coverageRgba.empty() ? compat::Slice<std::uint8_t>{}
        : compat::Slice<std::uint8_t>(coverageRgba);
    auto sample = sampleLayer(layer, x, y, settings);
    return PsdDepthSampleResult{sample.valid, sample.value};
}

PsdDepthSampleResult ngPsdDepthFrontmost(compat::Slice<PsdDepthSampleResult> samples) {
    PsdDepthSampleResult best;
    for (auto sample : samples) {
        if (!sample.valid || !std::isfinite(sample.value)) continue;
        if (!best.valid || sample.value > best.value) best = sample;
    }
    return best;
}

namespace detail {
void finalizeGridResult(PsdDepthGridResult& result, GridAccum& accum, PsdDepthImportSettings& settings) {
    auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(accum.grid);
    compat::Slice<float> existing;
    if (mapped != nullptr) existing = mapped->copyDepths();
    if (existing.isNull() || existing.size() != accum.best.size()) {
        existing.resize(accum.best.size());
        std::fill(existing.begin(), existing.end(), 0.0f);
    }

    result.grid = accum.grid;
    result.depths.resize(accum.best.size(), std::numeric_limits<float>::quiet_NaN());
    result.baseDepths.resize(accum.best.size(), std::numeric_limits<float>::quiet_NaN());
    result.winnerLayerPaths.resize(accum.best.size());
    result.missingVertexMask.resize(accum.best.size());
    result.layerMasks = accum.layerMasks.duplicate();
    if (!ngPsdDepthGridEnabled(settings, compat::ngRequireObject(accum.grid)->getUUID())) {
        result.depths = existing;
        std::fill(result.missingVertexMask.begin(), result.missingVertexMask.end(), true);
        result.skipped = true;
        return;
    }
    const auto winnerPaths = accum.winnerLayerPaths;
    for (auto winnerLayerPath : winnerPaths) {
        if (winnerLayerPath.size() == 0) continue;
        auto masks = result.layerMasks;
        for (auto& mask : masks) {
            if (mask.layerPath == winnerLayerPath) {
                mask.selectedVertices++;
                break;
            }
        }
    }
    bool hasMinMax = false;
    const auto count = accum.best.size();
    const auto finalAt = [](auto& values, std::size_t index, std::size_t line) -> decltype(auto) {
        if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), line);
        return values[index];
    };
    for (std::size_t i = 0; i < count; ++i) {
        if (finalAt(accum.has, i, 2870)) {
            {
                auto depths = result.depths;
                auto& destination = finalAt(depths, i, 2871);
                destination = ext::nodes::ngFiniteDepthOrZero(finalAt(accum.best, i, 2871));
            }
            {
                auto depths = result.baseDepths;
                auto& destination = finalAt(depths, i, 2872);
                destination = ext::nodes::ngFiniteDepthOrZero(finalAt(accum.baseBest, i, 2872));
            }
            {
                auto paths = result.winnerLayerPaths;
                auto& destination = finalAt(paths, i, 2873);
                destination = finalAt(accum.winnerLayerPaths, i, 2873);
            }
            finalAt(result.missingVertexMask, i, 2874) = false;
            result.sampledVertices++;
            if (!hasMinMax || finalAt(result.depths, i, 2876) < result.minDepth)
                result.minDepth = finalAt(result.depths, i, 2876);
            if (!hasMinMax || finalAt(result.depths, i, 2877) > result.maxDepth)
                result.maxDepth = finalAt(result.depths, i, 2877);
            hasMinMax = true;
        } else {
            finalAt(result.missingVertexMask, i, 2880) = true;
            result.missingVertices++;
            switch (settings.missingPolicy) {
                case PsdDepthMissingPolicy::KeepExisting: {
                    auto depths = result.depths;
                    auto& destination = finalAt(depths, i, 2884);
                    destination = finalAt(existing, i, 2884);
                    break;
                }
                case PsdDepthMissingPolicy::SetZero:
                    finalAt(result.depths, i, 2887) = 0.0f;
                    break;
                case PsdDepthMissingPolicy::SetBack: {
                    auto depths = result.depths;
                    auto& destination = finalAt(depths, i, 2890);
                    destination = ext::nodes::ngFiniteDepthOrZero(settings.backDepth * settings.depthScale);
                    break;
                }
                case PsdDepthMissingPolicy::SkipGrid:
                    result.skipped = true;
                    result.depths = existing;
                    return;
                default:
                    throw compat::ngStaticError<PsdDepthEnumSwitchError>(
                        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 2882);
            }
        }
    }
    if (!hasMinMax) {
        result.minDepth = 0.0f;
        result.maxDepth = 0.0f;
    }
    ext::nodes::ngNormalizeDepths(result.depths);
    ext::nodes::ngNormalizeDepths(result.baseDepths);
}

void buildCompositePreview(
    PsdDepthGridResult& result,
    compat::Slice<DepthLayerImage> layers,
    std::shared_ptr<nicxlive::core::nodes::Deformable> grid,
    PsdDepthImportSettings& settings
) {
    constexpr int MaxPreviewSize = 192;

    bool hasBounds = false;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    for (auto& layer : layers) {
        if (layer.grid != grid || layer.width <= 0 || layer.height <= 0) continue;
        auto layerRight = depthsample::detail::add32(layer.left, layer.width);
        auto layerBottom = depthsample::detail::add32(layer.top, layer.height);
        if (!hasBounds) {
            left = layer.left;
            top = layer.top;
            right = layerRight;
            bottom = layerBottom;
            hasBounds = true;
        } else {
            left = std::min(left, layer.left);
            top = std::min(top, layer.top);
            right = std::max(right, layerRight);
            bottom = std::max(bottom, layerBottom);
        }
    }
    if (!hasBounds || right <= left || bottom <= top) return;

    auto sourceWidth = depthsample::detail::sub32(right, left);
    auto sourceHeight = depthsample::detail::sub32(bottom, top);
    auto scale = std::min(
        (float)MaxPreviewSize / (float)sourceWidth,
        (float)MaxPreviewSize / (float)sourceHeight
    );
    if (scale > 1.0f) scale = 1.0f;
    if (scale <= 0.0f) scale = 1.0f;

    auto width = std::max(1, compat::ngFloatToInt32(std::round((float)sourceWidth * scale)));
    auto height = std::max(1, compat::ngFloatToInt32(std::round((float)sourceHeight * scale)));
    result.previewLeft = left;
    result.previewTop = top;
    result.previewWidth = width;
    result.previewHeight = height;
    result.rawCompositePreviewRgba.resize((size_t)width * (size_t)height * 4);
    result.compositePreviewRgba.resize((size_t)width * (size_t)height * 4);

    for (std::size_t py = 0; py < static_cast<std::size_t>(height); ++py) {
        for (std::size_t px = 0; px < static_cast<std::size_t>(width); ++px) {
            auto documentX = (float)left + ((float)px + 0.5f) / scale;
            auto documentY = (float)top + ((float)py + 0.5f) / scale;
            bool hasRawSample = false;
            float rawBestDepth = 0.0f;
            float rawBestGray = 0.0f;
            std::uint8_t rawBestAlpha = 0;
            bool hasSample = false;
            float bestDepth = 0.0f;
            float bestGray = 0.0f;
            std::uint8_t bestAlpha = 0;

            for (auto& layer : layers) {
                if (layer.grid != grid) continue;
                if (!ngPsdDepthGridLayerEnabled(settings,
                    compat::ngRequireObject(grid)->getUUID(), layer.layerPath)) continue;
                auto layerX = compat::ngFloatToInt32(std::round(layerToSourceX(layer, documentX)));
                auto layerY = compat::ngFloatToInt32(std::round(layerToSourceY(layer, documentY)));
                if (layerX < 0 || layerY < 0 || layerX >= layer.width || layerY >= layer.height) continue;
                auto index = ((size_t)layerY * (size_t)layer.width + (size_t)layerX) * 4;
                auto rawAlpha = surfaceAlpha(layer, index, layerX, layerY);
                if (ngDepthSampleAcceptsAlpha(rawAlpha, settings.alphaThreshold)) {
                    auto sampleSettings = settings;
                    auto rawDepth = pixelDepth(layer.data, index, sampleSettings);
                    if (!hasRawSample || rawDepth > rawBestDepth) {
                        hasRawSample = true;
                        rawBestDepth = rawDepth;
                        rawBestGray = pixelDepth01(layer.data, index, sampleSettings);
                        rawBestAlpha = ngDepthSampleAlphaByte(rawAlpha);
                    }
                }
                if (!coverageReliable(layer, layerX, layerY)) continue;
                auto alpha = surfaceAlpha(layer, index, layerX, layerY);
                if (!ngDepthSampleAcceptsAlpha(alpha, settings.alphaThreshold)) continue;
                auto sampleSettings = settings;
                auto depth = pixelDepth(layer.data, index, sampleSettings);
                if (!hasSample || depth > bestDepth) {
                    hasSample = true;
                    bestDepth = depth;
                    bestGray = pixelDepth01(layer.data, index, sampleSettings);
                    bestAlpha = ngDepthSampleAlphaByte(alpha);
                }
            }

            auto previewAt = [](compat::Slice<std::uint8_t>& values, std::size_t index,
                                std::size_t sourceLine) -> std::uint8_t& {
                if (index >= values.size())
                    throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
                        compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"),
                        sourceLine);
                return values[index];
            };
            auto outIndex = ((size_t)py * (size_t)width + (size_t)px) * 4;
            if (!hasRawSample) {
                previewAt(result.rawCompositePreviewRgba, outIndex + 0, 3003) = 0;
                previewAt(result.rawCompositePreviewRgba, outIndex + 1, 3004) = 0;
                previewAt(result.rawCompositePreviewRgba, outIndex + 2, 3005) = 0;
                previewAt(result.rawCompositePreviewRgba, outIndex + 3, 3006) = 0;
            } else {
                auto gray = (std::uint8_t)std::round(rawBestGray * 255.0f);
                previewAt(result.rawCompositePreviewRgba, outIndex + 0, 3009) = gray;
                previewAt(result.rawCompositePreviewRgba, outIndex + 1, 3010) = gray;
                previewAt(result.rawCompositePreviewRgba, outIndex + 2, 3011) = gray;
                previewAt(result.rawCompositePreviewRgba, outIndex + 3, 3012) = rawBestAlpha;
            }
            if (!hasSample) {
                previewAt(result.compositePreviewRgba, outIndex + 0, 3015) = 0;
                previewAt(result.compositePreviewRgba, outIndex + 1, 3016) = 0;
                previewAt(result.compositePreviewRgba, outIndex + 2, 3017) = 0;
                previewAt(result.compositePreviewRgba, outIndex + 3, 3018) = 0;
            } else {
                auto gray = (std::uint8_t)std::round(bestGray * 255.0f);
                previewAt(result.compositePreviewRgba, outIndex + 0, 3021) = gray;
                previewAt(result.compositePreviewRgba, outIndex + 1, 3022) = gray;
                previewAt(result.compositePreviewRgba, outIndex + 2, 3023) = gray;
                previewAt(result.compositePreviewRgba, outIndex + 3, 3024) = bestAlpha;
            }
        }
    }
}

}
}
