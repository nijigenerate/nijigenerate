#pragma once
#include "../common/session.hpp"
#include "../renderer.hpp"
#include "diagnostics.hpp"
#include "gpu.hpp"
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <math/veca.hpp>
#include <common/resource_key_map.hpp>
#include <functional>

namespace nijigenerate::viewport::depth::draw {
struct DepthDrawViewportRenderGeometryStats {
    std::size_t normalImageLines = 0, rawDepthLines = 0, coverageLines = 0, coveragePoints = 0;
    std::size_t targetMeshes = 0, targetSurfaceTriangles = 0, targetMeshGridUuids = 0, targetLines = 0;
    std::size_t layerPlaneLines = 0, selectedLayerLines = 0, depthRangeLines = 0, gapLines = 0;
    std::size_t overlapGapLines = 0, gapHandles = 0, textLabels = 0, missingPoints = 0;
    std::size_t winningPoints = 0, winningPointGroups = 0;
};
struct DepthDrawViewportMeshRenderPlan {
    std::uint64_t targetGridUuid = 0;
    bool hasTarget = false;
    std::size_t drawableChildren = 0;
    bool willAttemptTexture = false, hasMesh = false;
};
struct DepthDrawViewportCoveragePoint {
    DepthDrawString layerId;
    nicxlive::core::math::Vec2 documentPoint{NAN, NAN};
    float alpha = NAN;
    DepthTargetRenderPoint renderPoint;
};
struct DepthDrawViewportGapHandle {
    DepthDrawString backLayerId, frontLayerId;
    bool overlap = false;
    nicxlive::core::math::Vec2 documentPoint{NAN, NAN};
    float backDepth = NAN, frontDepth = NAN;
    DepthTargetRenderPoint renderPoint;
};
struct DepthDrawViewportTextLabel {
    DepthDrawString text;
    nicxlive::core::math::Vec2 point{NAN, NAN};
    DepthDrawString backLayerId, frontLayerId;
    bool overlap = false;
};
struct DepthDrawViewportWinningPointGroup {
    DepthDrawString layerId;
    nicxlive::core::math::Vec4 color{NAN, NAN, NAN, NAN};
    compat::Slice<DepthTargetRenderPoint> points;
};
struct DepthDrawViewportRenderGeometry {
    compat::Slice<DepthTargetRenderLine> normalImageLines, rawDepthLines, coverageLines;
    compat::Slice<DepthDrawViewportCoveragePoint> coveragePoints;
    compat::Slice<DepthTargetRenderMesh> targetMeshes;
    compat::Slice<std::uint64_t> targetMeshGridUuids;
    compat::Slice<DepthTargetRenderLine> targetLines, layerPlaneLines, selectedLayerLines, depthRangeLines;
    compat::Slice<DepthTargetRenderLine> gapLines, overlapGapLines;
    compat::Slice<DepthDrawViewportGapHandle> gapHandles;
    compat::Slice<DepthDrawViewportTextLabel> textLabels;
    compat::Slice<DepthTargetRenderPoint> missingPoints, winningPoints;
    compat::Slice<DepthDrawViewportWinningPointGroup> winningPointGroups;
    DepthDrawViewportRenderGeometryStats stats() const;
};
inline constexpr std::size_t DepthDrawMaxCoverageOverlayPoints = 16384;
std::size_t ngDepthDrawCoverageOverlayStride(std::size_t pixelCount);

class DepthDrawViewport : public ::nijigenerate::viewport::base::Viewport,
    public std::enable_shared_from_this<DepthDrawViewport> {
    using Vec2 = nicxlive::core::math::Vec2;
    using Vec4 = nicxlive::core::math::Vec4;
    using Vec3Array = nicxlive::core::math::Vec3Array;
    using Camera = std::shared_ptr<nicxlive::core::math::Camera>;
    using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
    struct UuidHash {
        std::size_t operator()(std::uint64_t value) const noexcept {
            // core.internal.hash scalar hashOf, Igor Stepanov, BSL-1.0:
            // narrow size_t folds the high word; otherwise preserve the value.
            if constexpr (sizeof(std::size_t) == 4)
                return static_cast<std::size_t>(value ^ (value >> 32));
            else return static_cast<std::size_t>(value);
        }
    };
    struct DepthDrawGpuPreviewPendingJob {
        DepthDrawGpuTargetComposeJob job;
        std::shared_ptr<DepthDrawSession> session;
        std::uint64_t revision = 0;
    };
    compat::Slice<NodeRef> selection;
    std::shared_ptr<common::session::DepthViewSession> viewSession;
    std::shared_ptr<DepthDrawSession> drawSession;
    std::shared_ptr<DepthTargetRenderer> renderer;
    std::shared_ptr<DepthTextureMeshRenderer> meshRenderer;
    std::shared_ptr<DepthTargetOffscreenTextureRenderer> offscreenTextureRenderer;
    std::int32_t documentWidth = 1, documentHeight = 1;
    nicxlive::core::common::ResourceKeyMap<std::uint64_t, DepthDrawComposeResult,
        UuidHash, std::equal_to<std::uint64_t>> previewResults;
    nicxlive::core::common::ResourceKeyMap<std::uint64_t, DepthDrawGpuPreviewPendingJob,
        UuidHash, std::equal_to<std::uint64_t>> pendingGpuPreviewJobs;
    compat::RetainedString gpuPreviewErrorMessage;
    void cancelPendingGpuPreviews();
    void syncTargets();
    float selectedDepthDisplayScale();
    const DepthDrawComposeResult* selectedPreviewResult(DepthDrawComposeResult& result);
    Vec2 rectMin(DepthDrawRect rect);
    Vec2 rectMax(DepthDrawRect rect);
    Vec2 documentToModelPoint(Vec2 point);
    void planeDocumentBounds(DepthDrawDepthSpaceLayerPlane plane, Vec2& minimum, Vec2& maximum);
    bool findDepthSpaceLayer(DepthDrawDepthSpaceSummary& summary, DepthDrawString layerId,
        DepthDrawDepthSpaceLayerPlane& plane);
    Vec2 gapMarkerPoint(DepthDrawDepthSpaceLayerPlane backPlane, DepthDrawDepthSpaceLayerPlane frontPlane);
    Vec3Array toLineBuffer(compat::Slice<DepthTargetRenderLine> lines);
    Vec3Array toPointBuffer(compat::Slice<DepthTargetRenderPoint> points);
    void drawLines(compat::Slice<DepthTargetRenderLine> lines, Vec4 color, float width = 1.0f);
    void drawPoints(compat::Slice<DepthTargetRenderPoint> points, Vec4 color, float size = 4.0f);
    void drawRenderGeometry(DepthDrawViewportRenderGeometry& geometry, Camera camera);
    void addWinningPoint(DepthDrawViewportRenderGeometry& geometry, DepthDrawString layerId,
        Vec4 color, DepthTargetRenderPoint point);
    void addCoveragePoints(DepthDrawViewportRenderGeometry& geometry, const DepthDrawLayer& layer,
        float representativeDepth, float depthDisplayScale);
public:
    explicit DepthDrawViewport(std::shared_ptr<DepthDrawSession> session = {});
    DepthDrawViewport(const DepthDrawViewport&) = delete;
    DepthDrawViewport(DepthDrawViewport&&) = delete;
    DepthDrawViewport& operator=(const DepthDrawViewport&) = delete;
    DepthDrawViewport& operator=(DepthDrawViewport&&) = delete;
    virtual std::shared_ptr<common::session::DepthViewSession> depthViewSession();
    virtual std::shared_ptr<DepthDrawSession> depthDrawSession();
    virtual std::shared_ptr<DepthTargetRenderer> targetRenderer();
    virtual void setDepthDrawSession(std::shared_ptr<DepthDrawSession> session);
    virtual void setDocumentSize(std::int32_t width, std::int32_t height);
    virtual DepthDrawComposeResult previewResult(std::uint64_t gridUuid);
    virtual DepthDrawComposeResult composePreview(std::uint64_t gridUuid);
    virtual bool composeSelectedPreviewForUpdate();
    virtual compat::RetainedString gpuPreviewError() const;
    virtual std::size_t pendingGpuPreviewJobCount() const;
    virtual bool submitGpuPreview(std::uint64_t gridUuid);
    virtual std::size_t pollGpuPreviews();
    virtual std::size_t composeDirtyGpuPreviews();
    virtual bool selectLayerPlaneAtDocumentPoint(Vec2 point);
    virtual Vec2 depthViewPointToDocumentPoint(Vec2 point, float representativeDepth = 0.0f);
    virtual bool selectLayerPlaneAtDepthViewPoint(Vec2 point, float representativeDepth = 0.0f);
    virtual bool selectTargetAtDepthViewPoint(Vec2 point, float radius = 14.0f);
    virtual std::size_t composeDirtyPreviews();
    virtual DepthDrawViewportRenderGeometry collectRenderGeometry();
    virtual compat::Slice<DepthDrawViewportMeshRenderPlan> collectMeshRenderPlan(DepthDrawViewportRenderGeometry& geometry);
    void drawOptions() override;
    void drawConfirmBar() override;
    void present() override;
    void withdraw() override;
    void selectionChanged(compat::Slice<NodeRef> selection) override;
    void draw(Camera camera) override;
    void update(ImGuiIO* io, Camera camera) override;
private:
    void prepareSourceClassInfo() override {
        ::nijigenerate::viewport::base::Viewport::prepareSourceClassInfo();
        ::nijigenerate::viewport::base::Viewport::registerSourceClassInfo<DepthDrawViewport>(
            "nijigenerate.viewport.depth.draw.viewport.DepthDrawViewport");
    }
};
}

namespace nijigenerate::viewport::depth::draw::viewport {
using ::nijigenerate::viewport::depth::draw::DepthDrawViewportRenderGeometryStats;
using ::nijigenerate::viewport::depth::draw::DepthDrawViewportMeshRenderPlan;
using ::nijigenerate::viewport::depth::draw::DepthDrawViewportCoveragePoint;
using ::nijigenerate::viewport::depth::draw::DepthDrawViewportGapHandle;
using ::nijigenerate::viewport::depth::draw::DepthDrawViewportTextLabel;
using ::nijigenerate::viewport::depth::draw::DepthDrawViewportWinningPointGroup;
using ::nijigenerate::viewport::depth::draw::DepthDrawViewportRenderGeometry;
using ::nijigenerate::viewport::depth::draw::DepthDrawMaxCoverageOverlayPoints;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawCoverageOverlayStride;
using ::nijigenerate::viewport::depth::draw::DepthDrawViewport;
} // namespace nijigenerate::viewport::depth::draw::viewport

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_DRAW_VIEWPORT_VALUE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.draw.viewport." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.draw.viewport." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_DEPTH_DRAW_VIEWPORT_VALUE_ORIGIN(DepthDrawViewportRenderGeometry);
NG_DEPTH_DRAW_VIEWPORT_VALUE_ORIGIN(DepthDrawViewportRenderGeometryStats);
NG_DEPTH_DRAW_VIEWPORT_VALUE_ORIGIN(DepthDrawViewportMeshRenderPlan);
NG_DEPTH_DRAW_VIEWPORT_VALUE_ORIGIN(DepthDrawViewportCoveragePoint);
NG_DEPTH_DRAW_VIEWPORT_VALUE_ORIGIN(DepthDrawViewportGapHandle);
NG_DEPTH_DRAW_VIEWPORT_VALUE_ORIGIN(DepthDrawViewportTextLabel);
NG_DEPTH_DRAW_VIEWPORT_VALUE_ORIGIN(DepthDrawViewportWinningPointGroup);
#undef NG_DEPTH_DRAW_VIEWPORT_VALUE_ORIGIN
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::DepthDrawViewport> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.draw.viewport.DepthDrawViewport)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDrawViewport)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.draw.viewport.DepthDrawViewport";
    inline static constexpr char unqualifiedElement[] = "DepthDrawViewport";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::draw::DepthDrawViewport> {
    inline static constexpr char className[] = "nijigenerate.viewport.depth.draw.viewport.DepthDrawViewport";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.depth.draw.viewport.DepthDrawViewport)";
};
}
