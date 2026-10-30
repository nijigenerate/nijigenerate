#include "regression_smoke.hpp"
#if defined(NG_REGRESSION_SMOKE)
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/depth/bone.hpp>
#include <nijigenerate/commands/depth/map.hpp>
#include <nijigenerate/commands/vertex/define_mesh.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_temp_directory.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/ext/nodes/exgriddeformer.hpp>
#include <nijigenerate/ext/nodes/expathdeformer.hpp>
#include <nijigenerate/io/save.hpp>
#include <nijigenerate/panels/package.hpp>
#include <nijigenerate/platform/environment.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/depth/draw/gpu.hpp>
#include <nijigenerate/viewport/depth/mesheditor/editor.hpp>
#include <nijigenerate/viewport/depth/renderer.hpp>
#include <nijigenerate/viewport/depth/viewport.hpp>
#include <nijigenerate/viewport/depth/tools/base.hpp>
#include <nijigenerate/widgets/modal.hpp>
#include <nijigenerate/windows/package.hpp>
#include <nijigenerate/windows/autosave.hpp>
#include <nijigenerate/windows/settings.hpp>
#include <nijigenerate/windows/command_browser_helpers.hpp>
#include <compat/source_allocator.hpp>
#include <compat/phobos_string_hash.hpp>
#include <nodes/part.hpp>
#include <param/parameter.hpp>
#include <texture.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <limits>
#include <string>
#include <tuple>

namespace nijigenerate::regression_smoke {
namespace {
using Text = compat::RetainedString;
template<class T> using Array = compat::Slice<T>;
using Node = nicxlive::core::nodes::Node;
using Grid = ext::nodes::ExGridDeformer;
using GridRef = std::shared_ptr<Grid>;
using Import = io::depthmap_psd::PsdDepthImportResult;
using Window = windows::psddepthmap::PSDDepthMapWindow;
using namespace viewport::depth::draw;
using namespace project;
using namespace core::actionstack;
using namespace commands::depth::map;

template<std::size_t N> Text literal(const char (&value)[N]) { return Text::fromStaticLiteral(value); }
template<class T, class... Args> std::shared_ptr<T> sourceNew(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{}, std::forward<Args>(args)...);
}
template<class Operation> auto allocated(Operation&& operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) { throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current()); }
    catch (const std::length_error&) { throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current()); }
}
Text concatenate(Text left, Text right) {
    return allocated([&] {
        std::string bytes;
        bytes.reserve(left.size() + right.size());
        bytes.append(left.view());
        bytes.append(right.view());
        return Text(std::move(bytes));
    });
}
// Each subsequent operand is evaluated after the preceding Source concatenation.
template<class First, class... Rest> Text message(First&& first, Rest&&... rest) {
    auto result = first();
    ((result = concatenate(result, rest())), ...);
    return result;
}
template<class T> Text sourceString(const T& value) {
    const std::array arguments{core::translation::ngFormatArgument(value)};
    auto formatted = core::translation::ngFormatTyped("%s", arguments);
    return allocated([&] { return Text(std::move(formatted)); });
}
Text joined(Array<Text> values) {
    return allocated([&] {
        std::string result;
        for (std::size_t i=0;i<values.size();++i) {
            if (i) result.append(" | ");
            result.append(values[i].view());
        }
        return Text(std::move(result));
    });
}
int sourceInt(Text input) {
    // The same selected Phobos parse/whole-input conversion used by the browser,
    // without its UI catch that intentionally turns exceptions into false.
    using namespace windows::command_browser::detail;
    auto raw = input.view();
    if (raw.empty()) integerConversionError<std::int32_t>(raw, 2397);
    bool negative = false;
    if (raw.front() == '-' || raw.front() == '+') {
        negative = raw.front() == '-';
        raw.remove_prefix(1);
    }
    if (raw.empty() || raw.front() < '0' || raw.front() > '9')
        integerConversionError<std::int32_t>(raw, 2397);
    std::uint32_t value = 0;
    const std::uint32_t limit = static_cast<std::uint32_t>(INT32_MAX) + static_cast<unsigned>(negative);
    std::size_t consumed = 0;
    for (const unsigned char c : raw) {
        if (c < '0' || c > '9') break;
        const auto digit = static_cast<std::uint32_t>(c - '0');
        if (value > limit / 10 || (value == limit / 10 && digit > limit % 10)) integerConversionOverflow(2379);
        value = value * 10 + digit;
        ++consumed;
    }
    const auto parsed = std::bit_cast<std::int32_t>(negative ? std::uint32_t{0} - value : value);
    if (consumed != raw.size()) integerConversionError<std::int32_t>(raw.substr(consumed), 1898);
    return parsed;
}

thread_local Text failureMessage;
thread_local std::uint32_t nextJobId = 1;
thread_local std::uint32_t submitCount = 0;
thread_local std::uint32_t pollCount = 0;
struct JobIdHash { std::size_t operator()(std::uint32_t value) const noexcept { return value; } };
using Readbacks = nicxlive::core::common::ResourceKeyMap<std::uint32_t, DepthDrawGpuComposeReadback,
    JobIdHash, std::equal_to<std::uint32_t>>;
thread_local Readbacks readbacks;

bool regressionSmokeDepthDrawGpuSupported() { return true; }
bool regressionSmokeDepthDrawGpuSubmit(DepthDrawGpuComposePacket& packet, std::uint32_t& jobId, GpuError& error) {
    jobId = nextJobId++;
    error = {};
    ++submitCount;
    DepthDrawGpuComposeReadback readback;
    readback.targetGridUuid = packet.targetGridUuid;
    readback.depths.resize(packet.vertices.size());
    readback.winningLayerIndices.resize(packet.vertices.size());
    for (std::size_t i = 0; i < packet.vertices.size(); ++i) {
        readback.depths[i] = 0.625f;
        readback.winningLayerIndices[i] = packet.layers.empty() ? -1 : 0;
    }
    readback.layers.resize(packet.layers.size());
    for (std::size_t layerIndex = 0; layerIndex < packet.layers.size(); ++layerIndex) {
        Array<std::uint8_t> validSamples;
        Array<float> sampleDepths;
        validSamples.resize(packet.vertices.size());
        sampleDepths.resize(packet.vertices.size());
        for (std::size_t i = 0; i < packet.vertices.size(); ++i) {
            validSamples[i] = 1;
            sampleDepths[i] = 0.625f;
        }
        readback.layers[layerIndex] = {static_cast<std::uint32_t>(layerIndex), validSamples, sampleDepths};
    }
    readbacks[jobId] = readback;
    return true;
}
bool regressionSmokeDepthDrawGpuPoll(std::uint32_t jobId, DepthDrawGpuDispatchPollResult& result, GpuError& error) {
    result = {};
    error = {};
    const auto readback = readbacks.findRetained(jobId);
    if (!readback) {
        error = literal("missing regression smoke DepthDraw GPU readback");
        return false;
    }
    result.ready = true;
    result.readback = *readback;
    readbacks.erase(jobId);
    ++pollCount;
    return true;
}
void resetRegressionSmokeDepthDrawGpuHooks() {
    nextJobId = 1;
    submitCount = 0;
    pollCount = 0;
    readbacks = {};
    ngSetDepthDrawGpuTestHooks(&regressionSmokeDepthDrawGpuSupported,
        &regressionSmokeDepthDrawGpuSubmit, &regressionSmokeDepthDrawGpuPoll);
}

class RegressionSmokeDirtyAction final : public actions::Action {
public:
    RegressionSmokeDirtyAction() {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(RegressionSmokeDirtyAction),
            literal("nijigenerate.regression_smoke.RegressionSmokeDirtyAction"));
    }
    void rollback() override {}
    void redo() override {}
    actions::ActionText describe() override { return literal("Regression smoke dirty marker"); }
    actions::ActionText describeUndo() override { return literal("Regression smoke dirty marker"); }
    actions::ActionText getName() override { return literal("RegressionSmokeDirtyAction"); }
    bool merge(actions::ActionPtr) override { return false; }
    bool canMerge(actions::ActionPtr) override { return false; }
};
Text nodeName(const GridRef& grid) { return Text(grid->name); }
void showPanels(std::initializer_list<Text> names) {
    for (auto name : names) if (auto panel = panels::incFindPanelByName(name)) panel->visible = true;
}
void showAllPanels() { for (auto panel : panels::incPanels) panel->visible = true; }
void ensureAnimMode() { incSetEditMode(EditMode::AnimEdit); }
void ensureModelMode() { incSetEditMode(EditMode::ModelEdit); }
void ensureDepthMode() { incSetEditMode(EditMode::DepthEdit, false); }
void ensureVertexMode() { incSetEditMode(EditMode::VertexEdit); }
Text localDepthDrawDataPath(Text filename, Text fallback) {
    auto userProfile = platform::ngGetEnvironmentVariableRetained("USERPROFILE");
    if (!userProfile.empty()) {
        const std::array pieces{userProfile, literal("src"), literal("depth-draw"), literal("data"), filename};
        auto candidate = compat::phobos::ngBuildPath(pieces);
        if (compat::phobos::ngFileExists(candidate.view())) return candidate;
    }
    return fallback;
}
Text writeSmokeDepthPng(Text filename, std::uint8_t baseGray) {
    const std::array pieces{compat::phobos::ngTempDirectory(), filename};
    auto path = compat::phobos::ngBuildPath(pieces);
    constexpr int width = 32, height = 32;
    nicxlive::core::SourceTextureBytes pixels(width * height * 4, 0);
    for (int i = 0; i < width * height; ++i) {
        const auto offset = i * 4;
        const auto gray = static_cast<std::uint8_t>(std::min(255, static_cast<int>(baseGray) + (i % 4) * 8));
        pixels[offset] = gray;
        pixels[offset + 1] = gray;
        pixels[offset + 2] = gray;
        pixels[offset + 3] = 255;
    }
    nicxlive::core::ShallowTexture texture(pixels, width, height, 4);
    texture.save(path);
    return path;
}
GridRef createSmokeDepthGridWithAxes(Text name, Array<float> xs, Array<float> ys) {
    auto grid = Grid::createSourceDeferred([] { return incActivePuppet()->root; });
    grid->name = name.toNullableString();
    auto context = sourceNew<commands::Context>();
    context->nodes({std::static_pointer_cast<Node>(grid)});
    auto result = commands::ngRunCommand(
        commands::detail::constructSourceCommand<commands::vertex::define_mesh::DefineGridCommand>(xs, ys), context);
    if (!result->succeeded) ngRegressionSmokeFail(message(
        [] { return literal("Regression smoke failed to define target grid: "); }, [&] { return result->message; }));
    return grid;
}
GridRef createSmokeDepthGrid(Text name) {
    return createSmokeDepthGridWithAxes(name, {-1.0f, 0.0f, 1.0f}, {-1.0f, 1.0f});
}
struct TextHash { std::size_t operator()(const Text& text) const noexcept {
    return nicxlive::core::d_runtime::ngPhobosStringHash(text.view());
} };
template<class Value> using StringMap = nicxlive::core::common::ResourceKeyMap<Text, Value, TextHash, std::equal_to<Text>>;
Import combinePsdDepthImportPreviews(Array<Import> previews, Array<std::uint64_t> targetGridUuids) {
    Import result;
    for (std::size_t previewIndex = 0; previewIndex < previews.size(); ++previewIndex) {
        auto preview = previews[previewIndex];
        if (previewIndex >= targetGridUuids.size()) break;
        auto targetGridUuid = targetGridUuids[previewIndex];
        auto prefix = concatenate(literal("/combined-"), sourceString(previewIndex));
        StringMap<Text> renamedPaths;
        StringMap<bool> usedPaths;
        for (auto grid : preview.grids) {
            if (!grid.grid || grid.grid->uuid != targetGridUuid) continue;
            auto renamed = grid;
            renamed.layerMasks = grid.layerMasks.duplicate();
            for (auto& layerMask : renamed.layerMasks) {
                auto oldPath = layerMask.layerPath;
                auto newPath = concatenate(prefix, oldPath);
                renamedPaths[oldPath] = newPath;
                usedPaths[oldPath] = true;
                layerMask.layerPath = newPath;
            }
            result.grids.append(renamed);
        }
        for (auto layer : preview.composedLayers) {
            if (!usedPaths.contains(layer.layerPath)) continue;
            auto renamed = layer;
            auto newPath = renamedPaths.findRetained(layer.layerPath);
            if (!newPath) continue;
            renamed.id = *newPath;
            renamed.layerPath = *newPath;
            result.composedLayers.append(renamed);
        }
        for (auto mapping : preview.mappings) {
            if (mapping.targetGridUuid != targetGridUuid || !usedPaths.contains(mapping.layerPath)) continue;
            auto renamed = mapping;
            if (auto newPath = renamedPaths.findRetained(mapping.layerPath)) renamed.layerPath = *newPath;
            result.mappings.append(renamed);
        }
        result.sourceDepthLayerCount += preview.sourceDepthLayerCount;
        result.composedLayerCount += preview.composedLayerCount;
        result.compositionDiagnostics.appendRange(preview.compositionDiagnostics);
        result.matchedLayers += preview.matchedLayers;
        result.unmatchedLayers += preview.unmatchedLayers;
        result.ambiguousLayers += preview.ambiguousLayers;
        result.skippedGrids += preview.skippedGrids;
        result.gpuCompositionRequested = result.gpuCompositionRequested || preview.gpuCompositionRequested;
    }
    return result;
}
void attachSmokeCoveragePart(GridRef grid, Text name, float extent = 1.0f) {
    using namespace nicxlive::core;
    nodes::MeshData data;
    auto vertices=allocated([&] { return std::vector<math::Vec2>{{-extent,-extent},{extent,-extent},{-extent,extent},{extent,extent}}; });
    data.vertices = math::Vec2Array(vertices);
    auto uvs=allocated([] { return std::vector<math::Vec2>{{0.f,0.f},{1.f,0.f},{0.f,1.f},{1.f,1.f}}; });
    data.uvs = math::Vec2Array(uvs);
    data.indices = {std::uint16_t(0),1,3,0,3,2};
    data.origin = {0.f,0.f};
    auto texture = Texture::createSourceDeferred([] {
        SourceTextureBytes bytes{std::uint8_t(255),255,255,255};
        return ShallowTexture(bytes,1,1,4,4);
    },false);
    auto part = nodes::Part::createDataSourceDeferred([&] { return data; },
        [&] { return common::SharedArray<std::shared_ptr<Texture>>{texture}; },
        [] { return Node::inCreateUUID(); }, [&] { return grid; });
    part->name = name.toNullableString();
}
void setupPsdConsecutiveScenario();
void setupPsdImportScenario();
void setupDepthScenario(Text scenario);
void setupPsdConsecutiveScenario() {
    ensureDepthMode();
    auto grid = createSmokeDepthGridWithAxes(literal("PsdDepthConsecutiveSmoke:G"),{0.f,16.f,31.f},{0.f,16.f,31.f});
    if (ngRegressionSmokeFailed()) return;
    auto window = windows::base::ngCreateWindowDeferred<Window>([] {
        return writeSmokeDepthPng(literal("nijigenerate-psd-depth-consecutive-smoke.png"),96);
    });
    window->rebuildPreviewForRegressionSmoke();
    if (!window->loadErrorForRegressionSmoke().empty() ||
        !window->remapAnyLayerToSampledGridForRegressionSmoke(nodeName(grid),grid->uuid) ||
        !window->has3DAdjustGeometryForRegressionSmoke(nodeName(grid))) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth consecutive-adjust smoke failed to prepare mapped geometry: "); },
            [&] { return window->loadErrorForRegressionSmoke(); }, [] { return literal(" :: "); },
            [&] { return window->previewSummaryForRegressionSmoke(); })); return;
    }
    if (!window->setFirstMappedLayerZTransformForRegressionSmoke(nodeName(grid),1.25f,.75f) ||
        !window->hasRealtime3DAdjustTransformForRegressionSmoke(nodeName(grid)) ||
        !window->setFirstMappedLayerZTransformForRegressionSmoke(nodeName(grid),.8f,-.25f) ||
        !window->hasRealtime3DAdjustTransformForRegressionSmoke(nodeName(grid))) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth consecutive-adjust smoke failed to refresh every edit before Apply: "); },
            [&] { return window->previewSummaryForRegressionSmoke(); })); return;
    }
    Text applyMessage;
    if (!window->applyForRegressionSmoke(applyMessage) || window->hasPending3DAdjustLayerChangesForRegressionSmoke() ||
        window->last3DAdjustApplyRecomposedGridCountForRegressionSmoke()!=1 ||
        !window->hasAppliedDepthsForRegressionSmoke(nodeName(grid))) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth consecutive-adjust smoke failed to apply the final edit: "); },
            [&] { return applyMessage; }, [] { return literal(" :: "); },
            [&] { return window->previewSummaryForRegressionSmoke(); })); return;
    }
}
template<class Left,class Right> bool sameArray(const Left& left,const Right& right) {
    if (left.size()!=right.size()) return false;
    for (std::size_t i=0;i<left.size();++i) if (left[i]!=right[i]) return false;
    return true;
}
void setupPsdImportScenario() {
    ensureDepthMode(); showPanels({literal("Viewport"),literal("Tool Settings"),literal("Inspector")});
    Array<float> axes;
    for (float value=-600.f;value<=600.f;value+=40.f) axes.append(value);
    auto bodyGrid=createSmokeDepthGridWithAxes(literal("Body:G"),axes,axes);
    if (ngRegressionSmokeFailed()) return;
    attachSmokeCoveragePart(bodyGrid,literal("bottomwear-back"),300.f);
    auto window=windows::base::ngCreateWindowDeferred<Window>([] {
        return localDepthDrawDataPath(literal("Midori-20260621-color-psd-depth.psd"),literal("regression-smoke-depth.psd"));
    });
    window->rebuildPreviewForRegressionSmoke();
    if (!window->loadErrorForRegressionSmoke().empty()) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to build preview: "); },
            [&] { return window->loadErrorForRegressionSmoke(); })); return;
    }
    if (!window->hasSampledPreviewGridForRegressionSmoke(nodeName(bodyGrid))) {
        const bool bodyRemapped=window->remapLayerNameToGridForRegressionSmoke(literal("bottomwear-back"),bodyGrid->uuid) &&
            window->hasSampledPreviewGridForRegressionSmoke(nodeName(bodyGrid));
        const bool anyRemapped=bodyRemapped || window->remapAnyLayerToSampledGridForRegressionSmoke(nodeName(bodyGrid),bodyGrid->uuid);
        if (!anyRemapped) {
            ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to remap body layer to target grid: "); },
                [&] { return joined(window->diagnosticsForRegressionSmoke()); }, [] { return literal(" :: "); },
                [&] { return window->previewSummaryForRegressionSmoke(); })); return;
        }
    }
    if (window->previewGridCountForRegressionSmoke()==0 || !window->hasSampledPreviewGridForRegressionSmoke(nodeName(bodyGrid))) {
        ngRegressionSmokeFail(literal("PSD depth import smoke failed to build sampled preview grid")); return;
    }
    if (!window->has3DAdjustGeometryForRegressionSmoke(nodeName(bodyGrid))) {
        ngRegressionSmokeFail(literal("PSD depth import smoke failed to build 3D Adjust relationship geometry")); return;
    }
    if (!window->hasFrontOnlyIntersectionConstraintForRegressionSmoke()) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to constrain adjusted layers to intersecting front layers only: "); },
            [&] { return window->frontOnlyIntersectionConstraintDiagnosticsForRegressionSmoke(); })); return;
    }
    auto pngGrid=createSmokeDepthGridWithAxes(literal("PngDepthSmoke:G"),{-1.f,0.f,1.f},{-1.f,0.f,1.f});
    attachSmokeCoveragePart(pngGrid,literal("png-depth-smoke"),1.f);
    auto pngWindow=windows::base::ngCreateWindowDeferred<Window>([] {
        return writeSmokeDepthPng(literal("nijigenerate-psd-depth-import-smoke.png"),192);
    });
    pngWindow->rebuildPreviewForRegressionSmoke();
    if (!pngWindow->loadErrorForRegressionSmoke().empty()) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to load PNG source: "); },
            [&] { return pngWindow->loadErrorForRegressionSmoke(); })); return;
    }
    if (!pngWindow->remapAnyLayerToSampledGridForRegressionSmoke(nodeName(pngGrid),pngGrid->uuid) ||
        !pngWindow->hasSampledPreviewGridForRegressionSmoke(nodeName(pngGrid)) ||
        !pngWindow->has3DAdjustGeometryForRegressionSmoke(nodeName(pngGrid))) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to route PNG through Source / Mapping and 3D Adjust: "); },
            [&] { return joined(pngWindow->diagnosticsForRegressionSmoke()); }, [] { return literal(" :: "); },
            [&] { return pngWindow->previewSummaryForRegressionSmoke(); })); return;
    }
    auto before=pngWindow->previewDepthsForRegressionSmoke(nodeName(pngGrid));
    if (!pngWindow->setFirstMappedLayerZTransformForRegressionSmoke(nodeName(pngGrid),1.25f,.75f) ||
        !pngWindow->hasPending3DAdjustLayerChangesForRegressionSmoke() ||
        !pngWindow->hasRealtime3DAdjustTransformForRegressionSmoke(nodeName(pngGrid)) ||
        !sameArray(pngWindow->previewDepthsForRegressionSmoke(nodeName(pngGrid)),before)) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to update 3D Adjust in real time without composing targets: "); },
            [&] { return pngWindow->previewSummaryForRegressionSmoke(); })); return;
    }
    if (!pngWindow->setFirstMappedLayerZTransformForRegressionSmoke(nodeName(pngGrid),.8f,-.25f) ||
        !pngWindow->hasRealtime3DAdjustTransformForRegressionSmoke(nodeName(pngGrid))) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to update consecutive 3D Adjust edits before Apply: "); },
            [&] { return pngWindow->previewSummaryForRegressionSmoke(); })); return;
    }
    Text applyMessage;
    if (!pngWindow->applyForRegressionSmoke(applyMessage) || pngWindow->hasPending3DAdjustLayerChangesForRegressionSmoke() ||
        pngWindow->last3DAdjustApplyRecomposedGridCountForRegressionSmoke()!=1 ||
        !pngWindow->hasAppliedDepthsForRegressionSmoke(nodeName(pngGrid)) ||
        sameArray(pngWindow->previewDepthsForRegressionSmoke(nodeName(pngGrid)),before)) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to flush and apply deferred 3D Adjust Z Scale/Offset: "); },
            [&] { return applyMessage; }, [] { return literal(" :: "); },
            [&] { return pngWindow->previewSummaryForRegressionSmoke(); })); return;
    }
    auto gpuGrid=createSmokeDepthGridWithAxes(literal("PngGpuDepthSmoke:G"),{-1.f,0.f,1.f},{-1.f,0.f,1.f});
    attachSmokeCoveragePart(gpuGrid,literal("png-gpu-depth-smoke"),1.f);
    auto gpuWindow=windows::base::ngCreateWindowDeferred<Window>([] {
        return writeSmokeDepthPng(literal("nijigenerate-psd-depth-import-gpu-smoke.png"),208);
    });
    gpuWindow->rebuildPreviewForRegressionSmoke();
    if (!gpuWindow->loadErrorForRegressionSmoke().empty()) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to load GPU PNG source: "); },
            [&] { return gpuWindow->loadErrorForRegressionSmoke(); })); return;
    }
    if (!gpuWindow->remapAnyLayerToSampledGridForRegressionSmoke(nodeName(gpuGrid),gpuGrid->uuid)) {
        ngRegressionSmokeFail(literal("PSD depth import smoke failed to map GPU PNG source")); return;
    }
    resetRegressionSmokeDepthDrawGpuHooks(); gpuWindow->setGpuCompositionForRegressionSmoke(true);
    const auto submits=submitCount; const auto polls=pollCount;
    Text gpuApplyMessage;
    const bool applied=gpuWindow->applyForRegressionSmoke(gpuApplyMessage);
    ngClearDepthDrawGpuTestHooks();
    if (!applied || submits==0 || polls==0 || submitCount!=submits || pollCount!=polls ||
        gpuGrid->copyDepths().empty() || gpuGrid->copyDepths()[0]!=.625f) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to apply PNG source through GPU submit/poll/readback: "); },
            [&] { return gpuApplyMessage; })); return;
    }
    auto a=createSmokeDepthGrid(literal("PngMultiSmokeA:G")); auto b=createSmokeDepthGrid(literal("PngMultiSmokeB:G"));
    attachSmokeCoveragePart(a,literal("png-multi-smoke-a"),1.f); attachSmokeCoveragePart(b,literal("png-multi-smoke-b"),1.f);
    a->replaceDepths({.1f,.2f,.3f,.4f,.5f,.6f}); b->replaceDepths({.6f,.5f,.4f,.3f,.2f,.1f});
    auto wa=windows::base::ngCreateWindowDeferred<Window>([] { return writeSmokeDepthPng(literal("nijigenerate-psd-depth-multi-a.png"),80); });
    auto wb=windows::base::ngCreateWindowDeferred<Window>([] { return writeSmokeDepthPng(literal("nijigenerate-psd-depth-multi-b.png"),176); });
    wa->rebuildPreviewForRegressionSmoke(); wb->rebuildPreviewForRegressionSmoke();
    if (!wa->loadErrorForRegressionSmoke().empty() || !wb->loadErrorForRegressionSmoke().empty()) {
        ngRegressionSmokeFail(literal("PSD depth import smoke failed to load multi-binding PNG sources")); return;
    }
    if (!wa->remapAnyLayerToSampledGridForRegressionSmoke(nodeName(a),a->uuid) ||
        !wb->remapAnyLayerToSampledGridForRegressionSmoke(nodeName(b),b->uuid)) {
        ngRegressionSmokeFail(literal("PSD depth import smoke failed to map multiple PNG bindings")); return;
    }
    auto combined=combinePsdDepthImportPreviews({wa->previewForRegressionSmoke(),wb->previewForRegressionSmoke()},{a->uuid,b->uuid});
    incActionClearHistory();
    PsdDepthComposedView composed; Text composeError;
    if (!ngComposePsdDepthImportResult(combined,composed,composeError)) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to compose multiple mapped targets: "); },
            [&] { return composeError; })); return;
    }
    auto result=ngApplyPsdDepthImportResult(composed);
    if (!result->succeeded || sameArray(a->copyDepths(),Array<float>{.1f,.2f,.3f,.4f,.5f,.6f}) ||
        sameArray(b->copyDepths(),Array<float>{.6f,.5f,.4f,.3f,.2f,.1f})) {
        ngRegressionSmokeFail(message([] { return literal("PSD depth import smoke failed to apply multiple mapped targets: "); },
            [&] { return result->message; }, [] { return literal(" a="); }, [&] { return sourceString(a->copyDepths()); },
            [] { return literal(" b="); }, [&] { return sourceString(b->copyDepths()); }, [] { return literal(" composedA="); },
            [&] { return sourceString(combined.grids[0].depths); }, [] { return literal(" composedB="); },
            [&] { return sourceString(combined.grids[1].depths); })); return;
    }
    incActionUndo();
    if (!sameArray(a->copyDepths(),Array<float>{.1f,.2f,.3f,.4f,.5f,.6f}) ||
        !sameArray(b->copyDepths(),Array<float>{.6f,.5f,.4f,.3f,.2f,.1f})) {
        ngRegressionSmokeFail(literal("PSD depth import smoke failed to undo multiple mapped targets")); return;
    }
    incActionRedo();
    if (a->copyDepths()[0]==.1f || b->copyDepths()[0]==.6f) {
        ngRegressionSmokeFail(literal("PSD depth import smoke failed to redo multiple mapped targets")); return;
    }
    windows::base::incPushWindow(window);
}
void setupDepthScenario(Text scenario) {
    using namespace viewport::depth;
    using namespace tools::operation;
    using Vec2 = nicxlive::core::math::Vec2;
    using Vec2Array = nicxlive::core::math::Vec2Array;
    const auto value=scenario.view();
    ensureDepthMode(); showPanels({literal("Viewport"),literal("Tool Settings"),literal("Inspector")});
    if (value=="depth.bonesource-rotation-grid-ui" || value=="depth.bonesource-rotation-path-ui") {
        auto root=ext::nodes::ExDepthRigRoot::createSourceDeferred([] { return incActivePuppet()->root; });
        root->name=literal("BoneSource Rotation Root").toNullableString();
        auto bone=ext::nodes::ExDepthBone::createSourceDeferred([&] { return root; });
        bone->name=literal("BoneSource Rotation Bone").toNullableString(); bone->boneId=literal("RotationBone");
        std::shared_ptr<Node> target;
        if (value=="depth.bonesource-rotation-grid-ui") {
            auto grid=createSmokeDepthGrid(literal("BoneSource Rotation Grid"));
            if (ngRegressionSmokeFailed()) return;
            root->addBoneSource(grid,ext::nodes::ExDepthTargetKind::Grid,bone); target=grid;
        } else {
            auto path=ext::nodes::ExPathDeformer::createSourceDeferred([] { return incActivePuppet()->root; });
            path->name=literal("BoneSource Rotation Path").toNullableString();
            auto points=allocated([] { return std::vector<Vec2>{{-20.f,0.f},{0.f,50.f},{20.f,100.f}}; });
            path->rebuffer(Vec2Array(points)); path->replaceDepths({.25f,.5f,.75f});
            root->addBoneSource(path,ext::nodes::ExDepthTargetKind::Path,bone); target=path;
        }
        incActivePuppet()->rescanNodes(); incSelectNode(target);
    } else if (value=="depth.edit-live-ui-smoke") {
        auto grid=createSmokeDepthGridWithAxes(literal("DepthEditSmoke:G"),{-2.f,-1.f,0.f,1.f,2.f},{-2.f,-1.f,0.f,1.f,2.f});
        if (ngRegressionSmokeFailed()) return;
        auto view=sourceNew<::nijigenerate::viewport::depth::viewport::DepthEditViewport>();
        view->selectionChanged({std::static_pointer_cast<Node>(grid)}); view->present();
        compat::ngRunWithScopeExit([&] {
            auto editor=view->getEditor();
            if (!editor || editor->getTargets().size()!=1 || !editor->depthViewSession() ||
                !editor->depthViewSession()->targetByGrid(grid->uuid)) {
                ngRegressionSmokeFail(literal("DepthEdit smoke failed to initialize shared target view")); return;
            }
            auto activeEditor=editor->getEditorFor(grid);
            auto targetView=editor->depthViewSession()->targetByGrid(grid->uuid);
            if (!activeEditor || !targetView || editor->targetViewFor(activeEditor)!=targetView) {
                ngRegressionSmokeFail(literal("DepthEdit smoke failed to bind editor wrapper to shared target view")); return;
            }
            for (auto mode : {DepthToolMode::DirectDepth,DepthToolMode::AttachedPoint,DepthToolMode::Ring,DepthToolMode::Plane}) {
                view->setToolMode(mode); auto tool=view->activeTool();
                if (view->activeToolMode()!=mode || !tool || tool->mode()!=mode) {
                    ngRegressionSmokeFail(literal("DepthEdit smoke failed to route tool mode through viewport")); return;
                }
            }
            activeEditor->setDepth(0,.75f);
            if (targetView->getDepth(0)!=.75f || activeEditor->copyEditorDepths()[0]!=.75f) {
                ngRegressionSmokeFail(literal("DepthEdit smoke failed to route direct depth edit through shared target view")); return;
            }
            DepthBrushSettings brush; brush.amount=.35f; brush.radiusY=1.4f; brush.hardness=1.f;
            auto baseline=activeEditor->copyEditorDepths();
            if (!editor->commitOperationAdd(activeEditor,DepthAttachedPointOperation::createSource([] { return std::tuple{std::size_t(12),.25f}; })) ||
                !editor->commitOperationAdd(activeEditor,DepthRingOperation::createSource([&] { return std::tuple{Vec2{-2.f,0.f},Vec2{2.f,0.f},brush}; })) ||
                !editor->commitOperationAdd(activeEditor,DepthPlaneOperation::createSource([&] { return std::tuple{Vec2{0.f,0.f},2.f,2.f,brush}; }))) {
                ngRegressionSmokeFail(literal("DepthEdit smoke failed to add attached, ring, and plane operations")); return;
            }
            auto operations=editor->copyOperations(activeEditor); auto depths=activeEditor->copyEditorDepths();
            bool depthChanged=false;
            for (std::size_t i=0;i<depths.size();++i) if (i<baseline.size() && depths[i]!=baseline[i]) { depthChanged=true; break; }
            if (operations.size()!=3 || !depthChanged || !sameArray(targetView->copyWorkingDepths(),depths)) {
                ngRegressionSmokeFail(literal("DepthEdit smoke failed to apply operation edits through shared target view")); return;
            }
            DepthCamera3D camera; auto renderer=sourceNew<DepthTargetRenderer>();
            auto mesh=renderer->buildMesh(targetView,camera); auto lines=renderer->buildGridLines(targetView,camera);
            if (mesh.positions.size()!=targetView->getVertices().size() || mesh.indices.empty() || lines.empty()) {
                ngRegressionSmokeFail(literal("DepthEdit smoke failed to build shared target render geometry")); return;
            }
        },[&] { view->withdraw(); });
    }
}
} // namespace

RegressionSmokeOptions ngParseRegressionSmokeOptions(Array<Text> args) {
    RegressionSmokeOptions options;
    options.enabled = args.size() >= 2 && args[1].view() == "--regression-smoke";
    options.scenario = options.enabled && args.size() >= 3 ? args[2] : Text::emptyLiteral();
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto arg = args[i].view();
        if (arg == "--regression-computer-use") options.computerUse = true;
        if (arg == "--regression-frames" && i + 1 < args.size()) options.frames = sourceInt(args[i + 1]);
        if (arg == "--regression-frame-delay-ms" && i + 1 < args.size()) options.frameDelayMs = sourceInt(args[i + 1]);
    }
    if (options.frames < 1) options.frames = 1;
    if (options.frameDelayMs < 0) options.frameDelayMs = 0;
    return options;
}
void ngRegressionSmokeFail(Text value) { failureMessage = value; }
bool ngRegressionSmokeFailed() { return !failureMessage.empty(); }
Text ngRegressionSmokeFailureMessage() { return failureMessage; }

void ngSetupRegressionSmokeScenario(Text scenario) {
    using namespace windows;
    const auto value = scenario.view();
    const bool isPanelScenario = value.starts_with("panels.") || value == "viewport.panels" ||
        value == "node.visibility-lock" || value == "inspectors.commit-boundaries" || value == "undo.ui-commit-boundaries";
    const bool isViewportScenario = value.starts_with("viewport.");
    const bool isWidgetScenario = value.starts_with("widgets.");
    if (isPanelScenario || isViewportScenario || isWidgetScenario) {
        showAllPanels();
        if (value == "panels.timeline" || value == "animation.timeline-ui" || value == "animation.playback" ||
                value == "animation.keyframe-copy-paste" || value == "widgets.timeline" || value == "viewport.animation-mode")
            ensureAnimMode();
        else if (value.starts_with("depth.") || value == "viewport.depth-mode" || value == "depthbone.refresh-queue")
            ensureDepthMode();
        else if (value.starts_with("mesh.") || value.starts_with("deform.") || value == "viewport.model-mode")
            ensureModelMode();
        return;
    }
    if (value == "tools.command-browser") incPushWindow(ngCreateWindowDeferred<CommandBrowserWindow>());
    else if (value == "tools.texture-viewer") incPushWindow(ngCreateWindowDeferred<TextureViewerWindow>([] { return core::ngGetLogo(); }));
    else if (value == "tools.export-dialogs" || value == "windows.export-import" || value == "project.file-dialogs")
        incPushWindow(ngCreateWindowDeferred<inpexport::ExportWindow>([] { return literal("regression-smoke.inp"); }));
    else if (value == "tools.ai-agent" || value == "api.agent-panel") showPanels({literal("AI Agent")});
    else if (value == "tools.shell") showPanels({literal("Shell")});
    else if (value == "windows.welcome-about") {
        incPushWindow(ngCreateWindowDeferred<WelcomeWindow>());
        incPushWindow(ngCreateWindowDeferred<AboutWindow>());
    } else if (value == "windows.automesh-batch") widgets::modal::incModalAdd(sourceNew<AutoMeshBatchWindow>());
    else if (value == "windows.settings" || value == "settings.window") incPushWindow(ngCreateWindowDeferred<settings::SettingsWindow>());
    else if (value == "windows.rename") {
        static thread_local Text renameTarget = literal("Regression");
        incPushWindow(ngCreateWindow<RenameWindow>(renameTarget));
    } else if (value == "windows.flip-config") incPushWindow(ngCreateWindowDeferred<FlipPairWindow>());
    else if (value == "windows.parameter-editors") {
        auto parameter = nicxlive::core::param::Parameter::createSourceDeferred(
            [] { return literal("Regression Smoke").toNullableString(); }, [] { return true; });
        incActivePuppet()->parameters.push_back(parameter);
        incPushWindow(ngCreateWindowDeferred<ParamEditorWindow>([&] { return parameter; }));
    } else if (value == "windows.parameter-split") {
        auto parameter = nicxlive::core::param::Parameter::createSourceDeferred(
            [] { return literal("Regression Smoke Split").toNullableString(); }, [] { return true; });
        incActivePuppet()->parameters.push_back(parameter);
        incPushWindow(ngCreateWindowDeferred<ParamSplitWindow>([] { return std::size_t(0); }, [&] { return parameter; }));
    } else if (value == "windows.autosave")
        incPushWindow(ngCreateWindowDeferred<autosave::RestoreSaveWindow>([] { return literal("regression-smoke.inx"); }));
    else if (value == "render.backend-gl-sdl" || value == "platform.input-window") showPanels({literal("Viewport")});
    else if (value == "render.postprocess" || value == "render.onion-slice" || value == "viewport.driver-postprocess")
        showPanels({literal("Viewport")});
    else if (value == "project.close-dirty-prompts") {
        io::save::incSetSaveProjectOnClose(literal("Ask"));
        incActionPush(sourceNew<RegressionSmokeDirtyAction>());
        io::save::incCloseProjectAsk();
    } else if (value == "project.psd-depth-3d-adjust-consecutive-smoke") setupPsdConsecutiveScenario();
    else if (value == "project.psd-depth-map-import-ui-smoke" || value == "windows.psd-depth-map") setupPsdImportScenario();
    else if (value == "project.export-video" || value.starts_with("io.video-"))
        incPushWindow(ngCreateWindowDeferred<VideoExportWindow>([] { return literal("regression-smoke.mp4"); }));
    else if (value == "io.image-export")
        incPushWindow(ngCreateWindowDeferred<ImageExportWindow>([] { return literal("regression-smoke.png"); }));
    else if (value.starts_with("project.export-") || value == "render.blend-modes") showPanels({literal("Viewport")});
    else if (value == "automesh.async-shortcut") {
        ensureVertexMode(); widgets::modal::incModalAdd(sourceNew<AutoMeshBatchWindow>());
    } else if (value == "simplephysics.runtime") {
        incActivePuppet()->enableDrivers = true; showPanels({literal("Viewport")});
    } else if (value.starts_with("depth.") || value == "depthbone.refresh-queue") setupDepthScenario(scenario);
    else if (value.starts_with("mesh.") || value.starts_with("deform.")) {
        ensureModelMode(); showPanels({literal("Viewport"),literal("Tool Settings"),literal("Inspector")});
    }
}

} // namespace nijigenerate::regression_smoke
#endif
