#include "viewport.hpp"
#include "coordinate.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/viewport.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nodes/deformable.hpp>
#include <bit>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::viewport::depth::draw {
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Vec4;
using nicxlive::core::math::Vec3Array;
using CameraRef = std::shared_ptr<nicxlive::core::math::Camera>;
using TextureRef = std::shared_ptr<nicxlive::core::Texture>;
using namespace core::dbg;
namespace {
std::string_view stringView(DepthDrawString value) {
    return value.view();
}
DepthDrawString gapLabel(bool overlap, DepthDrawString backLayerId, DepthDrawString frontLayerId) {
    try {
        return DepthDrawString(std::string(overlap ? "Overlap: " : "Gap: ") +
            std::string(backLayerId.view()) + (overlap ? " / " : " -> ") +
            std::string(frontLayerId.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class M> struct MapValueReference {
    typename M::iterator entry;
    decltype(&std::declval<typename M::iterator>()->second) value = nullptr;
    explicit operator bool() const { return value != nullptr; }
    auto& operator*() const { return *value; }
};
template<class M, class K> MapValueReference<M> lookup(M& map, const K& key) {
    auto found = map.find(key);
    if (found == map.end()) return {found, nullptr};
    // Dereferencing retains the provider's actual selected Entry. Keep that
    // iterator with its direct value-cell address across reentrant callbacks.
    auto* value = &found->second;
    return {std::move(found), value};
}
std::int32_t add32(std::int32_t a, std::int32_t b) {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a) + std::bit_cast<std::uint32_t>(b));
}
}
void DepthDrawViewport::syncTargets() {
    const auto receiver = weak_from_this().lock();
    compat::ngRequireObject(viewSession)->clear();
    const auto currentSelection = selection;
    for (auto node : currentSelection) {
        auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node);
        if (target == nullptr || std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(target) == nullptr) continue;
        compat::ngRequireObject(viewSession)->ensureTarget(target);
    }
    if (drawSession != nullptr && compat::ngRequireObject(viewSession)->targetByGrid(
            compat::ngRequireObject(drawSession)->selectedGridUuid) != nullptr) {
        compat::ngRequireObject(viewSession)->selectTarget(compat::ngRequireObject(drawSession)->selectedGridUuid);
    }
}

float DepthDrawViewport::selectedDepthDisplayScale() {
    const auto receiver = weak_from_this().lock();
    auto target = compat::ngRequireObject(viewSession)->selectedTarget();
    return target == nullptr ? 1.0f : target->depthDisplayScale();
}

const DepthDrawComposeResult* DepthDrawViewport::selectedPreviewResult(DepthDrawComposeResult& result) {
    if (drawSession == nullptr) return nullptr;
    if (auto selected = lookup(previewResults, compat::ngRequireObject(drawSession)->selectedGridUuid)) {
        result = *selected;
        return &result;
    }
    for (const auto& [gridUuid, preview] : previewResults) {
        result = preview;
        return &result;
    }
    return nullptr;
}

Vec2 DepthDrawViewport::rectMin(DepthDrawRect rect) {
    return Vec2(static_cast<float>(rect.left), static_cast<float>(rect.top));
}

Vec2 DepthDrawViewport::rectMax(DepthDrawRect rect) {
    return Vec2(static_cast<float>(add32(rect.left, rect.width)), static_cast<float>(add32(rect.top, rect.height)));
}

Vec2 DepthDrawViewport::documentToModelPoint(Vec2 point) {
    return Vec2(
        point.x - static_cast<float>(documentWidth) * 0.5f,
        point.y - static_cast<float>(documentHeight) * 0.5f
    );
}

void DepthDrawViewport::planeDocumentBounds(
    DepthDrawDepthSpaceLayerPlane plane,
    Vec2& minPoint,
    Vec2& maxPoint
) {
    const auto receiver = weak_from_this().lock();
    minPoint = {NAN, NAN};
    maxPoint = {NAN, NAN};
    auto layer = drawSession == nullptr ? nullptr : compat::ngRequireObject(drawSession)->layerById(plane.layerId);
    if (layer != nullptr) {
        ngDepthDrawLayerDocumentBounds(*layer, minPoint, maxPoint);
        return;
    }
    minPoint = rectMin(plane.bounds);
    maxPoint = rectMax(plane.bounds);
}

bool DepthDrawViewport::findDepthSpaceLayer(DepthDrawDepthSpaceSummary& depthSpace, DepthDrawString layerId, DepthDrawDepthSpaceLayerPlane& plane) {
    plane = {};
    for (auto candidate : depthSpace.layers) {
        if (stringView(candidate.layerId) != stringView(layerId)) continue;
        plane = candidate;
        return true;
    }
    return false;
}

Vec2 DepthDrawViewport::gapMarkerPoint(DepthDrawDepthSpaceLayerPlane backPlane, DepthDrawDepthSpaceLayerPlane frontPlane) {
    Vec2 backMin{NAN, NAN};
    Vec2 backMax{NAN, NAN};
    Vec2 frontMin{NAN, NAN};
    Vec2 frontMax{NAN, NAN};
    planeDocumentBounds(backPlane, backMin, backMax);
    planeDocumentBounds(frontPlane, frontMin, frontMax);

    auto minX = backMin.x > frontMin.x ? backMin.x : frontMin.x;
    auto maxX = backMax.x < frontMax.x ? backMax.x : frontMax.x;
    auto minY = backMin.y > frontMin.y ? backMin.y : frontMin.y;
    auto maxY = backMax.y < frontMax.y ? backMax.y : frontMax.y;
    if (minX > maxX) {
        minX = (backMin.x + frontMin.x) * 0.5f;
        maxX = (backMax.x + frontMax.x) * 0.5f;
    }
    if (minY > maxY) {
        minY = (backMin.y + frontMin.y) * 0.5f;
        maxY = (backMax.y + frontMax.y) * 0.5f;
    }
    return Vec2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
}

void DepthDrawViewport::addWinningPoint(DepthDrawViewportRenderGeometry& geometry, DepthDrawString layerId, Vec4 color, DepthTargetRenderPoint point) {
    geometry.winningPoints.append(point);
    for (auto& group : geometry.winningPointGroups) {
        if (stringView(group.layerId) != stringView(layerId)) continue;
        group.points.append(point);
        return;
    }
    DepthDrawViewportWinningPointGroup group;
    group.layerId = layerId;
    group.color = color;
    group.points.append(point);
    geometry.winningPointGroups.append(group);
}

void DepthDrawViewport::addCoveragePoints(
    DepthDrawViewportRenderGeometry& geometry,
    const DepthDrawLayer& layer,
    float representativeDepth,
    float depthDisplayScale
) {
    if (!layer.hasNormalCoverage() || layer.width <= 0 || layer.height <= 0) return;
    auto stepX = static_cast<float>(layer.bounds.width) / static_cast<float>(layer.width);
    auto stepY = static_cast<float>(layer.bounds.height) / static_cast<float>(layer.height);
    auto scaleX = layer.xyScale.x == 0.0f ? 1.0f : layer.xyScale.x;
    auto scaleY = layer.xyScale.y == 0.0f ? 1.0f : layer.xyScale.y;
    auto pixelCount = static_cast<std::size_t>(layer.width) * static_cast<std::size_t>(layer.height);
    auto stride = ngDepthDrawCoverageOverlayStride(pixelCount);
    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex += stride) {
        auto rgbaIndex = pixelIndex * 4 + 3;
        if (rgbaIndex >= layer.normalCoverage.size()) continue;
        auto alphaByte = layer.normalCoverage[rgbaIndex];
        if (alphaByte == 0) continue;
        auto x = static_cast<int>(pixelIndex % static_cast<std::size_t>(layer.width));
        auto y = static_cast<int>(pixelIndex / static_cast<std::size_t>(layer.width));
        auto documentPoint = Vec2(
            static_cast<float>(layer.bounds.left) + layer.xyOffset.x +
                (static_cast<float>(x) + 0.5f) * stepX * scaleX,
            static_cast<float>(layer.bounds.top) + layer.xyOffset.y +
                (static_cast<float>(y) + 0.5f) * stepY * scaleY
        );
        DepthDrawViewportCoveragePoint point;
        point.layerId = layer.id;
        point.documentPoint = documentPoint;
        point.alpha = static_cast<float>(alphaByte) / 255.0f;
        const auto pointRenderer = compat::ngRequireObject(renderer);
        const auto modelPoint = documentToModelPoint(documentPoint);
        const auto camera = compat::ngRequireObject(viewSession)->camera;
        point.renderPoint = pointRenderer->buildPoint(
            modelPoint, representativeDepth, depthDisplayScale, camera, 3.0f);
        geometry.coveragePoints.append(point);
    }
}

DepthDrawViewportRenderGeometry DepthDrawViewport::collectRenderGeometry() {
    const auto receiver = weak_from_this().lock();
    DepthDrawViewportRenderGeometry geometry;
    if (drawSession == nullptr) return geometry;

    const auto targets = compat::ngRequireObject(viewSession)->targets;
    for (auto target : targets) {
        if (target == nullptr) continue;
        auto targetNode = target->getTarget();
        if (targetNode == nullptr) continue;
        if (compat::ngRequireObject(drawSession)->display.showComposite) {
            auto targetMesh = compat::ngRequireObject(renderer)->buildMesh(target,
                compat::ngRequireObject(viewSession)->camera);
            geometry.targetMeshes.append(targetMesh);
            geometry.targetMeshGridUuids.append(targetNode->getUUID());
            const auto lineRenderer = compat::ngRequireObject(renderer);
            const auto gridVertices = target->getVertices();
            geometry.targetLines.append(lineRenderer->buildGridLines(gridVertices, targetMesh.positions));
        }

        auto result = lookup(previewResults, targetNode->getUUID());
        if (!result) continue;

        auto summary = ngDepthDrawCompositePreviewSummary(*result);
        auto vertices = target->getVertices();
        for (auto point : summary.points) {
            if (!compat::ngRequireObject(drawSession)->display.showComposite) continue;
            if (point.vertexIndex >= vertices.size()) continue;
            const auto pointRenderer = compat::ngRequireObject(renderer);
            const auto vertex = vertices[point.vertexIndex];
            const auto pointDepth = point.depth;
            const auto pointScale = target->depthDisplayScale();
            const auto camera = compat::ngRequireObject(viewSession)->camera;
            auto renderPoint = pointRenderer->buildPoint(
                vertex, pointDepth, pointScale, camera, point.missing ? 6.0f : 5.0f);
            if (point.missing) {
                if (compat::ngRequireObject(drawSession)->display.showMissingVertices)
                    geometry.missingPoints.append(renderPoint);
            } else if (compat::ngRequireObject(drawSession)->display.showWinningLayer) {
                addWinningPoint(geometry, point.winningLayerId, point.winningLayerColor, renderPoint);
            }
        }
    }

    DepthDrawComposeResult preview;
    auto previewPointer = selectedPreviewResult(preview);
    auto depthSpace = ngDepthDrawDepthSpaceSummary(drawSession, previewPointer);
    auto depthDisplayScale = selectedDepthDisplayScale();
    for (auto plane : depthSpace.layers) {
        if (!plane.visible || !plane.enabled) continue;
        Vec2 documentMin{NAN, NAN};
        Vec2 documentMax{NAN, NAN};
        planeDocumentBounds(plane, documentMin, documentMax);
        auto minPoint = documentToModelPoint(documentMin);
        auto maxPoint = documentToModelPoint(documentMax);
        auto sourcePlaneLines = compat::ngRequireObject(renderer)->buildPlaneLines(
            minPoint,
            maxPoint,
            plane.representativeDepth,
            depthDisplayScale,
            compat::ngRequireObject(viewSession)->camera
        );
        auto sourceLayer = compat::ngRequireObject(drawSession)->layerById(plane.layerId);
        if (sourceLayer != nullptr) {
            if (compat::ngRequireObject(drawSession)->display.showNormalImage && sourceLayer->rgba.size() > 0) {
                geometry.normalImageLines.append(sourcePlaneLines);
            }
            if (compat::ngRequireObject(drawSession)->display.showRawDepth && sourceLayer->hasDepthPixels()) {
                geometry.rawDepthLines.append(sourcePlaneLines);
            }
            if (compat::ngRequireObject(drawSession)->display.showCoverage && sourceLayer->hasNormalCoverage()) {
                geometry.coverageLines.append(sourcePlaneLines);
                addCoveragePoints(geometry, *sourceLayer, plane.representativeDepth, depthDisplayScale);
            }
        }
        if (compat::ngRequireObject(drawSession)->display.showLayerPlanes) {
            geometry.layerPlaneLines.append(sourcePlaneLines);
            if (plane.selected) {
                geometry.selectedLayerLines.append(sourcePlaneLines);
            }
        }
        if (compat::ngRequireObject(drawSession)->display.showDepthRanges && plane.hasDepthRange) {
            geometry.depthRangeLines.append(compat::ngRequireObject(renderer)->buildRangeLines(
                minPoint,
                maxPoint,
                plane.minDepth,
                plane.maxDepth,
                depthDisplayScale,
                compat::ngRequireObject(viewSession)->camera
            ));
        }
    }
    if (compat::ngRequireObject(drawSession)->display.showDepthRanges) {
        for (auto gap : depthSpace.gaps) {
            if (!gap.valid) continue;
            DepthDrawDepthSpaceLayerPlane backPlane;
            DepthDrawDepthSpaceLayerPlane frontPlane;
            if (!findDepthSpaceLayer(depthSpace, gap.backLayerId, backPlane) ||
                !findDepthSpaceLayer(depthSpace, gap.frontLayerId, frontPlane)) {
                continue;
            }
            auto documentPoint = gapMarkerPoint(backPlane, frontPlane);
            auto point = documentToModelPoint(documentPoint);
            auto line = compat::ngRequireObject(renderer)->buildDepthLine(
                point,
                gap.backDepth,
                point,
                gap.frontDepth,
                depthDisplayScale,
                compat::ngRequireObject(viewSession)->camera
            );
            if (gap.overlap) geometry.overlapGapLines.append(line);
            else geometry.gapLines.append(line);
            DepthDrawViewportGapHandle handle;
            handle.backLayerId = gap.backLayerId;
            handle.frontLayerId = gap.frontLayerId;
            handle.overlap = gap.overlap;
            handle.documentPoint = documentPoint;
            handle.backDepth = gap.backDepth;
            handle.frontDepth = gap.frontDepth;
            handle.renderPoint = compat::ngRequireObject(renderer)->buildPoint(
                point,
                (gap.backDepth + gap.frontDepth) * 0.5f,
                depthDisplayScale,
                compat::ngRequireObject(viewSession)->camera,
                7.0f
            );
            geometry.gapHandles.append(handle);
            DepthDrawViewportTextLabel label;
            label.backLayerId = gap.backLayerId;
            label.frontLayerId = gap.frontLayerId;
            label.overlap = gap.overlap;
            label.point = handle.renderPoint.point;
            label.text = gapLabel(gap.overlap, gap.backLayerId, gap.frontLayerId);
            geometry.textLabels.append(label);
        }
    }
    return geometry;
}

compat::Slice<DepthDrawViewportMeshRenderPlan> DepthDrawViewport::collectMeshRenderPlan(DepthDrawViewportRenderGeometry& geometry) {
    const auto receiver = weak_from_this().lock();
    compat::Slice<DepthDrawViewportMeshRenderPlan> plans;
    const auto targetMeshes = geometry.targetMeshes;
    for (std::size_t i = 0; i < targetMeshes.size(); ++i) {
        const auto mesh = targetMeshes[i];
        DepthDrawViewportMeshRenderPlan plan;
        plan.hasMesh = mesh.positions.size() > 0 && mesh.indices.size() > 0;
        if (i < geometry.targetMeshGridUuids.size()) {
            plan.targetGridUuid = geometry.targetMeshGridUuids[i];
            auto target = compat::ngRequireObject(viewSession)->targetByGrid(plan.targetGridUuid);
            plan.hasTarget = target != nullptr && target->getTarget() != nullptr;
            if (plan.hasTarget) {
                plan.drawableChildren = DepthTargetOffscreenTextureRenderer::drawableChildren(target->getTarget()).size();
            }
        }
        plan.willAttemptTexture = plan.hasMesh && plan.hasTarget && plan.drawableChildren > 0;
        plans.append(plan);
    }
    return plans;
}
}
