#pragma once
#include "psddepthmap_complete_values.hpp"
#include "base.hpp"
#include <nijigenerate/commands/depth/map.hpp>
#include <nijigenerate/commands/base.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/io/depthsample.hpp>
#include <nijigenerate/viewport/depth/camera.hpp>
#include <texture.hpp>
#include <array>
#include <common/resource_key_map.hpp>
#include <compat/phobos_string_hash.hpp>
namespace nijigenerate::windows::psddepthmap {
using namespace io::depthmap_psd;
using io::depthsample::DepthSampleConvolution;
using io::depthsample::DepthSampleChannel;
using commands::depth::map::PsdDepthComposedView;
using commands::depth::map::PsdDepthGpuComposeWork;
using detail::PsdDepthLayerEdit;
using detail::PsdDepthPendingLayerChange;
using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
using DeformableRef = std::shared_ptr<nicxlive::core::nodes::Deformable>;
using TextureRef = std::shared_ptr<nicxlive::core::Texture>;
using ContextRef = std::shared_ptr<commands::Context>;
using ExPartRef = std::shared_ptr<ext::nodes::ExPart>;
using Vec2 = nicxlive::core::math::Vec2;
class PSDDepthMapWindow : public base::Window {
    template<class T, class... Args> friend std::shared_ptr<T> base::ngCreateWindow(Args&&...);
    struct CacheHash {
        std::size_t operator()(const compat::RetainedStringSlice& key) const noexcept {
            return nicxlive::core::d_runtime::ngPhobosStringHash(key.view());
        }
    };
    template<class Value> using LayerCache = nicxlive::core::common::ResourceKeyMap<
        compat::RetainedStringSlice, Value, CacheHash, core::selector::SelectorStringEqual>;
    using TextureCache = LayerCache<TextureRef>;
    compat::RetainedString path;
    PsdDepthImportSettings settings;
    PsdDepthImportResult preview;
    PsdDepthComposedView composedPreview;
    PsdDepthGpuComposeWork pendingPreviewGpuComposition;
    compat::RetainedString errorMessage, lastApplyErrorMessage;
    bool previewDirty = true, onlyProblemLayers = false;
    std::ptrdiff_t selectedGridIndex = 0, selected3DAdjustLayerIndex = 0;
    bool initial3DAdjustTabSelected = false;
    TextureCache originalPreviewTextures, depthMaskPreviewTextures, rawCompositePreviewTextures;
    LayerCache<compat::Slice<PsdDepth3DAdjustSample>> threeDAdjustSamples;
    LayerCache<PsdDepth3DAdjustMesh> threeDAdjustMeshes;
    bool threeDAdjustSamplesPrepared = false;
    std::shared_ptr<detail::PsdDepth3DAdjustGpuRenderer> threeDAdjustGpuRenderer;
    TextureRef threeDAdjustPreviewTexture;
    int threeDAdjustPreviewWidth = 0, threeDAdjustPreviewHeight = 0;
    compat::Slice<std::uint8_t> threeDAdjustPreviewCaptureRgba;
    int threeDAdjustPreviewCaptureWidth = 0, threeDAdjustPreviewCaptureHeight = 0;
    bool threeDAdjustPreviewDirty = true;
    compat::Slice<PsdDepthPendingLayerChange> pending3DAdjustLayerChanges;
    std::size_t last3DAdjustApplyRecomposedGridCount = 0;
    LayerCache<PsdDepthLayerEdit> threeDAdjustPreparedTransforms;
    LayerCache<compat::Slice<std::size_t>> threeDAdjustFrontIntersections;
    LayerCache<bool> threeDAdjustDisplayDepthDirty;
    std::shared_ptr<core::actionstack::ActionStackScope> dialogActionScope;
    std::shared_ptr<commands::CommandScopeRegistration> dialogCommandScope;
    bool dialogDisplayed = false;
    compat::Slice<PsdDepthDialogLayerState> pendingLayerStatesAfterRebuild;
    LayerCache<bool> alphaDepthGapFillAppliedByLayer;
#if defined(NG_COMMAND_BROWSER_DIFFERENTIAL)
    bool failNextPreviewCompositionForRegression = false;
#endif
    viewport::depth::DepthCamera3D threeDAdjustCamera;
    std::uint64_t threeDAdjustCameraTargetUuid = 0;
    int threeDAdjustCameraLeft = INT32_MIN, threeDAdjustCameraTop = INT32_MIN;
    int threeDAdjustCameraRight = INT32_MIN, threeDAdjustCameraBottom = INT32_MIN;
    float threeDAdjustLastYaw = NAN, threeDAdjustLastPitch = NAN, threeDAdjustLastZoom = NAN;
    Vec2 threeDAdjustLastPan{NAN, NAN};
    static constexpr float PreviewSize = 160.f;
    static constexpr int PsdDepth3DAdjustMeshStep = 2;
    static constexpr float PsdDepth3DAdjustDepthScale = .22f;
    static constexpr std::array<const char*,13> ConvolutionNames{"Nearest","Box3x3","Box5x5","Gaussian3x3","Gaussian5x5","Median3x3","Frontmost3x3","Backmost3x3","BoxCustom","GaussianCustom","MedianCustom","FrontmostCustom","BackmostCustom"};
    static constexpr std::array<const char*,5> ChannelNames{"AverageRGB","R","G","B","Luminance"};
    static constexpr std::array<const char*,4> MissingPolicyNames{"KeepExisting","SetZero","SetBack","SkipGrid"};
    compat::RetainedString cleanupLayerKey(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    bool applyAlphaDepthGapFillToLayer(PsdDepthComposedLayer& layer, std::size_t layerIndex);
    void replayAlphaDepthGapFills();
    void rebuildPreview();
    bool previewCompositionPending() const;
    void cancelPendingPreviewGpuComposition();
    bool startPreviewComposition();
    void pollPendingPreviewGpuComposition();
    void applyAlphaDepthGapFill();
    void disposePreviewTextures();
    compat::RetainedString layerCacheKey(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    std::shared_ptr<PsdDepthComposedLayer> findLayerPreview(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    std::shared_ptr<PsdDepthComposedLayer> findComposedLayer(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    std::shared_ptr<PsdDepthLayerMapping> findMapping(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    bool hasOtherMappings();
    compat::RetainedString firstOtherLayerPath();
    TextureRef firstOtherPreviewTexture();
    bool isOthersSelected();
    compat::Slice<compat::RetainedString> diagnostics();
    void drawDiagnostics();
    compat::Slice<std::uint8_t> transformedDepthPreviewRgba(PsdDepthComposedLayer& layerPreview);
    TextureRef layerPreviewTexture(compat::RetainedString layerPath, std::uint64_t targetGridUuid, bool depthMask);
    TextureRef compositePreviewTexture(PsdDepthGridResult& gridResult);
    void drawLayerPreviewTooltip(compat::RetainedString layerPath, std::uint64_t targetGridUuid, bool depthMask);
    void drawLayerPreviewHoverLabel(compat::RetainedString label, compat::RetainedString layerPath, std::uint64_t targetGridUuid, bool depthMask);
    void drawLayerPreviewHoverText(compat::RetainedString label, compat::RetainedString layerPath, std::uint64_t targetGridUuid, bool depthMask);
    ExPartRef matchedPart(PsdDepthLayerMapping& mapping);
    void drawMatchedNodeTooltip(PsdDepthLayerMapping& mapping);
    void drawMatchedNodeText(PsdDepthLayerMapping& mapping);
    ContextRef dialogCommandContext();
    ContextRef dialogLayerCommandContext(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    ContextRef dialogNodeCommandContext(NodeRef node);
    bool drawEnumCombo(compat::RetainedString label, PsdDepthConvolution& value);
    bool drawEnumCombo(compat::RetainedString label, PsdDepthMissingPolicy& value);
    bool drawEnumCombo(compat::RetainedString label, PsdDepthChannel& value);
    bool isCustomConvolution();
    compat::Slice<DeformableRef> currentTargets();
    compat::RetainedString currentMappingLabel(PsdDepthLayerMapping& mapping);
    bool drawManualMappingCombo(PsdDepthLayerMapping& mapping);
    void drawOptions();
    void drawCompositePreviewTooltip(PsdDepthGridResult& gridResult);
    void ensureSelectedGridIndex();
    void drawGridList(float height);
    void drawGridEnabledCheckbox(DeformableRef grid);
    void drawComposedLayerEnabledCheckbox(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    void drawComposedLayerShowCheckbox(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    void drawComposedLayerDepthEnabledCheckbox(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    PsdDepthLayerEdit layerTransform(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    DepthSampleConvolution threeDAdjustSampleConvolution();
    bool gridResultUsesLayer(PsdDepthGridResult& gridResult, compat::RetainedString layerPath);
    bool layerTargetsGrid(compat::RetainedString layerPath, std::uint64_t gridUuid);
    void disposeCompositePreviewTexture(PsdDepthGridResult& gridResult);
    void disposeCompositePreviewTextures();
    void refreshGridResultDepths(PsdDepthGridResult& gridResult);
    void refreshPreviewDepthsForLayer(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    void invalidate3DAdjustLayerCaches(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    void refreshAfter3DAdjustLayerChange(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    void ensureDialogActionScope();
    void activateDialogCommandContext();
    void deactivateDialogCommandContext();
    void closeDialogActionScope();
    static PsdDepthImportSettings cloneDialogSettings(PsdDepthImportSettings source);
    static PsdDepthDialogLayerState dialogLayerState(PsdDepthComposedLayer& layer);
    static bool sameDialogLayerState(PsdDepthDialogLayerState a, PsdDepthDialogLayerState b);
    static void applyDialogLayerStateFields(PsdDepthComposedLayer& layer, PsdDepthDialogLayerState state);
    void stage3DAdjustLayerChange(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    bool compose3DAdjustChangesForApply();
    void drawLayerZControls(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    void drawMappingLayerRow(PsdDepthLayerMapping& mapping, DeformableRef grid = {}, PsdDepthGridLayerMask* layerMask = {});
    void drawSelectedGridLayers(float height);
    void drawComposedSourceLayers(float height);
    void drawGridPreview(float height);
    void drawSourceMappingTab(float height);
    std::shared_ptr<PsdDepthComposedLayer> selected3DAdjustLayerPreview();
    void draw3DAdjustLayerControlsPanel(float height);
    void resetPsdDepth3DAdjustCameraToBounds(float sourceWidth, float sourceHeight, ImVec2 canvasSize);
    bool threeDAdjustCameraChanged();
    void rememberThreeDAdjustCamera();
    bool buildThreeDAdjustOverallPreview( int framebufferWidth, int framebufferHeight, PsdDepthDialogOverallPreview& result );
    DepthSampleChannel threeDAdjustSampleChannel(PsdDepthChannel channel);
    DepthSampleChannel threeDAdjustSampleChannel();
    bool threeDAdjustDepthAt(PsdDepthComposedLayer& layerPreview, int x, int y, float& depth);
    void prepareThreeDAdjustSamples();
    bool preparedThreeDAdjustDepthAt(PsdDepthComposedLayer& layerPreview, int x, int y, float& depth);
    float threeDAdjustDepthDisplayScale();
    compat::Slice<PsdDepth3DAdjustSample> threeDAdjustLayerSamples(PsdDepthComposedLayer& layerPreview);
    PsdDepthLayerEdit threeDAdjustPreparedTransform(PsdDepthComposedLayer& layerPreview);
    float threeDAdjustDisplayDepth(PsdDepthComposedLayer& layerPreview, float bakedDepth);
    std::shared_ptr<PsdDepth3DAdjustSample> threeDAdjustSampleAtDocument( PsdDepthComposedLayer& layerPreview, int documentX, int documentY );
    bool threeDAdjustLayersIntersect( PsdDepthComposedLayer& backLayer, PsdDepthComposedLayer& frontLayer );
    compat::Slice<std::size_t> threeDAdjustFrontIntersectingLayerIndices(PsdDepthComposedLayer& layerPreview);
    float threeDAdjustConstrainedDisplayDepth( PsdDepthComposedLayer& layerPreview, PsdDepth3DAdjustSample& sample );
    void updateThreeDAdjustMeshDisplayDepths( PsdDepthComposedLayer& layerPreview, PsdDepth3DAdjustMesh& mesh );
    std::shared_ptr<PsdDepth3DAdjustMesh> threeDAdjustLayerMesh(PsdDepthComposedLayer& layerPreview);
    void draw3DAdjustRelationshipCanvas(float height);
    void draw3DAdjustTargetPreview(float height);
    PsdDepth3DAdjustGeometryStats threeDAdjustGeometryStats(PsdDepthGridResult& gridResult);
    void draw3DAdjustTab(float height);
    bool apply();
    void onBeginUpdate() override;
    void onUpdate() override;
    void onClose() override;
public:
    PSDDepthMapWindow() = default;
    virtual bool dialogCommandsAvailable();
    virtual bool applyDialogResult();
    virtual void cancelDialog();
    virtual compat::Slice<NodeRef> dialogContextNodesForLayer(compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    virtual compat::Slice<NodeRef> selectedDialogContextNodes();
    virtual bool dialogContextMatchesLayer(ContextRef ctx, compat::RetainedString layerPath, std::uint64_t targetGridUuid);
    virtual bool captureDialogContextLayerState( ContextRef ctx, compat::RetainedString layerPath, PsdDepthDialogLayerState& state );
    virtual bool dialogMappingTargetAvailable(NodeRef target);
    virtual bool dialogContextTargetUuid(ContextRef ctx, std::uint64_t& uuid);
    virtual PsdDepthDialogSettingsState captureDialogSettingsState();
    virtual bool captureDialogOverallPreview( PsdDepthDialogOverallPreview& result, compat::RetainedString& error );
    virtual bool captureDialogContextPartData( ContextRef ctx, compat::Slice<PsdDepthDialogPartData>& result, compat::RetainedString& error );
    virtual bool applyDialogSettingsState(PsdDepthDialogSettingsState state);
    virtual bool captureDialogLayerState( compat::RetainedString layerPath, std::uint64_t targetGridUuid, PsdDepthDialogLayerState& state );
    virtual bool applyDialogLayerState(PsdDepthDialogLayerState state, bool stageForApply);
    virtual compat::Slice<PsdDepthDialogLayerPixels> captureDialogLayerPixels();
    virtual bool applyDialogLayerPixels(compat::Slice<PsdDepthDialogLayerPixels> state);
    virtual bool applyDialogAlphaDepthGapFill();
    static bool dialogLayerPixelsEqual( compat::Slice<PsdDepthDialogLayerPixels> a, compat::Slice<PsdDepthDialogLayerPixels> b );

#if defined(NG_COMMAND_BROWSER_DIFFERENTIAL)
    virtual void beginDialogCommandSessionForRegression();
    virtual bool dialogCommandScopeActiveForRegression();
    virtual bool dialogActionScopeActiveForRegression();
    virtual void setDialogLayerStateForRegression(PsdDepthDialogLayerState state);
    virtual bool setDialogPartDataForRegression(
            DeformableRef target,
            int width,
            int height,
            compat::Slice<std::uint8_t> colorRgba,
            compat::Slice<std::uint8_t> depthRgba,
            compat::Slice<std::uint8_t> previewRgba,
            compat::Slice<float> depths
        );
    virtual void clearDialogPartDataForRegression();
    virtual bool setDialogLayerPixelsForRegression(
            int width,
            int height,
            compat::Slice<std::uint8_t> depthRgba,
            compat::Slice<std::uint8_t> maskRgba
        );
    virtual bool replayDialogCleanupAfterRebuildForRegression(compat::Slice<std::uint8_t> rebuiltDepthRgba);
    virtual void failNextPreviewCompositionForRegressionTest();
    virtual void useCurrentPreviewForDialogApplyRegression();
    virtual bool prepareDialogApplyForRegression();
    virtual void endDialogCommandSessionForRegression();
#endif
#if defined(NG_REGRESSION_SMOKE)
    virtual void rebuildPreviewForRegressionSmoke();
    virtual compat::RetainedString loadErrorForRegressionSmoke() const;
    virtual std::size_t previewGridCountForRegressionSmoke() const;
    virtual bool remapLayerNameToGridForRegressionSmoke(compat::RetainedString layerName, std::uint64_t gridUuid);
    virtual bool remapAnyLayerToSampledGridForRegressionSmoke(compat::RetainedString gridName, std::uint64_t gridUuid);
    virtual bool hasSampledPreviewGridForRegressionSmoke(compat::RetainedString gridName);
    virtual bool has3DAdjustGeometryForRegressionSmoke(compat::RetainedString gridName);
    virtual compat::Slice<compat::RetainedString> diagnosticsForRegressionSmoke();
    virtual compat::RetainedString previewSummaryForRegressionSmoke();
    virtual bool applyForRegressionSmoke(compat::RetainedString& message);
    virtual void setGpuCompositionForRegressionSmoke(bool enabled);
    virtual bool setFirstMappedLayerZTransformForRegressionSmoke(compat::RetainedString gridName, float zScale, float zOffset);
    virtual bool hasPending3DAdjustLayerChangesForRegressionSmoke() const;
    virtual std::size_t last3DAdjustApplyRecomposedGridCountForRegressionSmoke() const;
    virtual bool hasRealtime3DAdjustTransformForRegressionSmoke(compat::RetainedString gridName);
    virtual bool hasFrontOnlyIntersectionConstraintForRegressionSmoke();
    virtual compat::RetainedString frontOnlyIntersectionConstraintDiagnosticsForRegressionSmoke();
    virtual compat::Slice<float> previewDepthsForRegressionSmoke(compat::RetainedString gridName);
    virtual PsdDepthImportResult previewForRegressionSmoke();
    virtual bool hasAppliedDepthsForRegressionSmoke(compat::RetainedString gridName);
#endif
private:
    void initialize(compat::RetainedString path);
};
std::shared_ptr<PSDDepthMapWindow> ngActivePsdDepthMapWindow();
}
