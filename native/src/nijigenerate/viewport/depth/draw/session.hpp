#pragma once
#include "binding.hpp"
#include "layer.hpp"
#include <nijigenerate/compat/const_slice.hpp>
#include <common/resource_key_map.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace nijigenerate::viewport::depth::draw {
struct DepthDrawReloadStateResult {
    std::size_t matchedLayers = 0, preservedBindings = 0;
    bool preservedSelectedLayer = false, preservedSelectedTarget = false;
};
struct DepthDrawDisplayOptions {
    bool showNormalImage = true;
    bool showRawDepth = false, showCoverage = false;
    bool showComposite = true, showLayerPlanes = true, showDepthRanges = true, showMissingVertices = true;
    bool showWinningLayer = false, useGpuPreview = false;
    bool operator==(const DepthDrawDisplayOptions&) const = default;
};
struct DepthDrawLayerAlphaDepthGapFillSummary {
    bool succeeded = false;
    DepthDrawString layerId;
    io::depthimage::DepthDrawAlphaDepthGapDetection detected;
    io::depthimage::DepthDrawAlphaDepthGapFillResult filled;
};
struct DepthDrawLayerContourRepairSummary {
    bool succeeded = false;
    DepthDrawString layerId;
    std::size_t contourPixels = 0, filledPixels = 0;
};
class DepthDrawSession : public std::enable_shared_from_this<DepthDrawSession> {
    struct GridUuidHash {
        std::size_t operator()(std::uint64_t value) const noexcept {
            // core.internal.hash.hashOf(ulong), selected DMD 2.111.0.
            if constexpr (sizeof(std::size_t) >= sizeof(value)) return static_cast<std::size_t>(value);
            else return static_cast<std::size_t>(value ^ (value >> 32));
        }
    };
    template<class Value> using GridMap = nicxlive::core::common::ResourceKeyMap<
        std::uint64_t, Value, GridUuidHash, std::equal_to<std::uint64_t>>;
    GridMap<bool> previewDirtyTargets;
    GridMap<std::uint64_t> previewTargetRevisions;
    std::uint64_t diagnosticsRevisionValue = 0;
    void bumpDiagnosticsRevision();
    std::ptrdiff_t findLayerIndex(DepthDrawString id) const;
public:
    DepthDrawSession() = default;
    DepthDrawSession(const DepthDrawSession&) = delete;
    DepthDrawSession& operator=(const DepthDrawSession&) = delete;
    virtual ~DepthDrawSession() = default;
    compat::Slice<DepthDrawLayer> layers;
    compat::Slice<DepthDrawBinding> bindings;
    DepthDrawString selectedLayerId;
    std::uint64_t selectedGridUuid = 0;
    DepthDrawDisplayOptions display;
    DepthDrawString sourceIdentity;
    int documentWidth = 0, documentHeight = 0;
    // Dynamic storage retains its real backing through the selected cell.
    // Explicit process-static Slice backing remains a static-lifetime borrow.
    virtual std::shared_ptr<DepthDrawLayer> layerById(DepthDrawString id);
    virtual std::shared_ptr<const DepthDrawLayer> layerById(DepthDrawString id) const;
    virtual compat::Slice<DepthDrawBinding> bindingsForGrid(std::uint64_t gridUuid) const;
    virtual bool hasTargetGrid(std::uint64_t gridUuid) const;
    virtual void markTargetPreviewDirty(std::uint64_t gridUuid);
    virtual std::uint64_t targetPreviewRevision(std::uint64_t gridUuid) const;
    virtual std::uint64_t diagnosticsRevision() const;
    virtual void markLayerPreviewDirty(DepthDrawString layerId);
    virtual void markBindingPreviewDirty(const DepthDrawBinding binding);
    virtual void markAllPreviewDirty();
    virtual bool isTargetPreviewDirty(std::uint64_t gridUuid) const;
    virtual compat::Slice<std::uint64_t> dirtyTargetGridIds() const;
    virtual void clearTargetPreviewDirty(std::uint64_t gridUuid);
    virtual void clearPreviewDirty();
    virtual bool selectLayer(DepthDrawString id);
    virtual bool selectTargetGrid(std::uint64_t gridUuid);
    virtual bool selectLayerPlaneAtDocumentPoint(nicxlive::core::math::Vec2 documentPoint);
    virtual bool updateLayerXYTransform(DepthDrawString layerId, nicxlive::core::math::Vec2 offset,
        nicxlive::core::math::Vec2 scale);
    virtual bool updateLayerZTransform(DepthDrawString layerId, float backDepth, float frontDepth,
        bool invert, float zScale, float zOffset);
    virtual bool updateLayerSampling(DepthDrawString layerId, io::depthimage::DepthImageChannel channel,
        io::depthimage::DepthImageConvolution convolution, int customRadius, float alphaThreshold);
    virtual bool updateLayerVisibility(DepthDrawString layerId, bool visible, bool enabled);
    virtual DepthDrawLayerAlphaDepthGapFillSummary applyLayerAlphaDepthGapFill(DepthDrawString layerId,
        compat::ConstSlice<io::depthimage::DepthDrawAlphaDepthFocusedRule> focusedRules = {});
    virtual DepthDrawLayerContourRepairSummary repairLayerContourDepth(DepthDrawString layerId, int thickness = 2);
    virtual bool replayLayerCleanupOperations(DepthDrawString layerId);
    virtual bool updateBindingSampling(DepthDrawString layerId, std::uint64_t targetGridUuid,
        bool useNormalLayerAlpha, float coverageThreshold);
    virtual bool updateBindingState(DepthDrawString layerId, std::uint64_t targetGridUuid,
        bool enabled, int order, DepthMergePolicy mergePolicy);
    virtual bool normalizeGpuPreviewSampling();
    virtual bool updateDisplayOptions(DepthDrawDisplayOptions nextDisplay);
    virtual void clearSelection();
    virtual void clear();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DepthDrawSession> other);
    virtual int opCmp(std::shared_ptr<DepthDrawSession> other);
};
bool ngDepthDrawSessionObjectEquals(std::shared_ptr<DepthDrawSession> left,
    std::shared_ptr<DepthDrawSession> right);
DepthDrawReloadStateResult ngDepthDrawCarryReloadState(std::shared_ptr<DepthDrawSession> reloaded,
    std::shared_ptr<DepthDrawSession> previous);
} // namespace nijigenerate::viewport::depth::draw

namespace nijigenerate::viewport::depth::draw::session {
using ::nijigenerate::viewport::depth::draw::DepthDrawString;
using ::nijigenerate::viewport::depth::draw::DepthDrawReloadStateResult;
using ::nijigenerate::viewport::depth::draw::DepthDrawDisplayOptions;
using ::nijigenerate::viewport::depth::draw::DepthDrawLayerAlphaDepthGapFillSummary;
using ::nijigenerate::viewport::depth::draw::DepthDrawLayerContourRepairSummary;
using ::nijigenerate::viewport::depth::draw::DepthDrawSession;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawCarryReloadState;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawSessionObjectEquals;
} // namespace nijigenerate::viewport::depth::draw::session

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_SESSION_VALUE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "nijigenerate.viewport.depth.draw.session." #Type; \
        inline static constexpr char unqualifiedArgument[] = #Type; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.draw.session." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_DEPTH_SESSION_VALUE_ORIGIN(DepthDrawReloadStateResult)
NG_DEPTH_SESSION_VALUE_ORIGIN(DepthDrawDisplayOptions)
NG_DEPTH_SESSION_VALUE_ORIGIN(DepthDrawLayerAlphaDepthGapFillSummary)
NG_DEPTH_SESSION_VALUE_ORIGIN(DepthDrawLayerContourRepairSummary)
#undef NG_DEPTH_SESSION_VALUE_ORIGIN

template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::draw::DepthDrawSession> {
    inline static constexpr char className[] = "nijigenerate.viewport.depth.draw.session.DepthDrawSession";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.depth.draw.session.DepthDrawSession)";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::DepthDrawSession> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.draw.session.DepthDrawSession)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDrawSession)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.draw.session.DepthDrawSession";
    inline static constexpr char unqualifiedElement[] = "DepthDrawSession";
};
} // namespace nicxlive::core::d_runtime
