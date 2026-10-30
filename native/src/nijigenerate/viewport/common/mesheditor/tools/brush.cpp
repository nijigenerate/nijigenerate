// Copyright 2020-2024 Inochi2D Project; 2025 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "brush.hpp"
#include <nijigenerate/viewport/common/mesheditor/brushes/package.hpp>
#include <nijigenerate/viewport/common/mesheditor/brushstate.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/ext/nodes/excamera_math.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/widgets/statusbar.hpp>
#include <nijigenerate/widgets/texture.hpp>
#include <runtime_state.hpp>
#include <puppet.hpp>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::common::mesheditor::tools::brush {
namespace input = core::input;
namespace runtime = nicxlive::core;
namespace {
auto sourceEditorValues(const base::Editors& editors) {
    try {
        return editors.values();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::SourceCString sourceCString(compat::RetainedString input) {
    compat::SourceCString result{compat::RetainedStringSlice(input)};
    compat::detail::ngSourceCStringPostconditions(input.view(), result.data());
    return result;
}
using Vec2 = base::Vec2;
using Vec4 = runtime::math::Vec4;
using Mat4 = runtime::math::Mat4;
constexpr double infinity = std::numeric_limits<double>::infinity();
template<class Range> decltype(auto) sourceAt(Range& values, std::size_t index, std::size_t line) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
        compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/viewport/common/mesheditor/tools/brush.d"), line);
    return values.data()[index];
}
thread_local std::shared_ptr<brushes::Brush> selectedBrush;
std::shared_ptr<brushes::Brush> currentBrush() {
    if (!selectedBrush) {
        const auto brushes = brushes::ngBrushList();
        selectedBrush = sourceAt(brushes, 0, 63);
    }
    return selectedBrush;
}
void setCurrentBrush(std::shared_ptr<brushes::Brush> brush) { if (brush) selectedBrush = std::move(brush); }
template<class T> T clamp(T value, T low, T high) { return value < low ? low : value > high ? high : value; }
Vec2 subtract(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
#if defined(_MSC_VER)
// LDC's Windows MSVC target maps D real to binary64.
using SourceReal = double;
#else
using SourceReal = long double;
#endif
SourceReal length(Vec2 value) {
    const float xSquared = value.x * value.x;
    const float ySquared = value.y * value.y;
    SourceReal sum = 0;
    sum += xSquared;
    sum += ySquared;
    using std::sqrt;
    return sqrt(sum);
}
Vec2 interpolate(Vec2 start, Vec2 end, float parameter) {
    return {start.x * (1 - parameter) + end.x * parameter, start.y * (1 - parameter) + end.y * parameter};
}
Vec4 multiply(const Mat4& matrix, Vec4 value) {
    return ext::nodes::excamera_detail::point(matrix, value);
}
template<std::size_t N> compat::RetainedString text(const char (&key)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
}
// D leaves out-of-range floating-to-size_t conversions unspecified. Avoid native UB;
// Windows behavioral validation must not describe this fallback as target equivalence.
std::size_t sizeCast(double value) {
    const double upper = std::ldexp(1., std::numeric_limits<std::size_t>::digits);
    if (!std::isfinite(value) || value < 0 || value >= upper) return 0;
    return static_cast<std::size_t>(value);
}
}
void BrushTool::resetDragTracking() {
    dragStartPositions = {}; draggedVertices.resize(0); strokeVertices.resize(0);
    waitingForResult = false; pendingSampleParameter = 1; lastResultSerial = core::ngDifferenceAggregationResultSerial;
    bestScores.resize(0); bestParameters.resize(0); activeRegionValid = false;
    activeViewportWidth = activeViewportHeight = 0; activeRegion = {};
    currentSampleIndex = currentStageSampleCount = 0; currentStride = 0; currentRangeMin = 0; currentRangeMax = 1;
    stageBestScore = infinity; stageBestParameter = 1; bestAggregateScore = infinity; bestAggregateParameter = 1;
    lastSampleAggregateScore = infinity; anyVertexAdjusted = false; optimizePending = false;
    baselineTileValues.resize(0); baselineGlobalScore = infinity; baselineValid = false;
}
void BrushTool::captureBaselineDifference() {
    const auto keepAlive = shared_from_this();
    baselineTileValues.resize(TileCount, std::numeric_limits<double>::quiet_NaN());
    if (core::ngDifferenceAggregationResultValid) {
        for (int i = 0; i < TileCount; ++i) {
            const double count = core::ngDifferenceAggregationResult.tileCounts[i];
            baselineTileValues[i] = count > 0 ? core::ngDifferenceAggregationResult.tileTotals[i] / count : infinity;
        }
        baselineGlobalScore = computeGlobalDifferenceMetric();
    } else { for (int i = 0; i < TileCount; ++i) baselineTileValues[i] = 0; baselineGlobalScore = 0; }
    baselineValid = true;
}
bool BrushTool::onDragStart(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this(); const bool started = NodeSelect::onDragStart(mouse, editor);
    if (started) {
        resetDragTracking(); captureBaselineDifference();
        if (!flow) {
            const auto receiver = compat::ngRequireObject(editor);
            const auto position = compat::ngRequireObject(editor)->mousePos;
            const auto brush = currentBrush();
            weights = receiver->getVerticesInBrush(position, brush);
        }
        initPos = compat::ngRequireObject(editor)->mousePos; axisDirection = 0;
    }
    return started;
}
bool BrushTool::onDragEnd(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this(); const bool result = NodeSelect::onDragEnd(mouse, editor);
    if (!draggedVertices.empty() && brushstate::ngBrushHasTeacherPart()) initializeStrokeOptimization(editor);
    else resetDragTracking();
    return result;
}
void BrushTool::setToolMode(base::VertexToolMode mode, EditorRef editor) {
    const auto keepAlive = shared_from_this(); NodeSelect::setToolMode(mode, editor); viewport::base::ngSetViewportAlwaysUpdate(true);
}
std::int32_t BrushTool::peek(ImGuiIO* io, EditorRef editor) {
    const auto keepAlive = shared_from_this(); NodeSelect::peek(io, editor);
    if (input::ngInputIsMouseReleased(ImGuiMouseButton_Left))
        onDragEnd(compat::ngRequireObject(editor)->mousePos, editor);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (!editor) compat::ngFailNullAccess(true);
        editor->maybeSelectOne = UINT64_MAX;
    }
    const auto dragging = [&] { return viewport::base::ngDragStartedInViewport(ImGuiMouseButton_Left) &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left) && input::ngInputIsDragRequested(ImGuiMouseButton_Left); };
    if (!isDragging && !compat::ngRequireObject(editor)->isSelecting && dragging()) {
        isDragging = true;
        onDragStart(compat::ngRequireObject(editor)->mousePos, editor);
    }
    if (isDragging) return Drawing;
    if (compat::ngRequireObject(io)->KeyAlt) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (compat::ngRequireObject(editor)->isPointOver(editor->mousePos)) {
                if (compat::ngRequireObject(io)->KeyShift) return int(SelectActionID::ToggleSelect);
                if (!compat::ngRequireObject(editor)->isSelected(editor->vtxAtMouse)) return int(SelectActionID::SelectOne);
                return int(SelectActionID::MaybeSelectOne);
            }
            return int(SelectActionID::SelectArea);
        }
        if (!isDragging && !compat::ngRequireObject(editor)->isSelecting &&
            input::ngInputIsMouseReleased(ImGuiMouseButton_Left) &&
            compat::ngRequireObject(editor)->maybeSelectOne != UINT64_MAX)
            return int(SelectActionID::SelectMaybeSelectOne);
        if (dragging() && !compat::ngRequireObject(editor)->isSelecting) return int(SelectActionID::StartDrag);
    }
    return int(SelectActionID::None);
}
std::int32_t BrushTool::unify(compat::Slice<std::int32_t> requests) {
    struct PriorityHash {
        std::size_t operator()(std::int32_t value) const noexcept { return static_cast<std::size_t>(value); }
    };
    runtime::common::ResourceKeyMap<std::int32_t, std::int32_t,
        PriorityHash, std::equal_to<std::int32_t>> priorities;
    priorities[Drawing] = 0;
    priorities[int(SelectActionID::None)] = 10;
    priorities[int(SelectActionID::SelectArea)] = 5;
    priorities[int(SelectActionID::ToggleSelect)] = 2;
    priorities[int(SelectActionID::SelectOne)] = 2;
    priorities[int(SelectActionID::MaybeSelectOne)] = 2;
    priorities[int(SelectActionID::StartDrag)] = 2;
    priorities[int(SelectActionID::SelectMaybeSelectOne)] = 2;
    int selected = int(SelectActionID::None), current = priorities.at(selected);
    for (auto value : requests) {
        const auto found = priorities.find(value);
        if (found == priorities.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/common/mesheditor/tools/brush.d"), 264);
        const auto next = found->second;
        if (next < current) {
            selected = value;
            current = next;
        }
    }
    return selected;
}
bool BrushTool::update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) {
    const auto keepAlive = shared_from_this(); changed = false;
    const auto tip = []<std::size_t N, std::size_t K>(const char (&action)[N], const char (&key)[K]) {
        const auto actionText = text(action);
        const auto keyText = text(key);
        widgets::ngStatusTooltip(actionText, keyText);
    };
    tip("Deform", "Left Mouse");
    tip("Snap to X/Y axis", "SHIFT");
    tip("Select vertices", "ALT");
    if (action == int(SelectActionID::ToggleSelect)) { if (compat::ngRequireObject(editor)->vtxAtMouse != UINT64_MAX) editor->toggleSelect(editor->vtxAtMouse); }
    else if (action == int(SelectActionID::SelectOne)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != UINT64_MAX) editor->selectOne(editor->vtxAtMouse);
        else compat::ngRequireObject(editor)->deselectAll();
    } else if (action == int(SelectActionID::MaybeSelectOne)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != UINT64_MAX)
            editor->maybeSelectOne = editor->vtxAtMouse;
    } else if (action == int(SelectActionID::SelectArea)) {
        const auto origin = compat::ngRequireObject(editor)->mousePos;
        editor->selectOrigin = origin;
        editor->isSelecting = true;
    }
    if (action == int(SelectActionID::SelectMaybeSelectOne) &&
        compat::ngRequireObject(editor)->maybeSelectOne != UINT64_MAX)
        editor->selectOne(editor->maybeSelectOne);
    if (action == int(SelectActionID::StartDrag))
        onDragStart(compat::ngRequireObject(editor)->mousePos, editor);
    if (action == Drawing) {
        if (compat::ngRequireObject(io)->KeyShift) {
            static thread_local int threshold = 32;
            if (axisDirection == 0) {
                const auto delta = subtract(compat::ngRequireObject(editor)->mousePos, initPos);
                if (std::abs(delta.x) / viewport::base::viewportZoom > threshold) axisDirection = 1;
                else if (std::abs(delta.y) / viewport::base::viewportZoom > threshold) axisDirection = 2;
            }
            if (axisDirection == 1) {
                if (!editor) compat::ngFailNullAccess(true);
                editor->mousePos.y = initPos.y;
            } else if (axisDirection == 2) {
                if (!editor) compat::ngFailNullAccess(true);
                editor->mousePos.x = initPos.x;
            }
        }
        if (flow) {
            const auto receiver = compat::ngRequireObject(editor);
            const auto position = compat::ngRequireObject(editor)->mousePos;
            const auto brush = currentBrush();
            weights = receiver->getVerticesInBrush(position, brush);
        }
        const auto currentPosition = compat::ngRequireObject(editor)->mousePos;
        const auto delta = subtract(currentPosition, compat::ngRequireObject(editor)->lastMousePos);
        auto selected = compat::ngRequireObject(editor)->selected;
        if (selected.empty()) {
            const auto count = weights.size();
            selected = {};
            selected.resize(count);
            for (std::size_t i = 0; i < count; ++i) selected[i] = i;
        }
        for (auto index : selected) {
            const float weight = sourceAt(weights, index, 326);
            const auto vertices = compat::ngRequireObject(editor)->getVerticesByIndex({index});
            if (auto vertex = sourceAt(vertices, 0, 327); vertex && weight > 0) {
                const auto vertexOwner = core::math::mesh::ngTryRetainMeshVertex(vertex);
                registerDraggedVertex(index, vertex);
                compat::ngRequireObject(editor)->moveMeshVertex(vertex,
                    {vertex->position.x + delta.x * weight, vertex->position.y + delta.y * weight});
                compat::ngRequireObject(editor)->markActionDirty();
            }
        }
        compat::ngRequireObject(editor)->refreshMesh(); changed = true;
    } else if (isDragging) onDragUpdate(compat::ngRequireObject(editor)->mousePos, editor);
    if (applyOptimizationIfReady(editor)) changed = true;
    if (changed) compat::ngRequireObject(editor)->refreshMesh(); return changed;
}
void BrushTool::registerDraggedVertex(std::uint64_t index, core::math::mesh::MeshVertex* vertex) {
    if (!dragStartPositions.contains(index)) {
        dragStartPositions[index] = compat::ngRequireObject(vertex)->position;
        draggedVertices.append(index);
    }
}
void BrushTool::initializeStrokeOptimization(EditorRef editor) {
    const auto keepAlive = shared_from_this(); strokeVertices.resize(0);
    const auto indices = draggedVertices;
    for (auto index : indices) {
        const auto vertices = compat::ngRequireObject(editor)->getVerticesByIndex({index});
        auto vertex = sourceAt(vertices, 0, 362); if (!vertex) continue;
        const auto found = dragStartPositions.find(index);
        const auto start = found == dragStartPositions.end() ? vertex->position : found->second;
        const auto end = vertex->position; if (length(subtract(end, start)) <= OptimizationMoveEpsilon) continue;
        strokeVertices.append({vertex, start, end, core::math::mesh::ngTryRetainMeshVertex(vertex)});
    }
    waitingForResult = false; pendingSampleParameter = 1; lastResultSerial = core::ngDifferenceAggregationResultSerial; activeRegionValid = false;
    if (strokeVertices.empty()) { optimizePending = false; resetDragTracking(); return; }
    bestScores.resize(strokeVertices.size(), std::numeric_limits<double>::quiet_NaN());
    bestParameters.resize(strokeVertices.size(), std::numeric_limits<float>::quiet_NaN());
    for (std::size_t i = 0; i < strokeVertices.size(); ++i) { bestScores[i] = infinity; bestParameters[i] = 1; }
    int viewportWidth = 0, viewportHeight = 0;
    runtime::inGetViewport(viewportWidth, viewportHeight);
    activeRegion = runtime::inGetDifferenceAggregationRegion(); activeRegionValid = activeRegion.width > 0 && activeRegion.height > 0;
    activeViewportWidth = viewportWidth;
    activeViewportHeight = viewportHeight;
    currentStride = OptimizationInitialStride; currentRangeMin = 0; currentRangeMax = 1;
    bestAggregateScore = stageBestScore = lastSampleAggregateScore = infinity;
    bestAggregateParameter = stageBestParameter = 1; configureOptimizationStage();
    waitingForResult = false; anyVertexAdjusted = false; optimizePending = true;
}
void BrushTool::configureOptimizationStage() {
    const auto keepAlive = shared_from_this(); currentStageSampleCount = computeStageSampleCount(currentRangeMin, currentRangeMax, currentStride);
    if (!currentStageSampleCount) currentStageSampleCount = 1;
    currentSampleIndex = 0; stageBestScore = infinity; stageBestParameter = computeSampleParameter(0);
}
std::size_t BrushTool::computeStageSampleCount(float minimum, float maximum, float stride) const {
    if (stride <= OptimizationParameterEpsilon) return 1;
    const float clampedMin = clamp(minimum, 0.f, 1.f);
    const float clampedMax = clamp(maximum, 0.f, 1.f);
    const float span = clampedMax - clampedMin;
    if (span <= OptimizationParameterEpsilon) return 1;
    const auto steps = sizeCast(std::ceil(double(span) / double(stride))) + 1;
    return steps < 2 ? 2 : steps;
}
float BrushTool::computeSampleParameter(std::size_t index) const {
    if (currentStageSampleCount <= 1) return clamp(currentRangeMin, 0.f, 1.f);
    const float minimum = clamp(currentRangeMin, 0.f, 1.f), maximum = clamp(currentRangeMax, 0.f, 1.f);
    float parameter = minimum + currentStride * float(index);
    if (index >= currentStageSampleCount - 1 || parameter >= maximum - OptimizationParameterEpsilon) parameter = maximum;
    if (parameter < minimum) parameter = minimum; if (parameter > maximum) parameter = maximum; return parameter;
}
double BrushTool::computeLocalContributionWeight() const {
    if (currentStride <= OptimizationFinalStrideThreshold + OptimizationParameterEpsilon) return 1;
    if (OptimizationInitialStride <= OptimizationParameterEpsilon) return 1;
    double ratio = 1 - double(currentStride) / double(OptimizationInitialStride);
    if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1; return ratio;
}
void BrushTool::setStrokeParameter(EditorRef editor, float parameter) {
    const auto keepAlive = shared_from_this(); const auto strokes = strokeVertices;
    for (const auto& stroke : strokes) {
        const auto candidate = interpolate(stroke.startPos, stroke.endPos, parameter);
        compat::ngRequireObject(editor)->moveMeshVertex(stroke.vertex, candidate);
    }
}
double BrushTool::updateScoresForSample(EditorRef editor) {
    const auto keepAlive = shared_from_this(); const double global = computeGlobalDifferenceMetric();
    std::array<double, TileCount> tiles; tiles.fill(NAN); bool anyFinite = false;
    if (activeRegionValid) for (int i = 0; i < TileCount; ++i) {
        const double count = core::ngDifferenceAggregationResult.tileCounts[i];
        if (count > 0) { tiles[i] = core::ngDifferenceAggregationResult.tileTotals[i] / count; anyFinite = true; }
        else tiles[i] = infinity;
    }
    const auto combined = computeCombinedMatrix(editor);
    const auto cameraOwner = runtime::inGetCameraHandle();
    const auto camera = compat::ngRequireObject(cameraOwner)->matrix();
    const double localWeight = computeLocalContributionWeight(); double sum = 0; std::size_t count = 0;
    constexpr double epsilon = 1e-9; const auto strokes = strokeVertices;
    for (std::size_t i = 0; i < strokes.size(); ++i) {
        const auto* vertex = strokes[i].vertex;
        if (!vertex) compat::ngFailNullAccess(false);
        const auto position = vertex->position;
        double local = infinity;
        if (activeRegionValid && anyFinite) local = sampleDifferenceAt(position, combined, camera, tiles);
        double blended = infinity; const bool hasLocal = std::isfinite(local), hasGlobal = std::isfinite(global);
        if (localWeight >= 1 - epsilon && hasLocal) blended = local;
        else if (localWeight <= epsilon || !hasLocal) blended = global;
        else if (hasLocal && hasGlobal) blended = global * (1 - localWeight) + local * localWeight;
        else if (hasLocal) blended = local; else blended = global;
        if (!std::isfinite(blended)) continue;
        if (!std::isfinite(sourceAt(bestScores, i, 535)) ||
                blended <= sourceAt(bestScores, i, 535) - epsilon) {
            sourceAt(bestScores, i, 536) = blended;
            sourceAt(bestParameters, i, 537) = pendingSampleParameter;
        }
        sum += blended; ++count;
    }
    return count == 0 ? global : sum / double(count);
}
void BrushTool::applyBestParameters(EditorRef editor) {
    const auto keepAlive = shared_from_this(); bool adjusted = false; const bool fallback = std::isfinite(bestAggregateScore);
    const float fallbackParameter = clamp(bestAggregateParameter, 0.f, 1.f); const auto strokes = strokeVertices;
    for (std::size_t i = 0; i < strokes.size(); ++i) {
        const auto& data = strokes[i]; float parameter = 1;
        if (i < bestParameters.size() && i < bestScores.size() && std::isfinite(bestScores[i])) parameter = clamp(bestParameters[i], 0.f, 1.f);
        else if (fallback) parameter = fallbackParameter;
        const auto candidate = interpolate(data.startPos, data.endPos, parameter);
        if (length(subtract(candidate, data.endPos)) > OptimizationMoveEpsilon) adjusted = true;
        compat::ngRequireObject(editor)->moveMeshVertex(data.vertex, candidate);
    }
    anyVertexAdjusted = adjusted;
}
BrushTool::Mat4 BrushTool::computeCombinedMatrix(EditorRef editor) {
    const auto keepAlive = shared_from_this();
    const auto baseMatrix = compat::ngRequireObject(editor)->transform;
    const auto target = compat::ngRequireObject(editor)->getTarget();
    if (!target) return baseMatrix;
    auto puppetMatrix = Mat4::identity();
    if (const auto puppet = target->puppetRef()) {
        puppetMatrix = puppet->transform.matrix();
        if (const auto part = std::dynamic_pointer_cast<runtime::nodes::Part>(target); part && part->ignorePuppet) puppetMatrix = Mat4::identity();
    }
    return puppetMatrix * baseMatrix;
}
double BrushTool::sampleDifferenceAt(Vec2 position, Mat4 combined, Mat4 camera, std::array<double, TileCount>& tiles) const {
    if (!activeRegionValid || activeViewportWidth <= 0 || activeViewportHeight <= 0) return infinity;
    const auto clip = multiply(camera, multiply(combined, {position.x, position.y, 0, 1}));
    if (std::fabs(clip.w) < 1e-6f) return infinity;
    const double ndcX = clip.x / clip.w, ndcY = clip.y / clip.w;
    const double px = (ndcX * .5 + .5) * activeViewportWidth, py = (ndcY * .5 + .5) * activeViewportHeight;
    const double x = px - activeRegion.x, y = py - activeRegion.y;
    if (x < 0 || y < 0 || x >= activeRegion.width || y >= activeRegion.height) return infinity;
    const double invWidth = activeRegion.width > 0 ? double(TileColumns) / activeRegion.width : 0;
    const double invHeight = activeRegion.height > 0 ? double(TileColumns) / activeRegion.height : 0;
    const double gridX = x * invWidth, gridY = y * invHeight;
    const int x0 = clamp(compat::ngFloatToInt32(std::floor(gridX)), 0, TileColumns - 1);
    const int y0 = clamp(compat::ngFloatToInt32(std::floor(gridY)), 0, TileColumns - 1);
    const int x1 = x0 < TileColumns - 1 ? x0 + 1 : x0, y1 = y0 < TileColumns - 1 ? y0 + 1 : y0;
    double tx = gridX - x0, ty = gridY - y0; if (x1 == x0) tx = 0; if (y1 == y0) ty = 0;
    double sum = 0, weightSum = 0;
    const auto accumulate = [&](int ix, int iy, double weight) {
        if (weight <= 0) return; const double value = tiles[std::size_t(iy) * TileColumns + ix];
        if (!std::isfinite(value)) return; sum += value * weight; weightSum += weight;
    };
    accumulate(x0, y0, (1 - tx) * (1 - ty)); accumulate(x1, y0, tx * (1 - ty));
    accumulate(x0, y1, (1 - tx) * ty); accumulate(x1, y1, tx * ty);
    return weightSum > 0 ? sum / weightSum : infinity;
}
double BrushTool::computeGlobalDifferenceMetric() const {
    if (!core::ngDifferenceAggregationResultValid) return infinity;
    const auto& result = core::ngDifferenceAggregationResult;
    if (result.alpha > 0) return (result.red + result.green + result.blue) / result.alpha;
    double sum = 0, weights = 0;
    for (double value : result.tileTotals) sum += value;
    for (double value : result.tileCounts) weights += value;
    return weights > 0 ? sum / weights : infinity;
}
void BrushTool::finalizeOptimization(EditorRef editor) {
    const auto keepAlive = shared_from_this();
    applyBestParameters(editor);
    compat::ngRequireObject(editor)->refreshMesh();
    if (anyVertexAdjusted) compat::ngRequireObject(editor)->markActionDirty();
    resetDragTracking();
}
bool BrushTool::applyOptimizationIfReady(EditorRef editor) {
    const auto keepAlive = shared_from_this(); if (!optimizePending) return false;
    if (!brushstate::ngBrushHasTeacherPart() || strokeVertices.empty()) { resetDragTracking(); return false; }
    if (!baselineValid && core::ngDifferenceAggregationResultValid) captureBaselineDifference();
    if (!activeRegionValid) {
        int width = 0, height = 0; runtime::inGetViewport(width, height); activeRegion = runtime::inGetDifferenceAggregationRegion();
        activeRegionValid = activeRegion.width > 0 && activeRegion.height > 0;
        if (activeRegionValid) { activeViewportWidth = width; activeViewportHeight = height; }
    }
    if (waitingForResult) {
        if (core::ngDifferenceAggregationResultValid && core::ngDifferenceAggregationResultSerial != lastResultSerial) {
            lastResultSerial = core::ngDifferenceAggregationResultSerial; activeRegion = runtime::inGetDifferenceAggregationRegion();
            activeRegionValid = activeRegion.width > 0 && activeRegion.height > 0;
            const double aggregate = updateScoresForSample(editor); lastSampleAggregateScore = aggregate;
            if (std::isfinite(aggregate)) {
                if (!std::isfinite(stageBestScore) || aggregate < stageBestScore) { stageBestScore = aggregate; stageBestParameter = pendingSampleParameter; }
                if (!std::isfinite(bestAggregateScore) || aggregate < bestAggregateScore) { bestAggregateScore = aggregate; bestAggregateParameter = pendingSampleParameter; }
            }
            waitingForResult = false;
        }
        return false;
    }
    if (currentSampleIndex >= currentStageSampleCount || currentStageSampleCount == 0) {
        float pivot = stageBestParameter;
        if (!std::isfinite(stageBestScore) && std::isfinite(bestAggregateScore)) pivot = bestAggregateParameter;
        pivot = clamp(pivot, 0.f, 1.f);
        if (currentStride <= OptimizationFinalStrideThreshold + OptimizationParameterEpsilon || currentStageSampleCount <= 1) {
            finalizeOptimization(editor); return true;
        }
        currentRangeMin = clamp(pivot - currentStride, 0.f, 1.f); currentRangeMax = clamp(pivot + currentStride, 0.f, 1.f);
        currentStride *= .5f;
        if (currentRangeMax - currentRangeMin <= OptimizationParameterEpsilon) currentRangeMin = currentRangeMax = pivot;
        configureOptimizationStage();
    }
    if (currentSampleIndex < currentStageSampleCount) {
        pendingSampleParameter = computeSampleParameter(currentSampleIndex++); setStrokeParameter(editor, pendingSampleParameter);
        lastResultSerial = core::ngDifferenceAggregationResultSerial;
        compat::ngRequireObject(editor)->refreshMesh(); waitingForResult = true; return true;
    }
    finalizeOptimization(editor); return true;
}
void BrushTool::dbgDrawDifferenceDiagnostics(EditorRef) {
    const auto keepAlive = shared_from_this();
    if (!baselineValid && core::ngDifferenceAggregationResultValid) captureBaselineDifference();
    auto region = runtime::inGetDifferenceAggregationRegion();
    int width = 0, height = 0;
    runtime::inGetViewport(width, height);
    if (region.width <= 0 || region.height <= 0) region = {0, 0, width, height};
    if (baselineTileValues.size() < TileCount) return;
    if (activeViewportWidth > 0) width = activeViewportWidth; else if (width <= 0) width = region.width;
    if (activeViewportHeight > 0) height = activeViewportHeight; else if (height <= 0) height = region.height;
    if (width <= 0 || height <= 0) return;
    const float tileWidth = float(region.width) / TileColumns, tileHeight = float(region.height) / TileColumns;
    if (tileWidth <= 0 || tileHeight <= 0) return;
    const auto camera = runtime::inGetCameraHandle();
    const auto inverse = core::math::source_matrix::inverse(
        compat::ngRequireObject(camera)->matrix());
    const double global = core::ngDifferenceAggregationResultValid ? computeGlobalDifferenceMetric() : baselineGlobalScore;
    runtime::math::Vec3Array improved, worsened, unchanged;
    for (int ty = 0; ty < TileColumns; ++ty) for (int tx = 0; tx < TileColumns; ++tx) {
        const std::size_t index = std::size_t(ty) * TileColumns + tx;
        double baseline = index < baselineTileValues.size() ? baselineTileValues[index] : baselineGlobalScore;
        if (!std::isfinite(baseline)) baseline = baselineGlobalScore;
        double current;
        if (core::ngDifferenceAggregationResultValid) {
            const double count = core::ngDifferenceAggregationResult.tileCounts[index];
            current = count > 0 ? core::ngDifferenceAggregationResult.tileTotals[index] / count : global;
        } else current = baseline;
        if (!std::isfinite(current)) current = global;
        if (!std::isfinite(baseline)) baseline = baselineGlobalScore;
        if (!std::isfinite(current)) current = 0;
        if (!std::isfinite(baseline)) baseline = 0;
        const double delta = current - baseline;
        const float centerX = region.x + (tx + .5f) * tileWidth, centerY = region.y + (ty + .5f) * tileHeight;
        auto world = multiply(inverse, {(centerX / width) * 2.f - 1.f, (centerY / height) * 2.f - 1.f, 0, 1});
        if (world.w != 0) { const auto w = world.w; world.x /= w; world.y /= w; world.z /= w; world.w /= w; }
        const runtime::math::Vec3 center{world.x, world.y, world.z};
        if (delta < -1e-6) improved.append(center);
        else if (delta > 1e-6) worsened.append(center);
        else unchanged.append(center);
    }
    core::dbg::inDbgPointsSize(10);
    if (improved.size()) { core::dbg::inDbgSetBuffer(improved); core::dbg::inDbgDrawPoints({0, 1, 0, .8f}); }
    if (worsened.size()) { core::dbg::inDbgSetBuffer(worsened); core::dbg::inDbgDrawPoints({1, 0, 0, .8f}); }
    if (unchanged.size()) { core::dbg::inDbgSetBuffer(unchanged); core::dbg::inDbgDrawPoints({.6f, .6f, .6f, .8f}); }
}
void BrushTool::draw(base::CameraRef camera, EditorRef editor) {
    const auto keepAlive = shared_from_this(); NodeSelect::draw(camera, editor);
    if (!ImGui::GetIO().KeyAlt) {
        const auto brush = compat::ngRequireObject(currentBrush());
        const auto position = compat::ngRequireObject(editor)->mousePos;
        const auto transform = compat::ngRequireObject(editor)->transform;
        brush->draw(position, transform);
    }
    // The authored diagnostic call remains disabled.
}
namespace {
void drawTeacherTargetOption(std::shared_ptr<BrushTool> tool) {
    if (!tool) return;
    const auto teacher = brushstate::ngBrushGetTeacherPart();
    ImGui::SameLine(0, 4); ImGui::PushID("BRUSH_TEACHER_TARGET");
    if (teacher && !teacher->textures.empty() && teacher->textures[0]) {
        widgets::ngTextureSlotUntitled("TeacherPreview", teacher->textures[0], ImVec2(64, 64), 32, 0, false);
    } else {
        auto background = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg); background.w = .15f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
        ImGui::BeginChild("Teacher Part", ImVec2(64, 32), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const auto label = sourceCString(text("Teacher"));
        ImGui::Text(label.data());
        ImGui::EndChild(); ImGui::PopStyleColor();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const auto payload = ImGui::AcceptDragDropPayload("_PUPPETNTREE")) {
            if (payload->Data) {
                runtime::nodes::Node* raw;
                std::memcpy(&raw, payload->Data, sizeof(raw));
                if (raw) if (auto part = std::dynamic_pointer_cast<runtime::nodes::Part>(raw->shared_from_this()))
                    brushstate::ngBrushSetTeacherPart(std::move(part));
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::SameLine(0, 0);
    if (ImGui::BeginPopupContextItem("TeacherTargetContext", ImGuiPopupFlags_MouseButtonRight)) {
        ImGui::SameLine(0, 4);
        const auto label = core::translation::ngTranslateCString("Clear");
        if (ImGui::MenuItem(label.data())) brushstate::ngBrushClearTeacherPart();
        ImGui::EndPopup();
    }
    ImGui::PopID();
}
}
bool detail::displayOptions(base::Editors editors) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    std::shared_ptr<BrushTool> tool;
    if (!editors.empty()) {
        const auto values = sourceEditorValues(editors);
        const auto editor = compat::ngArrayAt(values, 0);
        tool = std::dynamic_pointer_cast<BrushTool>(
            compat::ngRequireObject(editor)->getTool());
    }
    ImGui::BeginGroup();
    if (widgets::ngButtonColored("\xee\x8e\x9e", ImVec2(0, 0), tool && !tool->getFlow() ? widgets::colorUndefined : ImVec4(.6f, .6f, .6f, 1))) {
        for (const auto& entry : editors) {
            const auto editor = entry.second;
            if (const auto brush = std::dynamic_pointer_cast<BrushTool>(
                    compat::ngRequireObject(editor)->getTool())) brush->setFlow(false);
        }
    }
    widgets::ngTooltip(compat::RetainedStringSlice(text("Drag mode"))); ImGui::SameLine(0, 0);
    if (widgets::ngButtonColored("\xee\x8e\xa2", ImVec2(0, 0), tool && tool->getFlow() ? widgets::colorUndefined : ImVec4(.6f, .6f, .6f, 1))) {
        for (const auto& entry : editors) {
            const auto editor = entry.second;
            if (const auto brush = std::dynamic_pointer_cast<BrushTool>(
                    compat::ngRequireObject(editor)->getTool())) brush->setFlow(true);
        }
    }
    widgets::ngTooltip(compat::RetainedStringSlice(text("Flow mode"))); ImGui::EndGroup();
    ImGui::SameLine(0, 4); compat::ngRequireObject(currentBrush())->configure(); ImGui::SameLine(0, 4);
    ImGui::BeginGroup(); ImGui::PushID("BRUSH_SELECT");
    const auto name = compat::ngRequireObject(currentBrush())->name();
    const auto nameZ = sourceCString(name);
    if (ImGui::BeginCombo("###Brushes", nameZ.data())) {
        const auto brushes = brushes::ngBrushList();
        for (auto brush : brushes) {
            const auto label = compat::ngRequireObject(brush)->name();
            const auto labelZ = sourceCString(label);
            if (ImGui::Selectable(labelZ.data())) setCurrentBrush(brush);
        }
        ImGui::EndCombo();
    }
    ImGui::PopID(); ImGui::EndGroup(); ImGui::PopStyleVar(2);
    ImGui::SameLine(0, 4); drawTeacherTargetOption(tool); return false;
}
}
