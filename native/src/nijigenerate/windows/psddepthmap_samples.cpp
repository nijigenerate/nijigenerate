#include "psddepthmap.hpp"
#include "detail/psddepthmap_helpers.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/io/depthimage.hpp>
#include <nijigenerate/io/depthmap_psd_composition.hpp>
#include <nijigenerate/io/depthmap_psd_enum_functions.hpp>
#include <nijigenerate/viewport/depth/common/targetview.hpp>
#include <nijigenerate/project.hpp>
#include <puppet.hpp>
#include <nodes/deformable.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <type_traits>
namespace nijigenerate::windows::psddepthmap {
using namespace detail::helpers;
using namespace io::depthsample;
using namespace io::depthimage;
using namespace viewport::depth::common::targetview;
PsdDepth3DAdjustGeometryStats PSDDepthMapWindow::threeDAdjustGeometryStats(PsdDepthGridResult& gridResult) {
    PsdDepth3DAdjustGeometryStats stats;
    const auto keepAlive = weak_from_this().lock();
    if (!gridResult.grid) return stats;
    const auto grids = preview.grids;
    auto targetView = allocatePsdDepthObject<DepthTargetView>(gridResult.grid);
    auto targetVertices = targetView->getVertices();
    const auto layerMasks = gridResult.layerMasks;
    for (auto layerMask : layerMasks) {
        const auto layerPath = layerMask.layerPath;
        const auto gridUuid = compat::ngRequireObject(gridResult.grid)->getUUID();
        if (!findLayerPreview(layerPath, gridUuid)) continue;
        ++stats.layerPlanes;
        stats.depthRangeLines += 2;
    }
    if (targetVertices.size() > 1) stats.targetWireLines = targetVertices.size() - 1;
    const auto vertexCount = targetVertices.size();
    for (std::size_t i = 0; i < vertexCount; ++i) {
        if (i < gridResult.missingVertexMask.size() && compat::ngArrayAt(gridResult.missingVertexMask, i)) {
            ++stats.missingPoints;
        } else ++stats.sampledPoints;
    }
    return stats;
}
void PSDDepthMapWindow::resetPsdDepth3DAdjustCameraToBounds(float sourceWidth, float sourceHeight, ImVec2 canvasSize) {
        threeDAdjustCamera.yaw = 0.42f;
        threeDAdjustCamera.pitch = 0.35f;
        threeDAdjustCamera.zoom = dmin(
            (canvasSize.x - 80.0f) / dmax(1.0f, sourceWidth),
            (canvasSize.y - 80.0f) / dmax(1.0f, sourceHeight)
        );
        threeDAdjustCamera.zoom = dclamp(threeDAdjustCamera.zoom, 0.1f, 8.0f);
        threeDAdjustCamera.pan = Vec2{0,0};
        threeDAdjustPreviewDirty = true;
    }

bool PSDDepthMapWindow::threeDAdjustCameraChanged() {
        return threeDAdjustLastYaw != threeDAdjustCamera.yaw ||
            threeDAdjustLastPitch != threeDAdjustCamera.pitch ||
            threeDAdjustLastZoom != threeDAdjustCamera.zoom ||
            threeDAdjustLastPan.x != threeDAdjustCamera.pan.x ||
            threeDAdjustLastPan.y != threeDAdjustCamera.pan.y;
    }

void PSDDepthMapWindow::rememberThreeDAdjustCamera() {
        threeDAdjustLastYaw = threeDAdjustCamera.yaw;
        threeDAdjustLastPitch = threeDAdjustCamera.pitch;
        threeDAdjustLastZoom = threeDAdjustCamera.zoom;
        threeDAdjustLastPan = threeDAdjustCamera.pan;
    }

bool PSDDepthMapWindow::buildThreeDAdjustOverallPreview(
        int framebufferWidth,
        int framebufferHeight,
        PsdDepthDialogOverallPreview& result
    ) {
        const auto keepAlive = weak_from_this().lock();
        result = {};
        framebufferWidth = dmax(1, framebufferWidth);
        framebufferHeight = dmax(1, framebufferHeight);

        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        bool hasBounds = 0;
        auto includeBounds = [&](int x0, int y0, int x1, int y1) -> void {
            if (!hasBounds) {
                left = x0;
                top = y0;
                right = x1;
                bottom = y1;
                hasBounds = true;
            } else {
                left = dmin(left, x0);
                top = dmin(top, y0);
                right = dmax(right, x1);
                bottom = dmax(bottom, y1);
            }
        };
        for (auto layerPreviews = preview.composedLayers; auto& layerPreview : layerPreviews) {
            if (threeDAdjustLayerSamples(layerPreview).size() == 0) continue;
            includeBounds(
                layerPreview.left,
                layerPreview.top,
                add32(layerPreview.left, layerPreview.width),
                add32(layerPreview.top, layerPreview.height)
            );
        }
        if (!hasBounds) {
            includeBounds(0, 0, dmax(1, preview.compositionWidth), dmax(1, preview.compositionHeight));
        }
        if (right <= left) right = add32(left, 1);
        if (bottom <= top) bottom = add32(top, 1);

        auto centerX = ((float)left + (float)right) * 0.5f;
        auto centerY = ((float)top + (float)bottom) * 0.5f;
        auto sourceWidth = (float)sub32(right, left);
        auto sourceHeight = (float)sub32(bottom, top);
        auto canvasSize = ImVec2((float)framebufferWidth, (float)framebufferHeight);
        constexpr std::uint64_t PsdDepth3DAdjustSceneCameraKey = UINT64_MAX;
        if (threeDAdjustCameraTargetUuid != PsdDepth3DAdjustSceneCameraKey ||
            threeDAdjustCameraLeft != left ||
            threeDAdjustCameraTop != top ||
            threeDAdjustCameraRight != right ||
            threeDAdjustCameraBottom != bottom) {
            threeDAdjustCameraTargetUuid = PsdDepth3DAdjustSceneCameraKey;
            threeDAdjustCameraLeft = left;
            threeDAdjustCameraTop = top;
            threeDAdjustCameraRight = right;
            threeDAdjustCameraBottom = bottom;
            resetPsdDepth3DAdjustCameraToBounds(sourceWidth, sourceHeight, canvasSize);
        }

        result.width = framebufferWidth;
        result.height = framebufferHeight;
        result.yaw = threeDAdjustCamera.yaw;
        result.pitch = threeDAdjustCamera.pitch;
        result.zoom = threeDAdjustCamera.zoom;
        result.panX = threeDAdjustCamera.pan.x;
        result.panY = threeDAdjustCamera.pan.y;
        result.rgba.resize((size_t)framebufferWidth * (size_t)framebufferHeight * 4);
        result.depths.resize((size_t)framebufferWidth * (size_t)framebufferHeight,
            std::numeric_limits<float>::quiet_NaN());
        compat::Slice<float> zBuffer;
        zBuffer.resize((size_t)framebufferWidth * (size_t)framebufferHeight,
            std::numeric_limits<float>::quiet_NaN());
        const auto zLength = zBuffer.size();
        for (std::size_t i = 0; i < zLength; ++i) {
            auto pixelIndex = i * 4;
            compat::ngArrayAt(result.rgba, pixelIndex + 0) = 20;
            compat::ngArrayAt(result.rgba, pixelIndex + 1) = 23;
            compat::ngArrayAt(result.rgba, pixelIndex + 2) = 26;
            compat::ngArrayAt(result.rgba, pixelIndex + 3) = 255;
            compat::ngArrayAt(result.depths, i) = NAN;
            compat::ngArrayAt(zBuffer, i) = std::numeric_limits<float>::max();
        }

        auto yawCos = std::cos(threeDAdjustCamera.yaw);
        auto yawSin = std::sin(threeDAdjustCamera.yaw);
        auto pitchCos = std::cos(threeDAdjustCamera.pitch);
        auto pitchSin = std::sin(threeDAdjustCamera.pitch);
        auto depthDisplayScale = threeDAdjustDepthDisplayScale();

        for (auto layerPreviews = preview.composedLayers; auto& layerPreview : layerPreviews) {
            auto samples = threeDAdjustLayerSamples(layerPreview);
            for (auto sample : samples) {
                auto depth = threeDAdjustConstrainedDisplayDepth(layerPreview, sample);
                auto documentX = (float)layerPreview.left + (float)sample.x;
                auto documentY = (float)layerPreview.top + (float)sample.y;
                auto x = documentX - centerX;
                auto y = documentY - centerY;
                auto z = -depth * depthDisplayScale;
                auto rotatedX = x * yawCos + z * yawSin;
                auto rotatedZ = -x * yawSin + z * yawCos;
                auto rotatedY = y * pitchCos - rotatedZ * pitchSin;
                auto cameraDepth = y * pitchSin + rotatedZ * pitchCos;
                auto screenX = framebufferWidth * 0.5f +
                    rotatedX * threeDAdjustCamera.zoom + threeDAdjustCamera.pan.x;
                auto screenY = framebufferHeight * 0.5f +
                    rotatedY * threeDAdjustCamera.zoom + threeDAdjustCamera.pan.y;
                auto sx = toSourceInt(std::round(screenX));
                auto sy = toSourceInt(std::round(screenY));
                auto radius = 1;
                for (auto dy = -radius; dy < radius + 1; ++dy) {
                    auto pixelY = add32(sy, dy);
                    if (pixelY < 0 || pixelY >= framebufferHeight) continue;
                    for (auto dx = -radius; dx < radius + 1; ++dx) {
                        auto pixelX = add32(sx, dx);
                        if (pixelX < 0 || pixelX >= framebufferWidth) continue;
                        auto zIndex = (size_t)pixelY * (size_t)framebufferWidth +
                            (size_t)pixelX;
                        if (cameraDepth >= compat::ngArrayAt(zBuffer, zIndex)) continue;

                        auto alpha = (float)sample.a / 255.0f;
                        auto pixelIndex = zIndex * 4;
                        compat::ngArrayAt(result.rgba, pixelIndex + 0) = (std::uint8_t)dclamp(toSourceInt((
                            (float)sample.r * alpha +
                            (float)compat::ngArrayAt(result.rgba, pixelIndex + 0) * (1.0f - alpha) + 0.5f
                        )), 0, 255);
                        compat::ngArrayAt(result.rgba, pixelIndex + 1) = (std::uint8_t)dclamp(toSourceInt((
                            (float)sample.g * alpha +
                            (float)compat::ngArrayAt(result.rgba, pixelIndex + 1) * (1.0f - alpha) + 0.5f
                        )), 0, 255);
                        compat::ngArrayAt(result.rgba, pixelIndex + 2) = (std::uint8_t)dclamp(toSourceInt((
                            (float)sample.b * alpha +
                            (float)compat::ngArrayAt(result.rgba, pixelIndex + 2) * (1.0f - alpha) + 0.5f
                        )), 0, 255);
                        compat::ngArrayAt(result.rgba, pixelIndex + 3) = 255;
                        compat::ngArrayAt(result.depths, zIndex) = depth;
                        compat::ngArrayAt(zBuffer, zIndex) = cameraDepth;
                    }
                }
            }
        }
        result.minDepth = std::numeric_limits<float>::max();
        result.maxDepth = -std::numeric_limits<float>::max();
        for (auto depths = result.depths; const auto depth : depths) {
            if (depth != depth) continue;
            result.coveredPixels++;
            result.minDepth = dmin(result.minDepth, depth);
            result.maxDepth = dmax(result.maxDepth, depth);
        }
        if (result.coveredPixels == 0) {
            result.minDepth = 0.0f;
            result.maxDepth = 0.0f;
        }
        return true;
    }

DepthSampleChannel PSDDepthMapWindow::threeDAdjustSampleChannel(PsdDepthChannel channel) {
        switch (channel) {
        case PsdDepthChannel::AverageRGB:
            return DepthSampleChannel::AverageRGB;
        case PsdDepthChannel::R:
            return DepthSampleChannel::R;
        case PsdDepthChannel::G:
            return DepthSampleChannel::G;
        case PsdDepthChannel::B:
            return DepthSampleChannel::B;
        case PsdDepthChannel::Luminance:
            return DepthSampleChannel::Luminance;
        }
        throw compat::ngStaticError<compat::SwitchError>(
            compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/psddepthmap.d"), 2144);
    }

DepthSampleChannel PSDDepthMapWindow::threeDAdjustSampleChannel() {
        return threeDAdjustSampleChannel(settings.channel);
    }

bool PSDDepthMapWindow::threeDAdjustDepthAt(PsdDepthComposedLayer& layerPreview, int x, int y, float& depth) {
        depth = 0.0f;
        const auto keepAlive = weak_from_this().lock();
        std::ptrdiff_t layerIndex = -1;
        auto layers = preview.composedLayers;
        for (std::size_t i = 0; i < layers.size(); ++i) {
            auto& candidate = compat::ngArrayAt(layers, i);
            if (&candidate == &layerPreview) {
                layerIndex = (std::ptrdiff_t)i;
                break;
            }
        }
        if (layerIndex < 0) return false;
        auto sampleAt = [&](int sampleX, int sampleY) -> DepthSamplePoint {
            if (sampleX < 0 || sampleY < 0 || sampleX >= layerPreview.width || sampleY >= layerPreview.height) {
                return ngDepthSampleMissingPoint();
            }
            std::ptrdiff_t sourceIndex = 0;
            std::array<std::uint8_t, 4> sourcePixel{};
            if (!ngPsdDepthResolvedPixelAt(preview, (size_t)layerIndex,
                add32(layerPreview.left, sampleX), add32(layerPreview.top, sampleY), sourceIndex, sourcePixel)) {
                return ngDepthSampleMissingPoint();
            }
            auto sourceLayers = preview.composedLayers;
            auto source = &compat::ngArrayAt(sourceLayers, (size_t)sourceIndex);
            auto alpha = (float)sourcePixel[3] / 255.0f;
            if (alpha <= source->alphaThreshold) return ngDepthSampleMissingPoint();
            auto value = ngDepthSamplePixelDepth(
                sourcePixel,
                0,
                DepthSampleChannel::AverageRGB,
                source->invert,
                source->backDepth,
                source->frontDepth,
                source->sourceDepthScale
            );
            return DepthSamplePoint{true, value * source->depthScale + source->depthOffset, alpha};
        };
        const auto convolution = threeDAdjustSampleConvolution();
        const auto customRadius = layerPreview.customRadius;
        auto sampled = ngDepthSampleConvolve(sampleAt, convolution, customRadius, x, y);
        if (!sampled.valid) return false;
        depth = sampled.value;
        return true;
    }

void PSDDepthMapWindow::prepareThreeDAdjustSamples() {
        const auto keepAlive = weak_from_this().lock();
        if (threeDAdjustSamplesPrepared) return;
        threeDAdjustSamples = {};
        threeDAdjustPreparedTransforms = {};

        for (auto layers = preview.composedLayers; auto& layerPreview : layers) {
            const auto key = compat::RetainedStringSlice(
                layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid));
            threeDAdjustPreparedTransforms[key] = layerTransform(
                layerPreview.layerPath, layerPreview.targetGridUuid);
            compat::Slice<PsdDepth3DAdjustSample> samples;
            const auto height = layerPreview.height;
            for (auto y = 0; y < height; ++y) {
                const auto width = layerPreview.width;
                for (auto x = 0; x < width; ++x) {
                    auto rgbaIndex = ((size_t)y * (size_t)layerPreview.width + (size_t)x) * 4;
                    if (rgbaIndex + 3 >= layerPreview.maskRgba.size() ||
                        rgbaIndex + 3 >= layerPreview.colorRgba.size() ||
                        compat::ngArrayAt(layerPreview.maskRgba, rgbaIndex) == 0 ||
                        compat::ngArrayAt(layerPreview.maskRgba, rgbaIndex + 3) == 0 ||
                        compat::ngArrayAt(layerPreview.colorRgba, rgbaIndex + 3) < 3) continue;
                    float depth = std::numeric_limits<float>::quiet_NaN();
                    if (!threeDAdjustDepthAt(layerPreview, x, y, depth)) continue;
                    PsdDepth3DAdjustSample sample;
                    sample.x = x;
                    sample.y = y;
                    sample.depthByte = compat::ngArrayAt(layerPreview.depthRgba, rgbaIndex);
                    sample.sourceDepth = depth;
                    sample.depth = depth;
                    sample.r = compat::ngArrayAt(layerPreview.colorRgba, rgbaIndex + 0);
                    sample.g = compat::ngArrayAt(layerPreview.colorRgba, rgbaIndex + 1);
                    sample.b = compat::ngArrayAt(layerPreview.colorRgba, rgbaIndex + 2);
                    sample.a = compat::ngArrayAt(layerPreview.colorRgba, rgbaIndex + 3);
                    samples.append(sample);
                }
            }
            threeDAdjustSamples[key] = samples;
        }

        if (preview.compositionMode == PsdDepthCompositionMode::NToOne &&
            preview.depthSource.kind == PsdDepthCompositeSourceKind::FlatImage &&
            preview.compositionWidth > 0 && preview.compositionHeight > 0) {
            compat::Slice<float> upperDepthLimit;
            upperDepthLimit.resize((size_t)preview.compositionWidth * (size_t)preview.compositionHeight,
                std::numeric_limits<float>::quiet_NaN());
            std::fill(upperDepthLimit.begin(), upperDepthLimit.end(), std::numeric_limits<float>::max());

            // Same single document-space upper-depth buffer as depth-draw.
            auto layers = preview.composedLayers;
            for (std::size_t reverseIndex = layers.size(); reverseIndex-- > 0;) {
                auto& layerPreview = compat::ngArrayAt(layers, reverseIndex);
                if (!layerPreview.enabled || !layerPreview.visible) continue;
                const auto key = compat::RetainedStringSlice(
                    layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid));
                auto samples = lookup(threeDAdjustSamples, key);
                if (samples == nullptr) continue;
                auto depthStep = dmax(std::abs((layerPreview.frontDepth - layerPreview.backDepth) *
                    layerPreview.sourceDepthScale * layerPreview.depthScale) / 255.0f, 0.000001f);
                for (auto sampleValues = *samples; auto& sample : sampleValues) {
                    auto documentX = add32(layerPreview.left, sample.x);
                    auto documentY = add32(layerPreview.top, sample.y);
                    if (documentX < 0 || documentY < 0 ||
                        documentX >= preview.compositionWidth || documentY >= preview.compositionHeight) continue;
                    auto documentIndex = (size_t)documentY * (size_t)preview.compositionWidth +
                        (size_t)documentX;
                    if (compat::ngArrayAt(upperDepthLimit, documentIndex) < std::numeric_limits<float>::max()) {
                        sample.depth = dmin(sample.depth, compat::ngArrayAt(upperDepthLimit, documentIndex) - depthStep);
                    }
                    compat::ngArrayAt(upperDepthLimit, documentIndex) =
                        dmin(compat::ngArrayAt(upperDepthLimit, documentIndex), sample.depth);
                }
            }
        }
        threeDAdjustSamplesPrepared = true;
    }

bool PSDDepthMapWindow::preparedThreeDAdjustDepthAt(PsdDepthComposedLayer& layerPreview, int x, int y, float& depth) {
        depth = 0.0f;
        const auto keepAlive = weak_from_this().lock();
        const auto layerStorage = preview.composedLayers;
        prepareThreeDAdjustSamples();
        const auto key = compat::RetainedStringSlice(
            layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid));
        auto samples = lookup(threeDAdjustSamples, key);
        if (samples == nullptr) return false;
        auto wanted = add32(mul32(y, layerPreview.width), x);
        std::ptrdiff_t low = 0;
        using UnsignedIndex = std::make_unsigned_t<std::ptrdiff_t>;
        auto high = std::bit_cast<std::ptrdiff_t>(
            static_cast<UnsignedIndex>((*samples).size()) - UnsignedIndex{1});
        while (low <= high) {
            const auto difference = std::bit_cast<std::ptrdiff_t>(
                static_cast<UnsignedIndex>(high) - static_cast<UnsignedIndex>(low));
            auto middle = std::bit_cast<std::ptrdiff_t>(
                static_cast<UnsignedIndex>(low) + static_cast<UnsignedIndex>(difference / 2));
            auto sample = compat::ngArrayAt(*samples, (size_t)middle);
            auto found = add32(mul32(sample.y, layerPreview.width), sample.x);
            if (found < wanted) low = std::bit_cast<std::ptrdiff_t>(
                static_cast<UnsignedIndex>(middle) + UnsignedIndex{1});
            else if (found > wanted) high = std::bit_cast<std::ptrdiff_t>(
                static_cast<UnsignedIndex>(middle) - UnsignedIndex{1});
            else {
                depth = sample.depth;
                return true;
            }
        }
        return false;
    }

float PSDDepthMapWindow::threeDAdjustDepthDisplayScale() {
        compat::Slice<DeformableRef> targets;
        const auto keepAlive = weak_from_this().lock();
        auto grids = preview.grids;
        for (auto& gridResult : grids) {
            if (gridResult.grid != nullptr && !gridResult.skipped) targets.append(gridResult.grid);
        }
        auto puppet = project::incActivePuppet();
        auto targetScale = ngDepthDisplayScaleForTargetsInNodeSpace(
            puppet == nullptr ? nullptr : puppet->root,
            targets);
        if (targetScale > 0.0f) return targetScale;
        return ngDepthDisplayScaleForDocument(preview.compositionWidth, preview.compositionHeight);
    }

compat::Slice<PsdDepth3DAdjustSample> PSDDepthMapWindow::threeDAdjustLayerSamples(PsdDepthComposedLayer& layerPreview) {
        const auto keepAlive = weak_from_this().lock();
        const auto layerStorage = preview.composedLayers;
        prepareThreeDAdjustSamples();
        const auto key = compat::RetainedStringSlice(
            layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid));
        auto existing = lookup(threeDAdjustSamples, key);
        return existing == nullptr ? compat::Slice<PsdDepth3DAdjustSample>{} : *existing;
    }

PsdDepthLayerEdit PSDDepthMapWindow::threeDAdjustPreparedTransform(PsdDepthComposedLayer& layerPreview) {
        const auto keepAlive = weak_from_this().lock();
        const auto layerStorage = preview.composedLayers;
        prepareThreeDAdjustSamples();
        const auto key = compat::RetainedStringSlice(
            layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid));
        if (auto transform = lookup(threeDAdjustPreparedTransforms, key)) return *transform;
        return layerTransform(layerPreview.layerPath, layerPreview.targetGridUuid);
    }

float PSDDepthMapWindow::threeDAdjustDisplayDepth(PsdDepthComposedLayer& layerPreview, float bakedDepth) {
        const auto keepAlive = weak_from_this().lock();
        const auto layerStorage = preview.composedLayers;
        auto baked = threeDAdjustPreparedTransform(layerPreview);
        auto current = layerTransform(layerPreview.layerPath, layerPreview.targetGridUuid);
        if (std::abs(baked.zScale) > 0.000001f) {
            return (bakedDepth - baked.zOffset) * (current.zScale / baked.zScale) + current.zOffset;
        }
        return bakedDepth + current.zOffset - baked.zOffset;
    }

std::shared_ptr<PsdDepth3DAdjustSample> PSDDepthMapWindow::threeDAdjustSampleAtDocument(
        PsdDepthComposedLayer& layerPreview,
        int documentX,
        int documentY
    ) {
        const auto keepAlive = weak_from_this().lock();
        const auto layers = preview.composedLayers;
        auto x = sub32(documentX, layerPreview.left);
        auto y = sub32(documentY, layerPreview.top);
        if (x < 0 || y < 0 || x >= layerPreview.width || y >= layerPreview.height) return {};
        const auto key = compat::RetainedStringSlice(
            layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid));
        auto samples = lookup(threeDAdjustSamples, key);
        if (samples == nullptr) return {};
        auto wanted = add32(mul32(y, layerPreview.width), x);
        std::ptrdiff_t low = 0;
        using UnsignedIndex = std::make_unsigned_t<std::ptrdiff_t>;
        auto high = std::bit_cast<std::ptrdiff_t>(
            static_cast<UnsignedIndex>((*samples).size()) - UnsignedIndex{1});
        while (low <= high) {
            const auto difference = std::bit_cast<std::ptrdiff_t>(
                static_cast<UnsignedIndex>(high) - static_cast<UnsignedIndex>(low));
            auto middle = std::bit_cast<std::ptrdiff_t>(
                static_cast<UnsignedIndex>(low) + static_cast<UnsignedIndex>(difference / 2));
            const auto row = mul32(compat::ngArrayAt(*samples, (size_t)middle).y, layerPreview.width);
            auto found = add32(row, compat::ngArrayAt(*samples, (size_t)middle).x);
            if (found < wanted) low = std::bit_cast<std::ptrdiff_t>(
                static_cast<UnsignedIndex>(middle) + UnsignedIndex{1});
            else if (found > wanted) high = std::bit_cast<std::ptrdiff_t>(
                static_cast<UnsignedIndex>(middle) - UnsignedIndex{1});
            else return retainedElement(*samples, &compat::ngArrayAt(*samples, (size_t)middle));
        }
        return {};
    }

bool PSDDepthMapWindow::threeDAdjustLayersIntersect(
        PsdDepthComposedLayer& backLayer,
        PsdDepthComposedLayer& frontLayer
    ) {
        const auto keepAlive = weak_from_this().lock();
        const auto layers = preview.composedLayers;
        if (backLayer.left >= add32(frontLayer.left, frontLayer.width) ||
            frontLayer.left >= add32(backLayer.left, backLayer.width) ||
            backLayer.top >= add32(frontLayer.top, frontLayer.height) ||
            frontLayer.top >= add32(backLayer.top, backLayer.height)) return false;

        auto backSamples = threeDAdjustLayerSamples(backLayer);
        auto frontSamples = threeDAdjustLayerSamples(frontLayer);
        if (backSamples.size() <= frontSamples.size()) {
            for (auto& sample : backSamples) {
                const auto documentX = add32(backLayer.left, sample.x);
                const auto documentY = add32(backLayer.top, sample.y);
                if (threeDAdjustSampleAtDocument(frontLayer, documentX, documentY) != nullptr) return true;
            }
        } else {
            for (auto& sample : frontSamples) {
                const auto documentX = add32(frontLayer.left, sample.x);
                const auto documentY = add32(frontLayer.top, sample.y);
                if (threeDAdjustSampleAtDocument(backLayer, documentX, documentY) != nullptr) return true;
            }
        }
        return false;
    }

compat::Slice<size_t> PSDDepthMapWindow::threeDAdjustFrontIntersectingLayerIndices(PsdDepthComposedLayer& layerPreview) {
        const auto keepAlive = weak_from_this().lock();
        const auto layerStorage = preview.composedLayers;
        prepareThreeDAdjustSamples();
        const auto key = compat::RetainedStringSlice(
            layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid));
        if (auto cached = lookup(threeDAdjustFrontIntersections, key)) return *cached;

        compat::Slice<size_t> result;
        std::ptrdiff_t layerIndex = -1;
        auto candidates = preview.composedLayers;
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            auto& candidate = compat::ngArrayAt(candidates, i);
            if (&candidate == &layerPreview) {
                layerIndex = (std::ptrdiff_t)i;
                break;
            }
        }
        if (layerIndex >= 0) {
            // composedLayers is stored back-to-front. Only later layers can
            // constrain this one, and only when their visible pixels overlap.
            const auto endIndex = preview.composedLayers.size();
            for (std::size_t i = (std::size_t)layerIndex + 1; i < endIndex; ++i) {
                const auto frontStorage = preview.composedLayers;
                auto frontLayer = &compat::ngArrayAt(preview.composedLayers, i);
                if (!frontLayer->enabled || !frontLayer->visible) continue;
                if (threeDAdjustLayersIntersect(layerPreview, *frontLayer)) result.append(i);
            }
        }
        threeDAdjustFrontIntersections[key] = result;
        return result;
    }

float PSDDepthMapWindow::threeDAdjustConstrainedDisplayDepth(
        PsdDepthComposedLayer& layerPreview,
        PsdDepth3DAdjustSample& sample
    ) {
        const auto keepAlive = weak_from_this().lock();
        const auto layerStorage = preview.composedLayers;
        auto depth = threeDAdjustDisplayDepth(layerPreview, sample.sourceDepth);
        auto depthStep = dmax(std::abs((layerPreview.frontDepth - layerPreview.backDepth) *
            layerPreview.sourceDepthScale * layerPreview.depthScale) / 255.0f, 0.000001f);
        auto documentX = add32(layerPreview.left, sample.x);
        auto documentY = add32(layerPreview.top, sample.y);
        for (auto frontIndex : threeDAdjustFrontIntersectingLayerIndices(layerPreview)) {
            const auto frontStorage = preview.composedLayers;
            auto frontLayer = &compat::ngArrayAt(preview.composedLayers, frontIndex);
            auto frontSample = threeDAdjustSampleAtDocument(*frontLayer, documentX, documentY);
            if (frontSample == nullptr) continue;
            // Comparing against every overlapping front layer directly is
            // sufficient to preserve ordering and avoids recursive duplicate
            // work when several front layers overlap the same pixel.
            const auto previousDepth = depth;
            const auto frontDepth = threeDAdjustDisplayDepth(*frontLayer, frontSample->sourceDepth);
            depth = dmin(previousDepth, frontDepth - depthStep);
        }
        return depth;
    }

void PSDDepthMapWindow::updateThreeDAdjustMeshDisplayDepths(
        PsdDepthComposedLayer& layerPreview,
        PsdDepth3DAdjustMesh& mesh
    ) {
        const auto keepAlive = weak_from_this().lock();
        const auto layerStorage = preview.composedLayers;
        const auto key = compat::RetainedStringSlice(
            layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid));
        auto dirty = lookup(threeDAdjustDisplayDepthDirty, key);
        if (dirty == nullptr || !*dirty || mesh.sourceDepths.size() * 5 != mesh.vertexData.size()) return;

        auto sourceDepths = mesh.sourceDepths;
        for (std::size_t vertexIndex = 0; vertexIndex < sourceDepths.size(); ++vertexIndex) {
            auto sourceDepth = compat::ngArrayAt(sourceDepths, vertexIndex);
            auto offset = vertexIndex * 5;
            PsdDepth3DAdjustSample sample;
            sample.x = toSourceInt(std::round(compat::ngArrayAt(mesh.vertexData, offset) - (float)layerPreview.left));
            sample.y = toSourceInt(std::round(compat::ngArrayAt(mesh.vertexData, offset + 1) - (float)layerPreview.top));
            sample.sourceDepth = sourceDepth;
            sample.depth = sourceDepth;
            compat::ngArrayAt(mesh.vertexData, offset + 2) = threeDAdjustConstrainedDisplayDepth(layerPreview, sample);
        }
        threeDAdjustDisplayDepthDirty.erase(key);
    }

std::shared_ptr<PsdDepth3DAdjustMesh> PSDDepthMapWindow::threeDAdjustLayerMesh(PsdDepthComposedLayer& layerPreview) {
        const auto keepAlive = weak_from_this().lock();
        const auto layerStorage = preview.composedLayers;
        const auto key = compat::RetainedStringSlice(
            layerCacheKey(layerPreview.layerPath, layerPreview.targetGridUuid));
        auto existing = lookup(threeDAdjustMeshes, key);
        if (existing != nullptr) return existing;

        PsdDepth3DAdjustMesh mesh;
        if (layerPreview.width <= 1 || layerPreview.height <= 1) {
            threeDAdjustMeshes[key] = mesh;
            return lookup(threeDAdjustMeshes, key);
        }

        auto step = dmax(1, PsdDepth3DAdjustMeshStep);
        auto cols = add32(sub32(add32(sub32(layerPreview.width, 1), step), 1) / step, 1);
        auto rows = add32(sub32(add32(sub32(layerPreview.height, 1), step), 1) / step, 1);
        compat::Slice<int> vertexLookup;
        vertexLookup.resize((size_t)cols * (size_t)rows);
        std::fill(vertexLookup.begin(), vertexLookup.end(), -1);

        auto sampleX = [&](int x) -> int {
            return dmin(sub32(layerPreview.width, 1), mul32(x, step));
        };
        auto sampleY = [&](int y) -> int {
            return dmin(sub32(layerPreview.height, 1), mul32(y, step));
        };
        auto vertexAt = [&](int gx, int gy) -> int {
            return compat::ngArrayAt(vertexLookup, (size_t)gy * (size_t)cols + (size_t)gx);
        };

        for (int gy = 0; gy < rows; ++gy) {
            for (int gx = 0; gx < cols; ++gx) {
                auto x = sampleX(gx);
                auto y = sampleY(gy);
                auto index = ((size_t)y * (size_t)layerPreview.width + (size_t)x) * 4;
                if (index + 3 >= layerPreview.maskRgba.size() ||
                    index + 3 >= layerPreview.colorRgba.size()) {
                    continue;
                }
                if (compat::ngArrayAt(layerPreview.colorRgba, index + 3) < 3) continue;
                float depth = std::numeric_limits<float>::quiet_NaN();
                if (!preparedThreeDAdjustDepthAt(layerPreview, x, y, depth)) continue;
                const auto documentX = add32(layerPreview.left, x);
                const auto documentY = add32(layerPreview.top, y);
                auto sourceSample = threeDAdjustSampleAtDocument(layerPreview, documentX, documentY);
                if (sourceSample == nullptr) continue;

                auto vertexIndex = (int)(mesh.vertexData.size() / 5);
                compat::ngArrayAt(vertexLookup, (size_t)gy * (size_t)cols + (size_t)gx) = vertexIndex;
                mesh.vertexData.append((float)layerPreview.left + (float)x);
                mesh.vertexData.append((float)layerPreview.top + (float)y);
                mesh.vertexData.append(depth);
                mesh.sourceDepths.append(sourceSample->sourceDepth);
                mesh.vertexData.append(((float)x + 0.5f) / (float)layerPreview.width);
                mesh.vertexData.append(((float)y + 0.5f) / (float)layerPreview.height);
            }
        }

        if (settings.smoothWavySurface) {
            compat::Slice<float> gridDepths;
            compat::Slice<std::uint8_t> vertexValid;
            compat::Slice<int> vertexGroup;
            gridDepths.resize(vertexLookup.size(), NAN);
            vertexValid.resize(vertexLookup.size());
            vertexGroup.resize(vertexLookup.size());
            std::fill(vertexGroup.begin(), vertexGroup.end(), -1);
            for (std::size_t gridIndex = 0; gridIndex < vertexLookup.size(); ++gridIndex) {
                auto vertexIndex = compat::ngArrayAt(vertexLookup, gridIndex);
                if (vertexIndex < 0) continue;
                compat::ngArrayAt(vertexValid, gridIndex) = 1;
                compat::ngArrayAt(vertexGroup, gridIndex) = 0;
                compat::ngArrayAt(gridDepths, gridIndex) = compat::ngArrayAt(mesh.vertexData, (size_t)vertexIndex * 5 + 2);
            }
            auto depthRange = std::abs((layerPreview.frontDepth - layerPreview.backDepth) *
                layerPreview.sourceDepthScale * layerPreview.depthScale);
            auto smoothed = ngDepthDrawSmoothGridDepthValues(
                gridDepths, vertexValid, vertexGroup, cols, rows, 18.0f, depthRange);
            for (std::size_t gridIndex = 0; gridIndex < vertexLookup.size(); ++gridIndex) {
                auto vertexIndex = compat::ngArrayAt(vertexLookup, gridIndex);
                if (vertexIndex < 0) continue;
                compat::ngArrayAt(mesh.vertexData, (size_t)vertexIndex * 5 + 2) = compat::ngArrayAt(smoothed, gridIndex);
            }
        }

        for (int gy = 0; gy < rows - 1; ++gy) {
            for (int gx = 0; gx < cols - 1; ++gx) {
                auto p0 = vertexAt(gx, gy);
                auto p1 = vertexAt(gx + 1, gy);
                auto p2 = vertexAt(gx, gy + 1);
                auto p3 = vertexAt(gx + 1, gy + 1);
                if (p0 >= 0 && p1 >= 0 && p3 >= 0) {
                    mesh.indices.append(compat::Slice<std::uint32_t>{(std::uint32_t)p0, (std::uint32_t)p1, (std::uint32_t)p3});
                }
                if (p0 >= 0 && p3 >= 0 && p2 >= 0) {
                    mesh.indices.append(compat::Slice<std::uint32_t>{(std::uint32_t)p0, (std::uint32_t)p3, (std::uint32_t)p2});
                }
            }
        }

        threeDAdjustMeshes[key] = mesh;
        return lookup(threeDAdjustMeshes, key);
    }


}
