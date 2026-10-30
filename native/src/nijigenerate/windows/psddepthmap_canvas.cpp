#include "psddepthmap.hpp"
#include "detail/psddepthmap_gpu.hpp"
#include "detail/psddepthmap_helpers.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/core/texture_pixels.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <imgui_internal.h>
#include <cmath>
#include <limits>
#include <utility>
namespace nijigenerate::windows::psddepthmap {
using namespace detail::helpers;
namespace {
int toInt(float value) {
    return toSourceInt(value);
}
}
void PSDDepthMapWindow::draw3DAdjustRelationshipCanvas(float height) {
    const auto keepAlive = weak_from_this().lock();
    auto space = widgets::ngAvailableSpace();
    auto canvasHeight = dmax(260.0f, height - 8.0f);
    auto canvasSize = ImVec2(dmax(320.0f, space.x), canvasHeight);
    ImVec2 origin;
    origin = ImGui::GetCursorScreenPos();
    auto drawList = ImGui::GetWindowDrawList();
    auto bg = ImGui::GetColorU32(ImVec4(0.08f, 0.09f, 0.10f, 1.0f));
    auto border = ImGui::GetColorU32(ImVec4(0.45f, 0.48f, 0.50f, 1.0f));
    auto textureTint = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    auto canvasMax = ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y);
    drawList->AddRectFilled( origin, canvasMax, bg, 4.0f);
    drawList->AddRect( origin, canvasMax, border, 4.0f, ImDrawFlags_None, 1.0f);
    ImGui::InvisibleButton("###psdDepth3DAdjustViewport", canvasSize);
    auto& io = ImGui::GetIO();
    if (ImGui::IsItemHovered()) {
        auto previousYaw = threeDAdjustCamera.yaw;
        auto previousPitch = threeDAdjustCamera.pitch;
        auto previousZoom = threeDAdjustCamera.zoom;
        auto previousPan = threeDAdjustCamera.pan;
        auto usesWheel = io.MouseWheel != 0;
        viewport::depth::ngUpdateDepthCamera3D(
            threeDAdjustCamera,
            &io,
            io.MouseDown[1] && !io.KeyShift,
            (io.MouseDown[1] && io.KeyShift) || io.MouseDown[2],
            usesWheel
        );
        if (previousYaw != threeDAdjustCamera.yaw ||
            previousPitch != threeDAdjustCamera.pitch ||
            previousZoom != threeDAdjustCamera.zoom ||
            previousPan.x != threeDAdjustCamera.pan.x ||
            previousPan.y != threeDAdjustCamera.pan.y) {
            threeDAdjustPreviewDirty = true;
        }
        if (usesWheel) {
            ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX, 0);
            ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY, 0);
        }
    }

    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    bool hasBounds = false;
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
    auto hasRenderPixels = [&](PsdDepthComposedLayer& layerPreview) -> bool {
        return threeDAdjustLayerSamples(layerPreview).size() > 0;
    };
    for (auto layerPreviews = preview.composedLayers; auto& layerPreview : layerPreviews) {
        if (!hasRenderPixels(layerPreview)) continue;
        includeBounds(
            layerPreview.left,
            layerPreview.top,
            add32(layerPreview.left, layerPreview.width),
            add32(layerPreview.top, layerPreview.height)
        );
    }
    if (!hasBounds) includeBounds(0, 0, dmax(1, preview.compositionWidth), dmax(1, preview.compositionHeight));
    if (right <= left) right = add32(left, 1);
    if (bottom <= top) bottom = add32(top, 1);
    auto centerX = (static_cast<float>(left) + static_cast<float>(right)) * 0.5f;
    auto centerY = (static_cast<float>(top) + static_cast<float>(bottom)) * 0.5f;
    auto sourceW = static_cast<float>((sub32(right, left)));
    auto sourceH = static_cast<float>((sub32(bottom, top)));
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
        resetPsdDepth3DAdjustCameraToBounds(sourceW, sourceH, canvasSize);
        threeDAdjustPreviewDirty = true;
    }
    auto depthDisplayScale = threeDAdjustDepthDisplayScale();

    auto framebufferWidth = dmax(1, toInt(canvasSize.x));
    auto framebufferHeight = dmax(1, toInt(canvasSize.y));
    if (threeDAdjustGpuRenderer == nullptr) threeDAdjustGpuRenderer = allocatePsdDepthObject<detail::PsdDepth3DAdjustGpuRenderer>();
    const auto renderReceiver = threeDAdjustGpuRenderer;
    auto gpuTexture = renderReceiver->render(
        framebufferWidth,
        framebufferHeight,
        [&] {
            for (auto layerPreviews = preview.composedLayers; auto& layerPreview : layerPreviews) {
                if (!hasRenderPixels(layerPreview)) continue;
                auto mesh = threeDAdjustLayerMesh(layerPreview);
                if (mesh == nullptr || mesh->indices.size() == 0) continue;
                updateThreeDAdjustMeshDisplayDepths(layerPreview, *mesh);
                auto layerTexture = layerPreviewTexture(
                    layerPreview.layerPath,
                    layerPreview.targetGridUuid,
                    false);
                const auto renderer = threeDAdjustGpuRenderer;
                auto drawTexture = layerTexture;
                auto& drawMesh = *mesh;
                const auto drawCenterX = centerX;
                const auto drawCenterY = centerY;
                const auto yaw = threeDAdjustCamera.yaw;
                const auto pitch = threeDAdjustCamera.pitch;
                const auto zoom = threeDAdjustCamera.zoom;
                const auto pan = threeDAdjustCamera.pan;
                const auto drawDepthScale = depthDisplayScale;
                auto firstPath = layerPreview.layerPath;
                const auto firstGrid = layerPreview.targetGridUuid;
                const auto firstTransform = layerTransform(std::move(firstPath), firstGrid);
                auto secondPath = layerPreview.layerPath;
                const auto secondGrid = layerPreview.targetGridUuid;
                const auto secondTransform = layerTransform(std::move(secondPath), secondGrid);
                renderer->drawLayer(std::move(drawTexture), drawMesh, drawCenterX, drawCenterY,
                    yaw, pitch, zoom, pan, drawDepthScale, firstTransform, secondTransform);
            }
        }
    );
    if (gpuTexture != nullptr) {
        drawList->AddImage(
            static_cast<ImTextureID>(compat::ngRequireObject(gpuTexture)->getTextureId()),
            origin,
            canvasMax,
            ImVec2(0, 1),
            ImVec2(1, 0),
            textureTint
        );
        drawList->AddRect( origin, canvasMax, border, 4.0f, ImDrawFlags_None, 1.0f);
        return;
    }
    if (threeDAdjustPreviewTexture != nullptr &&
        threeDAdjustPreviewWidth == framebufferWidth &&
        threeDAdjustPreviewHeight == framebufferHeight &&
        !threeDAdjustPreviewDirty &&
        !threeDAdjustCameraChanged()) {
        drawList->AddImage(
            static_cast<ImTextureID>(compat::ngRequireObject(threeDAdjustPreviewTexture)->getTextureId()),
            origin,
            canvasMax,
            ImVec2(0, 0),
            ImVec2(1, 1),
            textureTint
        );
        drawList->AddRect( origin, canvasMax, border, 4.0f, ImDrawFlags_None, 1.0f);
        return;
    }
    threeDAdjustPreviewDirty = false;
    rememberThreeDAdjustCamera();
    compat::Slice<std::uint8_t> framebuffer;
    compat::Slice<float> zBuffer;
    framebuffer.resize(static_cast<std::size_t>(framebufferWidth) * static_cast<std::size_t>(framebufferHeight) * 4);
    zBuffer.resize(static_cast<std::size_t>(framebufferWidth) * static_cast<std::size_t>(framebufferHeight),
        std::numeric_limits<float>::quiet_NaN());
    const auto pixelCount = static_cast<std::size_t>(framebufferWidth) * static_cast<std::size_t>(framebufferHeight);
    for (std::size_t i = 0; i < pixelCount; ++i) {
        auto pixelIndex = i * 4;
        compat::ngArrayAt(framebuffer, pixelIndex + 0) = 20;
        compat::ngArrayAt(framebuffer, pixelIndex + 1) = 23;
        compat::ngArrayAt(framebuffer, pixelIndex + 2) = 26;
        compat::ngArrayAt(framebuffer, pixelIndex + 3) = 255;
        compat::ngArrayAt(zBuffer, i) = (std::numeric_limits<float>::max)();
    }
    std::size_t renderedPixels = 0;


    float cameraYawCos = std::cos(threeDAdjustCamera.yaw);
    float cameraYawSin = std::sin(threeDAdjustCamera.yaw);
    float cameraPitchCos = std::cos(threeDAdjustCamera.pitch);
    float cameraPitchSin = std::sin(threeDAdjustCamera.pitch);
    float cameraZoom = threeDAdjustCamera.zoom;
    float cameraPanX = threeDAdjustCamera.pan.x;
    float cameraPanY = threeDAdjustCamera.pan.y;

    auto cameraDepthForPoint = [&](Vec2 point, float depth) -> float {
        float x = point.x;
        float y = point.y;
        float z = depth;

        float rz = -x * cameraYawSin + z * cameraYawCos;
        return y * cameraPitchSin + rz * cameraPitchCos;
    };

    auto projectDocumentPoint = [&](Vec2 documentPoint, float depth, float sceneDepthDisplayScale) -> ImVec2 {
        float x = documentPoint.x - centerX;
        float y = documentPoint.y - centerY;
        float z = -depth * sceneDepthDisplayScale;

        float rx = x * cameraYawCos + z * cameraYawSin;
        float rz = -x * cameraYawSin + z * cameraYawCos;
        float ry = y * cameraPitchCos - rz * cameraPitchSin;
        return ImVec2(
            origin.x + canvasSize.x * 0.5f + rx * cameraZoom + cameraPanX,
            origin.y + canvasSize.y * 0.5f + ry * cameraZoom + cameraPanY
        );
    };

    auto edgeFunction = [&](float ax, float ay, float bx, float by, float cx, float cy) -> float {
        return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
    };

    auto sampleCoverage = [&](PsdDepthComposedLayer& layerPreview, float u, float v) -> std::uint8_t {
        auto textureWidth = layerPreview.width;
        auto textureHeight = layerPreview.height;
        if (textureWidth <= 0 || textureHeight <= 0) return 0;
        auto x = dclamp(toInt((u * static_cast<float>(textureWidth))), 0, textureWidth - 1);
        auto y = dclamp(toInt((v * static_cast<float>(textureHeight))), 0, textureHeight - 1);
        auto index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(textureWidth) + static_cast<std::size_t>(x)) * 4;
        if (index + 3 >= layerPreview.colorRgba.size()) return 0;
        if (index + 3 >= layerPreview.maskRgba.size()) return 0;
        if (compat::ngArrayAt(layerPreview.maskRgba, index) == 0 ||
            compat::ngArrayAt(layerPreview.maskRgba, index + 3) == 0) return 0;
        return compat::ngArrayAt(layerPreview.colorRgba, index + 3);
    };

    auto sampleColor = [&](PsdDepthComposedLayer& layerPreview, float u, float v, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b, std::uint8_t& a) -> bool {
        r = g = b = a = 0;
        auto textureWidth = layerPreview.width;
        auto textureHeight = layerPreview.height;
        if (textureWidth <= 0 || textureHeight <= 0) return false;
        auto x = dclamp(toInt((u * static_cast<float>(textureWidth))), 0, textureWidth - 1);
        auto y = dclamp(toInt((v * static_cast<float>(textureHeight))), 0, textureHeight - 1);
        auto index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(textureWidth) + static_cast<std::size_t>(x)) * 4;
        if (index + 3 >= layerPreview.colorRgba.size()) return false;
        if (index + 3 >= layerPreview.maskRgba.size()) return false;
        if (compat::ngArrayAt(layerPreview.maskRgba, index) == 0 ||
            compat::ngArrayAt(layerPreview.maskRgba, index + 3) == 0) return false;
        r = compat::ngArrayAt(layerPreview.colorRgba, index + 0);
        g = compat::ngArrayAt(layerPreview.colorRgba, index + 1);
        b = compat::ngArrayAt(layerPreview.colorRgba, index + 2);
        a = compat::ngArrayAt(layerPreview.colorRgba, index + 3);
        return true;
    };

    auto rasterizeImageTriangle = [&](
        PsdDepthComposedLayer& layerPreview,
        ImVec2 p0,
        ImVec2 p1,
        ImVec2 p2,
        ImVec2 uv0,
        ImVec2 uv1,
        ImVec2 uv2,
        float z0,
        float z1,
        float z2
    ) -> void {

        auto x0 = p0.x - origin.x;
        auto y0 = p0.y - origin.y;
        auto x1 = p1.x - origin.x;
        auto y1 = p1.y - origin.y;
        auto x2 = p2.x - origin.x;
        auto y2 = p2.y - origin.y;
        auto area = edgeFunction(x0, y0, x1, y1, x2, y2);
        if (area == 0.0f) return;

        auto minX = dclamp(toInt(std::floor(dmin(dmin(x0, x1), x2))), 0, framebufferWidth - 1);
        auto maxX = dclamp(toInt(std::ceil(dmax(dmax(x0, x1), x2))), 0, framebufferWidth - 1);
        auto minY = dclamp(toInt(std::floor(dmin(dmin(y0, y1), y2))), 0, framebufferHeight - 1);
        auto maxY = dclamp(toInt(std::ceil(dmax(dmax(y0, y1), y2))), 0, framebufferHeight - 1);

        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                auto px = static_cast<float>(x) + 0.5f;
                auto py = static_cast<float>(y) + 0.5f;
                auto w0 = edgeFunction(x1, y1, x2, y2, px, py);
                auto w1 = edgeFunction(x2, y2, x0, y0, px, py);
                auto w2 = edgeFunction(x0, y0, x1, y1, px, py);
                if (area > 0.0f) {
                    if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
                } else {
                    if (w0 > 0.0f || w1 > 0.0f || w2 > 0.0f) continue;
                }
                w0 /= area;
                w1 /= area;
                w2 /= area;
                auto z = z0 * w0 + z1 * w1 + z2 * w2;
                auto zIndex = static_cast<std::size_t>(y) * static_cast<std::size_t>(framebufferWidth) + static_cast<std::size_t>(x);
                if (z >= compat::ngArrayAt(zBuffer, zIndex)) continue;

                auto u = uv0.x * w0 + uv1.x * w1 + uv2.x * w2;
                auto v = uv0.y * w0 + uv1.y * w1 + uv2.y * w2;
                if (sampleCoverage(layerPreview, u, v) < 128) continue;

                std::uint8_t sr;
                std::uint8_t sg;
                std::uint8_t sb;
                std::uint8_t sa;
                if (!sampleColor(layerPreview, u, v, sr, sg, sb, sa) || sa < 3) continue;

                auto pixelIndex = zIndex * 4;
                auto alpha = static_cast<float>(sa) / 255.0f;
                compat::ngArrayAt(framebuffer, pixelIndex + 0) = static_cast<std::uint8_t>(dclamp(toInt((
                    static_cast<float>(sr) * alpha + static_cast<float>(compat::ngArrayAt(framebuffer, pixelIndex + 0)) *
                    (1.0f - alpha) + 0.5f)), 0, 255));
                compat::ngArrayAt(framebuffer, pixelIndex + 1) = static_cast<std::uint8_t>(dclamp(toInt((
                    static_cast<float>(sg) * alpha + static_cast<float>(compat::ngArrayAt(framebuffer, pixelIndex + 1)) *
                    (1.0f - alpha) + 0.5f)), 0, 255));
                compat::ngArrayAt(framebuffer, pixelIndex + 2) = static_cast<std::uint8_t>(dclamp(toInt((
                    static_cast<float>(sb) * alpha + static_cast<float>(compat::ngArrayAt(framebuffer, pixelIndex + 2)) *
                    (1.0f - alpha) + 0.5f)), 0, 255));
                compat::ngArrayAt(framebuffer, pixelIndex + 3) = 255;
                compat::ngArrayAt(zBuffer, zIndex) = z;
            }
        }
    };

    for (auto layerPreviews = preview.composedLayers; auto& layerPreview : layerPreviews) {
        if (!hasRenderPixels(layerPreview)) continue;
        if (layerPreview.width <= 1 || layerPreview.height <= 1) continue;
        auto samples = threeDAdjustLayerSamples(layerPreview);
        if (samples.size() == 0) continue;
        auto step = 1;
        auto rasterizeDepthSample = [&](PsdDepth3DAdjustSample sample, float depth) -> void {
            auto documentPoint = Vec2(
                static_cast<float>(layerPreview.left) + static_cast<float>(sample.x),
                static_cast<float>(layerPreview.top) + static_cast<float>(sample.y)
            );
            auto projected = projectDocumentPoint(documentPoint, depth, depthDisplayScale);
            auto sx = toInt(std::round(projected.x - origin.x));
            auto sy = toInt(std::round(projected.y - origin.y));
            if (sx < -1 || sy < -1 || sx > framebufferWidth || sy > framebufferHeight) return;

            auto cameraPoint = Vec2(documentPoint.x - centerX, documentPoint.y - centerY);
            auto z = cameraDepthForPoint(cameraPoint, -depth * depthDisplayScale);
            auto radius = dmax(1, step / 2);
            for (int dy = -radius; dy <= radius; dy++) {
                auto y = add32(sy, dy);
                if (y < 0 || y >= framebufferHeight) continue;
                for (int dx = -radius; dx <= radius; dx++) {
                    auto x = add32(sx, dx);
                    if (x < 0 || x >= framebufferWidth) continue;
                    auto zIndex = static_cast<std::size_t>(y) * static_cast<std::size_t>(framebufferWidth) + static_cast<std::size_t>(x);
                    if (z >= compat::ngArrayAt(zBuffer, zIndex)) continue;

                    auto alpha = static_cast<float>(sample.a) / 255.0f;
                    auto pixelIndex = zIndex * 4;
                    compat::ngArrayAt(framebuffer, pixelIndex + 0) = static_cast<std::uint8_t>(dclamp(toInt((
                        static_cast<float>(sample.r) * alpha +
                        static_cast<float>(compat::ngArrayAt(framebuffer, pixelIndex + 0)) * (1.0f - alpha) +
                        0.5f)), 0, 255));
                    compat::ngArrayAt(framebuffer, pixelIndex + 1) = static_cast<std::uint8_t>(dclamp(toInt((
                        static_cast<float>(sample.g) * alpha +
                        static_cast<float>(compat::ngArrayAt(framebuffer, pixelIndex + 1)) * (1.0f - alpha) +
                        0.5f)), 0, 255));
                    compat::ngArrayAt(framebuffer, pixelIndex + 2) = static_cast<std::uint8_t>(dclamp(toInt((
                        static_cast<float>(sample.b) * alpha +
                        static_cast<float>(compat::ngArrayAt(framebuffer, pixelIndex + 2)) * (1.0f - alpha) +
                        0.5f)), 0, 255));
                    compat::ngArrayAt(framebuffer, pixelIndex + 3) = 255;
                    compat::ngArrayAt(zBuffer, zIndex) = z;
                    renderedPixels++;
                }
            }
        };

        for (auto sample : samples) {
            const auto rasterSample = sample;
            const auto depth = threeDAdjustConstrainedDisplayDepth(layerPreview, sample);
            rasterizeDepthSample(rasterSample, depth);
        }
    }

    threeDAdjustPreviewCaptureRgba = framebuffer.duplicate();
    threeDAdjustPreviewCaptureWidth = framebufferWidth;
    threeDAdjustPreviewCaptureHeight = framebufferHeight;
    auto textureData = nicxlive::core::SourceTextureBytes::retainedView(
        framebuffer.retentionOwner(), framebuffer.data(), framebuffer.size());
    core::texture_pixels::ngTexPremultiply(textureData);
    if (threeDAdjustPreviewTexture == nullptr ||
        threeDAdjustPreviewWidth != framebufferWidth ||
        threeDAdjustPreviewHeight != framebufferHeight) {
        if (threeDAdjustPreviewTexture != nullptr) threeDAdjustPreviewTexture->dispose();
        threeDAdjustPreviewTexture = nicxlive::core::Texture::createSource(
            textureData, framebufferWidth, framebufferHeight, 4, 4, false, false);
        threeDAdjustPreviewWidth = framebufferWidth;
        threeDAdjustPreviewHeight = framebufferHeight;
    } else {
        threeDAdjustPreviewTexture->setData(textureData, 4);
    }
    drawList->AddImage(
        static_cast<ImTextureID>(compat::ngRequireObject(threeDAdjustPreviewTexture)->getTextureId()),
        origin,
        canvasMax,
        ImVec2(0, 0),
        ImVec2(1, 1),
        textureTint
    );
    drawList->AddRect( origin, canvasMax, border, 4.0f, ImDrawFlags_None, 1.0f);
}

void PSDDepthMapWindow::draw3DAdjustTargetPreview(float height) {
    const auto keepAlive = weak_from_this().lock();
    auto available = widgets::ngAvailableSpace();
    auto controlWidth = dmin(360.0f, dmax(300.0f, available.x * 0.34f));
    if (ImGui::BeginTable("###PsdDepth3DRelationshipLayout", 2,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp, ImVec2(0, height))) {
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("3D View").data(),
            ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(core::translation::ngTranslateCString("Adjust").data(),
            ImGuiTableColumnFlags_WidthFixed, controlWidth);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        draw3DAdjustRelationshipCanvas(height);
        ImGui::TableNextColumn();
        draw3DAdjustLayerControlsPanel(height);
        ImGui::EndTable();
    }
}

void PSDDepthMapWindow::draw3DAdjustTab(float height) {
    const auto keepAlive = weak_from_this().lock();
    auto lines = diagnostics();
    if (lines.size() && (errorMessage.size() || preview.grids.size() == 0)) {
        drawDiagnostics();
        return;
    }
    if (preview.composedLayers.size() == 0) {
        widgets::ngText(compat::RetainedStringSlice(core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("No composed source layers were loaded."))));
        return;
    }
    if (lines.size()) {
        drawDiagnostics();
        ImGui::Separator();
    }
    if (ImGui::BeginChild("###PsdDepth3DAdjustContent", ImVec2(0, height), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        draw3DAdjustTargetPreview(widgets::ngAvailableSpace().y);
    }
    ImGui::EndChild();
}

}
