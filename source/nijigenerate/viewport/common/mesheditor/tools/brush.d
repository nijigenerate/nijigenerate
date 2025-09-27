module nijigenerate.viewport.common.mesheditor.tools.brush;

import nijigenerate.viewport.common.mesheditor.tools.enums;
import nijigenerate.viewport.common.mesheditor.tools.base;
import nijigenerate.viewport.common.mesheditor.tools.select;
import nijigenerate.viewport.common.mesheditor.operations;
import nijigenerate.viewport.common.mesheditor.brushes;
import i18n;
import nijigenerate.viewport.base;
import nijigenerate.viewport.common;
import nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.spline;
import nijigenerate.viewport.common.mesheditor.brushstate;
import nijigenerate.core.input;
import nijigenerate.core.actionstack;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate.widgets.texture;
import nijigenerate;
import nijilive;
import nijilive.core.dbg;
import nijilive.core.diff_collect : DifferenceEvaluationRegion;
import nijilive.core : inGetDifferenceAggregationRegion;
import bindbc.opengl;
import bindbc.imgui;
import std.algorithm.mutation;
import std.algorithm.searching;
//import std.stdio;
import std.string;
import std.math : floor, ceil, isFinite, fabs;
import std.algorithm : clamp;
import std.array : array;
import std.format : format;
import std.range : iota;
import std.stdio : stderr, writefln;
import nijigenerate.core.dpi : incGetUIScale;
import nijigenerate.core.window :
    ngDifferenceAggregationResult,
    ngDifferenceAggregationResultValid,
    ngDifferenceAggregationResultSerial;


enum TileColumns = 16;
enum TileCount = TileColumns * TileColumns;
enum float OptimizationInitialStride = 0.1f;
enum float OptimizationFinalStrideThreshold = 0.01f;
enum float OptimizationParameterEpsilon = 1e-6f;
enum float OptimizationMoveEpsilon = 1e-4f;

private struct StrokeVertex {
    MeshVertex* vertex;
    vec2 startPos;
    vec2 endPos;
}

private {
    Brush _currentBrush;
    Brush currentBrush() {
        if (_currentBrush is null) {
            _currentBrush = incBrushList()[0];
        }
        return _currentBrush;
    }
    void setCurrentBrush(Brush brush) {
        if (brush !is null)
            _currentBrush = brush;
    }
}

class BrushTool : NodeSelect {
    bool flow = false;
    float[] weights;
    vec2 initPos;
    int axisDirection; // 0: none, 1: lock to horizontal move only, 2: lock to vertical move only

    vec2[ulong] dragStartPositions;
    ulong[] draggedVertices;
    bool optimizePending;

    StrokeVertex[] strokeVertices;
    bool waitingForResult;
    float pendingSampleParameter;
    ulong lastResultSerial;
    double[] bestScores;
    float[] bestParameters;
    DifferenceEvaluationRegion activeRegion;
    bool activeRegionValid;
    int activeViewportWidth;
    int activeViewportHeight;
    size_t currentSampleIndex;
    size_t currentStageSampleCount;
    float currentStride;
    float currentRangeMin;
    float currentRangeMax;
    double stageBestScore;
    float stageBestParameter;
    double bestAggregateScore;
    float bestAggregateParameter;
    double lastSampleAggregateScore;
    bool anyVertexAdjusted;
    double[] baselineTileValues;
    double baselineGlobalScore = double.infinity;
    bool baselineValid;

    bool getFlow() { return flow; }
    void setFlow(bool value) { flow = value; }

    void resetDragTracking() {
        dragStartPositions = typeof(dragStartPositions).init;
        draggedVertices.length = 0;
        strokeVertices.length = 0;
        waitingForResult = false;
        pendingSampleParameter = 1.0f;
        lastResultSerial = ngDifferenceAggregationResultSerial;
        bestScores.length = 0;
        bestParameters.length = 0;
        activeRegionValid = false;
        activeViewportWidth = 0;
        activeViewportHeight = 0;
        activeRegion = DifferenceEvaluationRegion.init;
        currentSampleIndex = 0;
        currentStageSampleCount = 0;
        currentStride = 0;
        currentRangeMin = 0;
        currentRangeMax = 1;
        stageBestScore = double.infinity;
        stageBestParameter = 1.0f;
        bestAggregateScore = double.infinity;
        bestAggregateParameter = 1.0f;
        lastSampleAggregateScore = double.infinity;
        anyVertexAdjusted = false;
        optimizePending = false;
        baselineTileValues.length = 0;
        baselineGlobalScore = double.infinity;
        baselineValid = false;
    }

    void captureBaselineDifference() {
        baselineTileValues.length = TileCount;
        if (ngDifferenceAggregationResultValid) {
            foreach (i; 0 .. TileCount) {
                double count = ngDifferenceAggregationResult.tileCounts[i];
                double value = count > 0 ? ngDifferenceAggregationResult.tileTotals[i] / count : double.infinity;
                baselineTileValues[i] = value;
            }
            baselineGlobalScore = computeGlobalDifferenceMetric();
        } else {
            foreach (i; 0 .. TileCount) {
                baselineTileValues[i] = 0.0;
            }
            baselineGlobalScore = 0.0;
        }
        baselineValid = true;
    }

    override bool onDragStart(vec2 mousePos, IncMeshEditorOne impl) {
        bool started = super.onDragStart(mousePos, impl);
        if (started) {
            resetDragTracking();
            captureBaselineDifference();
            if (!flow)
                weights = impl.getVerticesInBrush(impl.mousePos, currentBrush);
            initPos = impl.mousePos;
            axisDirection = 0;
        }
        return started;
    }

    override bool onDragEnd(vec2 mousePos, IncMeshEditorOne impl) {
        bool result = super.onDragEnd(mousePos, impl);
        if (draggedVertices.length > 0 && incBrushHasTeacherPart()) {
            initializeStrokeOptimization(impl);
        } else {
            resetDragTracking();
        }
        return result;
    }

    override bool onDragUpdate(vec2 mousePos, IncMeshEditorOne impl) {
        return super.onDragUpdate(mousePos, impl);
    }


    enum BrushActionID {
        Drawing = cast(int)(SelectActionID.End),
        End
    }

    override
    void setToolMode(VertexToolMode toolMode, IncMeshEditorOne impl) {
        super.setToolMode(toolMode, impl);
        incViewport.alwaysUpdate = true;
    }

    override 
    int peek(ImGuiIO* io, IncMeshEditorOne impl) {
        super.peek(io, impl);

        if (incInputIsMouseReleased(ImGuiMouseButton.Left)) {
            onDragEnd(impl.mousePos, impl);
        }

        if (igIsMouseClicked(ImGuiMouseButton.Left)) impl.maybeSelectOne = ulong(-1);
        
        int action = SelectActionID.None;

        if (!isDragging && !impl.isSelecting && 
            incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
            isDragging = true;
            onDragStart(impl.mousePos, impl);
        }

        if (isDragging) {
            action = BrushActionID.Drawing;
        }

        if (action != SelectActionID.None)
            return action;

        if (io.KeyAlt) {
            // Left click selection
            if (igIsMouseClicked(ImGuiMouseButton.Left)) {
                if (impl.isPointOver(impl.mousePos)) {
                    if (io.KeyShift) return SelectActionID.ToggleSelect;
                    else if (!impl.isSelected(impl.vtxAtMouse))  return SelectActionID.SelectOne;
                    else return SelectActionID.MaybeSelectOne;
                } else {
                    return SelectActionID.SelectArea;
                }
            }
            if (!isDragging && !impl.isSelecting &&
                incInputIsMouseReleased(ImGuiMouseButton.Left) && impl.maybeSelectOne != ulong(-1)) {
                return SelectActionID.SelectMaybeSelectOne;
            }

            // Dragging
            if (incDragStartedInViewport(ImGuiMouseButton.Left) && igIsMouseDown(ImGuiMouseButton.Left) && incInputIsDragRequested(ImGuiMouseButton.Left)) {
                if (!impl.isSelecting) {
                    return SelectActionID.StartDrag;
                }
            }
        }

        return SelectActionID.None;

    }

    override
    int unify(int[] actions) {
        int[int] priorities;
        priorities[BrushActionID.Drawing] = 0;
        priorities[SelectActionID.None]                 = 10;
        priorities[SelectActionID.SelectArea]           = 5;
        priorities[SelectActionID.ToggleSelect]         = 2;
        priorities[SelectActionID.SelectOne]            = 2;
        priorities[SelectActionID.MaybeSelectOne]       = 2;
        priorities[SelectActionID.StartDrag]            = 2;
        priorities[SelectActionID.SelectMaybeSelectOne] = 2;

        int action = SelectActionID.None;
        int curPriority = priorities[action];
        foreach (a; actions) {
            auto newPriority = priorities[a];
            if (newPriority < curPriority) {
                curPriority = newPriority;
                action = a;
            }
        }
        return action;
    }

    override 
    bool update(ImGuiIO* io, IncMeshEditorOne impl, int action, out bool changed) {
        incStatusTooltip(_("Deform"), _("Left Mouse"));
        incStatusTooltip(_("Snap to X/Y axis"), _("SHIFT"));
        incStatusTooltip(_("Select vertices"), _("ALT"));

        // Left click selection
        if (action == SelectActionID.ToggleSelect) {
            if (impl.vtxAtMouse != ulong(-1))
                impl.toggleSelect(impl.vtxAtMouse);
        } else if (action == SelectActionID.SelectOne) {
            if (impl.vtxAtMouse != ulong(-1))
                impl.selectOne(impl.vtxAtMouse);
            else
                impl.deselectAll();
        } else if (action == SelectActionID.MaybeSelectOne) {
            if (impl.vtxAtMouse != ulong(-1))
                impl.maybeSelectOne = impl.vtxAtMouse;
        } else if (action == SelectActionID.SelectArea) {
            impl.selectOrigin = impl.mousePos;
            impl.isSelecting = true;
        }

        if (action == SelectActionID.SelectMaybeSelectOne) {
            if (impl.maybeSelectOne != ulong(-1))
                impl.selectOne(impl.maybeSelectOne);
        }

        // Dragging
        if (action == SelectActionID.StartDrag) {
            onDragStart(impl.mousePos, impl);
        }

        if (action == BrushActionID.Drawing) {
            if (io.KeyShift) {
                static int THRESHOLD = 32;
                if (axisDirection == 0) {
                    vec2 diffToInit = impl.mousePos - initPos;
                    if (abs(diffToInit.x) / incViewportZoom > THRESHOLD)
                        axisDirection = 1;
                    else if (abs(diffToInit.y) / incViewportZoom > THRESHOLD)
                        axisDirection = 2;
                }
                if (axisDirection == 1)
                    impl.mousePos.y = initPos.y;
                else if (axisDirection == 2)
                    impl.mousePos.x = initPos.x;
            }
            if (flow)
                weights = impl.getVerticesInBrush(impl.mousePos, currentBrush);
            auto diffPos = impl.mousePos - impl.lastMousePos;
            ulong[] selected = (impl.selected && impl.selected.length > 0)? impl.selected: array(iota(weights.length));
            foreach (idx; selected) {
                float weight = weights[idx];
                MeshVertex* v = impl.getVerticesByIndex([idx])[0];
                if (v is null)
                    continue;
                if (weight > 0) {
                    registerDraggedVertex(idx, v);
                    impl.moveMeshVertex(v, v.position + diffPos * weight);
                    impl.markActionDirty();
                }
            }

            impl.refreshMesh();
            changed = true;
        } else if (isDragging)
            onDragUpdate(impl.mousePos, impl);

        bool optimizationChanged = applyOptimizationIfReady(impl);

        if (optimizationChanged) {
            changed = true;
        }

        if (changed) impl.refreshMesh();
        return changed;
    }

    void registerDraggedVertex(ulong index, MeshVertex* vertex) {
        if (index !in dragStartPositions) {
            dragStartPositions[index] = vertex.position;
            draggedVertices ~= index;
        }
    }

    void initializeStrokeOptimization(IncMeshEditorOne impl) {
        strokeVertices.length = 0;
        foreach (idx; draggedVertices) {
            MeshVertex* vertex = impl.getVerticesByIndex([idx])[0];
            if (vertex is null)
                continue;

            vec2* startPtr = idx in dragStartPositions;
            vec2 startPos = startPtr ? *startPtr : vertex.position;
            vec2 endPos = vertex.position;
            if ((endPos - startPos).length <= OptimizationMoveEpsilon) {
                continue;
            }

            strokeVertices ~= StrokeVertex(vertex, startPos, endPos);
        }

        waitingForResult = false;
        pendingSampleParameter = 1.0f;
        lastResultSerial = ngDifferenceAggregationResultSerial;
        activeRegionValid = false;

        if (strokeVertices.length == 0) {
            optimizePending = false;
            resetDragTracking();
            return;
        }

        bestScores.length = strokeVertices.length;
        bestParameters.length = strokeVertices.length;
        foreach (i; 0 .. strokeVertices.length) {
            bestScores[i] = double.infinity;
            bestParameters[i] = 1.0f;
        }

        int viewportWidth, viewportHeight;
        inGetViewport(viewportWidth, viewportHeight);
        activeRegion = inGetDifferenceAggregationRegion();
        activeRegionValid = activeRegion.width > 0 && activeRegion.height > 0;
        activeViewportWidth = viewportWidth;
        activeViewportHeight = viewportHeight;

        currentStride = OptimizationInitialStride;
        currentRangeMin = 0.0f;
        currentRangeMax = 1.0f;
        bestAggregateScore = double.infinity;
        bestAggregateParameter = 1.0f;
        stageBestScore = double.infinity;
        stageBestParameter = 1.0f;
        lastSampleAggregateScore = double.infinity;
        configureOptimizationStage();

        waitingForResult = false;
        anyVertexAdjusted = false;
        optimizePending = true;
    }

    void configureOptimizationStage() {
        currentStageSampleCount = computeStageSampleCount(currentRangeMin, currentRangeMax, currentStride);
        if (currentStageSampleCount == 0) {
            currentStageSampleCount = 1;
        }
        currentSampleIndex = 0;
        stageBestScore = double.infinity;
        stageBestParameter = computeSampleParameter(0);
    }

    size_t computeStageSampleCount(float rangeMin, float rangeMax, float stride) const {
        if (stride <= OptimizationParameterEpsilon) {
            return 1;
        }
        float clampedMin = clamp(rangeMin, 0.0f, 1.0f);
        float clampedMax = clamp(rangeMax, 0.0f, 1.0f);
        float span = clampedMax - clampedMin;
        if (span <= OptimizationParameterEpsilon) {
            return 1;
        }
        size_t steps = cast(size_t)ceil(cast(double)span / cast(double)stride) + 1;
        if (steps < 2) {
            steps = 2;
        }
        return steps;
    }

    float computeSampleParameter(size_t index) const {
        if (currentStageSampleCount <= 1) {
            return clamp(currentRangeMin, 0.0f, 1.0f);
        }
        float clampedMin = clamp(currentRangeMin, 0.0f, 1.0f);
        float clampedMax = clamp(currentRangeMax, 0.0f, 1.0f);
        float parameter = clampedMin + currentStride * cast(float)index;
        if (index >= currentStageSampleCount - 1 || parameter >= clampedMax - OptimizationParameterEpsilon) {
            parameter = clampedMax;
        }
        if (parameter < clampedMin) {
            parameter = clampedMin;
        }
        if (parameter > clampedMax) {
            parameter = clampedMax;
        }
        return parameter;
    }

    double computeLocalContributionWeight() const {
        if (currentStride <= OptimizationFinalStrideThreshold + OptimizationParameterEpsilon) {
            return 1.0;
        }
        if (OptimizationInitialStride <= OptimizationParameterEpsilon) {
            return 1.0;
        }
        double ratio = 1.0 - cast(double)currentStride / cast(double)OptimizationInitialStride;
        if (ratio < 0) ratio = 0;
        if (ratio > 1) ratio = 1;
        return ratio;
    }


    void setStrokeParameter(IncMeshEditorOne impl, float parameter) {
        foreach (ref data; strokeVertices) {
            vec2 candidate = data.startPos * (1 - parameter) + data.endPos * parameter;
            impl.moveMeshVertex(data.vertex, candidate);
        }
    }

    double updateScoresForSample(IncMeshEditorOne impl) {
        double globalScore = computeGlobalDifferenceMetric();

        double[TileCount] tileValues;
        bool anyFinite = false;
        if (activeRegionValid) {
            foreach (i; 0 .. TileCount) {
                double count = ngDifferenceAggregationResult.tileCounts[i];
                if (count > 0) {
                    tileValues[i] = ngDifferenceAggregationResult.tileTotals[i] / count;
                    anyFinite = true;
                } else {
                    tileValues[i] = double.infinity;
                }
            }
        }

        mat4 combinedMatrix = computeCombinedMatrix(impl);
        auto camera = inGetCamera();
        mat4 cameraMatrix = camera.matrix();

        double localWeight = computeLocalContributionWeight();
        double aggregateSum = 0;
        size_t aggregateCount = 0;
        const double scoreEpsilon = 1e-9;

        foreach (i, ref data; strokeVertices) {
            vec2 position = data.vertex.position;
            double localScore = double.infinity;
            if (activeRegionValid && anyFinite) {
                localScore = sampleDifferenceAt(position, combinedMatrix, cameraMatrix, tileValues);
            }

            double blendedScore = double.infinity;
            bool hasLocal = isFinite(localScore);
            bool hasGlobal = isFinite(globalScore);

            if (localWeight >= 1.0 - scoreEpsilon && hasLocal) {
                blendedScore = localScore;
            } else if (localWeight <= scoreEpsilon || !hasLocal) {
                blendedScore = globalScore;
            } else if (hasLocal && hasGlobal) {
                blendedScore = globalScore * (1.0 - localWeight) + localScore * localWeight;
            } else if (hasLocal) {
                blendedScore = localScore;
            } else {
                blendedScore = globalScore;
            }

            if (!isFinite(blendedScore))
                continue;

            if (!isFinite(bestScores[i]) || blendedScore <= bestScores[i] - scoreEpsilon) {
                bestScores[i] = blendedScore;
                bestParameters[i] = pendingSampleParameter;
            }

            aggregateSum += blendedScore;
            ++aggregateCount;
        }

        if (aggregateCount == 0) {
            return globalScore;
        }
        return aggregateSum / cast(double)aggregateCount;
    }

    void applyBestParameters(IncMeshEditorOne impl) {
        bool adjusted = false;
        bool hasFallback = isFinite(bestAggregateScore);
        float fallbackParameter = clamp(bestAggregateParameter, 0.0f, 1.0f);
        foreach (i, ref data; strokeVertices) {
            float parameter = 1.0f;
            if (i < bestParameters.length && i < bestScores.length && isFinite(bestScores[i])) {
                parameter = clamp(bestParameters[i], 0.0f, 1.0f);
            } else if (hasFallback) {
                parameter = fallbackParameter;
            }

            vec2 candidate = data.startPos * (1 - parameter) + data.endPos * parameter;
            if ((candidate - data.endPos).length > OptimizationMoveEpsilon) {
                adjusted = true;
            }
            impl.moveMeshVertex(data.vertex, candidate);
        }

        anyVertexAdjusted = adjusted;
    }

    mat4 computeCombinedMatrix(IncMeshEditorOne impl) {
        mat4 baseMatrix = impl.transform;
        Node targetNode = impl.getTarget();
        if (targetNode is null) {
            return baseMatrix;
        }

        mat4 puppetMatrix = mat4.identity;
        if (auto puppet = targetNode.puppet) {
            puppetMatrix = puppet.transform.matrix;
            if (auto part = cast(Part)targetNode) {
                if (part.ignorePuppet) {
                    puppetMatrix = mat4.identity;
                }
            }
        }

        return puppetMatrix * baseMatrix;
    }

    double sampleDifferenceAt(vec2 position, mat4 combinedMatrix, mat4 cameraMatrix, ref double[TileCount] tileValues) const {
        if (!activeRegionValid || activeViewportWidth <= 0 || activeViewportHeight <= 0) {
            return double.infinity;
        }

        vec4 clip = cameraMatrix * (combinedMatrix * vec4(position.x, position.y, 0, 1));
        if (fabs(clip.w) < 1e-6f) {
            return double.infinity;
        }

        double ndcX = clip.x / clip.w;
        double ndcY = clip.y / clip.w;
        double px = (ndcX * 0.5 + 0.5) * activeViewportWidth;
        double py = (ndcY * 0.5 + 0.5) * activeViewportHeight;

        double localX = px - activeRegion.x;
        double localY = py - activeRegion.y;

        if (localX < 0 || localY < 0 || localX >= activeRegion.width || localY >= activeRegion.height) {
            return double.infinity;
        }

        double invWidth = activeRegion.width > 0 ? cast(double)TileColumns / activeRegion.width : 0;
        double invHeight = activeRegion.height > 0 ? cast(double)TileColumns / activeRegion.height : 0;

        double gridX = localX * invWidth;
        double gridY = localY * invHeight;

        int x0 = cast(int)floor(gridX);
        int y0 = cast(int)floor(gridY);
        x0 = clamp(x0, 0, TileColumns - 1);
        y0 = clamp(y0, 0, TileColumns - 1);

        int x1 = x0 < TileColumns - 1 ? x0 + 1 : x0;
        int y1 = y0 < TileColumns - 1 ? y0 + 1 : y0;

        double tx = gridX - x0;
        double ty = gridY - y0;
        if (x1 == x0) tx = 0;
        if (y1 == y0) ty = 0;

        double accum = 0;
        double weightSum = 0;

        auto accumulate = (int ix, int iy, double weight) {
            if (weight <= 0) return;
            size_t idx = cast(size_t)iy * TileColumns + ix;
            double value = tileValues[idx];
            if (!isFinite(value)) return;
            accum += value * weight;
            weightSum += weight;
        };

        accumulate(x0, y0, (1 - tx) * (1 - ty));
        accumulate(x1, y0, tx * (1 - ty));
        accumulate(x0, y1, (1 - tx) * ty);
        accumulate(x1, y1, tx * ty);

        if (weightSum > 0) {
            return accum / weightSum;
        }
        return double.infinity;
    }

    double computeGlobalDifferenceMetric() const {
        if (!ngDifferenceAggregationResultValid)
            return double.infinity;

        if (ngDifferenceAggregationResult.alpha > 0) {
            return ngDifferenceAggregationResult.total / ngDifferenceAggregationResult.alpha;
        }
        double sumTotals = 0;
        double sumWeights = 0;
        foreach (value; ngDifferenceAggregationResult.tileTotals) {
            sumTotals += value;
        }
        foreach (value; ngDifferenceAggregationResult.tileCounts) {
            sumWeights += value;
        }

        if (sumWeights > 0) {
            return sumTotals / sumWeights;
        }
        return double.infinity;
    }

    void finalizeOptimization(IncMeshEditorOne impl) {
        applyBestParameters(impl);
        impl.refreshMesh();
        if (anyVertexAdjusted) {
            impl.markActionDirty();
        }
        resetDragTracking();
    }

    bool applyOptimizationIfReady(IncMeshEditorOne impl) {
        if (!optimizePending)
            return false;

        if (!incBrushHasTeacherPart() || strokeVertices.length == 0) {
            resetDragTracking();
            return false;
        }

        if (!baselineValid && ngDifferenceAggregationResultValid) {
            captureBaselineDifference();
        }

        if (!activeRegionValid) {
            int viewportWidth, viewportHeight;
            inGetViewport(viewportWidth, viewportHeight);
            activeRegion = inGetDifferenceAggregationRegion();
            activeRegionValid = activeRegion.width > 0 && activeRegion.height > 0;
            if (activeRegionValid) {
                activeViewportWidth = viewportWidth;
                activeViewportHeight = viewportHeight;
            }
        }

        if (waitingForResult) {
            if (ngDifferenceAggregationResultValid && ngDifferenceAggregationResultSerial != lastResultSerial) {
                lastResultSerial = ngDifferenceAggregationResultSerial;
                activeRegion = inGetDifferenceAggregationRegion();
                activeRegionValid = activeRegion.width > 0 && activeRegion.height > 0;
                double aggregate = updateScoresForSample(impl);
                lastSampleAggregateScore = aggregate;
                if (isFinite(aggregate)) {
                    if (!isFinite(stageBestScore) || aggregate < stageBestScore) {
                        stageBestScore = aggregate;
                        stageBestParameter = pendingSampleParameter;
                    }
                    if (!isFinite(bestAggregateScore) || aggregate < bestAggregateScore) {
                        bestAggregateScore = aggregate;
                        bestAggregateParameter = pendingSampleParameter;
                    }
                }
                waitingForResult = false;
            }
            return false;
        }

        if (currentSampleIndex >= currentStageSampleCount || currentStageSampleCount == 0) {
            float pivot = stageBestParameter;
            if (!isFinite(stageBestScore) && isFinite(bestAggregateScore)) {
                pivot = bestAggregateParameter;
            }
            pivot = clamp(pivot, 0.0f, 1.0f);

            if (currentStride <= OptimizationFinalStrideThreshold + OptimizationParameterEpsilon || currentStageSampleCount <= 1) {
                finalizeOptimization(impl);
                return true;
            }

            currentRangeMin = clamp(pivot - currentStride, 0.0f, 1.0f);
            currentRangeMax = clamp(pivot + currentStride, 0.0f, 1.0f);
            currentStride *= 0.5f;
            if (currentRangeMax - currentRangeMin <= OptimizationParameterEpsilon) {
                currentRangeMin = pivot;
                currentRangeMax = pivot;
            }
            configureOptimizationStage();
        }

        if (currentSampleIndex < currentStageSampleCount) {
            pendingSampleParameter = computeSampleParameter(currentSampleIndex++);
            setStrokeParameter(impl, pendingSampleParameter);
            lastResultSerial = ngDifferenceAggregationResultSerial;
            impl.refreshMesh();
            waitingForResult = true;
            return true;
        }

        finalizeOptimization(impl);
        return true;
    }

    void dbgDrawDifferenceDiagnostics(IncMeshEditorOne impl) {
        if (!baselineValid && ngDifferenceAggregationResultValid) {
            captureBaselineDifference();
        }

        auto region = inGetDifferenceAggregationRegion();
        int viewportPixelWidth;
        int viewportPixelHeight;
        inGetViewport(viewportPixelWidth, viewportPixelHeight);
        if (region.width <= 0 || region.height <= 0) {
            region = DifferenceEvaluationRegion(0, 0, viewportPixelWidth, viewportPixelHeight);
        }

        if (baselineTileValues.length < TileCount)
            return;

        if (activeViewportWidth > 0)
            viewportPixelWidth = activeViewportWidth;
        else if (viewportPixelWidth <= 0)
            viewportPixelWidth = region.width;

        if (activeViewportHeight > 0)
            viewportPixelHeight = activeViewportHeight;
        else if (viewportPixelHeight <= 0)
            viewportPixelHeight = region.height;

        if (viewportPixelWidth <= 0 || viewportPixelHeight <= 0)
            return;

        float tilePixelWidth = cast(float)region.width / TileColumns;
        float tilePixelHeight = cast(float)region.height / TileColumns;
        if (tilePixelWidth <= 0 || tilePixelHeight <= 0)
            return;

        auto camera = inGetCamera();
        mat4 viewProj = camera.matrix();
        mat4 invViewProj = viewProj.inverse();

        double currentGlobal = ngDifferenceAggregationResultValid ? computeGlobalDifferenceMetric() : baselineGlobalScore;
        const double epsilon = 1e-6;
        vec4 improveColor = vec4(0, 1, 0, 0.8f);
        vec4 worsenColor = vec4(1, 0, 0, 0.8f);
        vec4 unchangedColor = vec4(0.6f, 0.6f, 0.6f, 0.8f);

        vec3[] improvedPoints;
        vec3[] worsenedPoints;
        vec3[] unchangedPoints;

        foreach (int ty; 0 .. TileColumns) {
            foreach (int tx; 0 .. TileColumns) {
                size_t idx = cast(size_t)ty * TileColumns + tx;
                double baselineValue = idx < baselineTileValues.length ? baselineTileValues[idx] : baselineGlobalScore;
                if (!isFinite(baselineValue)) baselineValue = baselineGlobalScore;

                double currentValue;
                if (ngDifferenceAggregationResultValid) {
                    double count = ngDifferenceAggregationResult.tileCounts[idx];
                    currentValue = count > 0 ? ngDifferenceAggregationResult.tileTotals[idx] / count : currentGlobal;
                } else {
                    currentValue = baselineValue;
                }

                if (!isFinite(currentValue)) currentValue = currentGlobal;
                if (!isFinite(baselineValue)) baselineValue = baselineGlobalScore;
                if (!isFinite(currentValue)) currentValue = 0;
                if (!isFinite(baselineValue)) baselineValue = 0;

                double delta = currentValue - baselineValue;

                float centerPixelX = region.x + (tx + 0.5f) * tilePixelWidth;
                float centerPixelY = region.y + (ty + 0.5f) * tilePixelHeight;
                float clipX = (centerPixelX / viewportPixelWidth) * 2.0f - 1.0f;
                float clipY = (centerPixelY / viewportPixelHeight) * 2.0f - 1.0f;
                vec4 clipPos = vec4(clipX, clipY, 0, 1);
                vec4 worldPos = invViewProj * clipPos;
                if (worldPos.w != 0) {
                    worldPos /= worldPos.w;
                }
                vec3 worldCenter = worldPos.xyz;

                if (delta < -epsilon) {
                    improvedPoints ~= worldCenter;
                } else if (delta > epsilon) {
                    worsenedPoints ~= worldCenter;
                } else {
                    unchangedPoints ~= worldCenter;
                }
            }
        }

        inDbgPointsSize(10);
        if (improvedPoints.length) {
            inDbgSetBuffer(improvedPoints);
            inDbgDrawPoints(improveColor);
        }
        if (worsenedPoints.length) {
            inDbgSetBuffer(worsenedPoints);
            inDbgDrawPoints(worsenColor);
        }
        if (unchangedPoints.length) {
            inDbgSetBuffer(unchangedPoints);
            inDbgDrawPoints(unchangedColor);
        }
    }

    override
    void draw(Camera camera, IncMeshEditorOne impl) {
        super.draw(camera, impl);
        if (!(igGetIO().KeyAlt))
            currentBrush.draw(impl.mousePos, impl.transform);

//        dbgDrawDifferenceDiagnostics(impl);
    }

}

class ToolInfoImpl(T: BrushTool) : ToolInfoBase!(T) {
    override
    bool viewportTools(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) {
        if (deformOnly)
            return super.viewportTools(deformOnly, toolMode, editors);
        return false;
    }
    override bool canUse(bool deformOnly, Node[] targets) { return deformOnly; }
    override
    bool displayToolOptions(bool deformOnly, VertexToolMode toolMode, IncMeshEditorOne[Node] editors) {
        igPushStyleVar(ImGuiStyleVar.ItemSpacing, ImVec2(0, 0));
        igPushStyleVar(ImGuiStyleVar.WindowPadding, ImVec2(4, 4));
        auto brushTool = cast(BrushTool)(editors.length == 0 ? null: editors.values()[0].getTool());
            igBeginGroup();
                if (incButtonColored("", ImVec2(0, 0), (brushTool !is null && !brushTool.getFlow())? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) { // path definition
                    foreach (e; editors) {
                        auto bt = cast(BrushTool)(e.getTool());
                        if (bt)
                            bt.setFlow(false);
                    }
                }
                incTooltip(_("Drag mode"));

                igSameLine(0, 0);
                if (incButtonColored("", ImVec2(0, 0), (brushTool !is null && brushTool.getFlow())? colorUndefined : ImVec4(0.6, 0.6, 0.6, 1))) { // path definition
                    foreach (e; editors) {
                        auto bt = cast(BrushTool)(e.getTool());
                        if (bt)
                            bt.setFlow(true);
                    }
                }
                incTooltip(_("Flow mode"));

            igEndGroup();

            igSameLine(0, 4);
            currentBrush.configure();
            igSameLine(0, 4);

            igBeginGroup();
            igPushID("BRUSH_SELECT");
                auto brushName = currentBrush.name();
                if(igBeginCombo("###Brushes", brushName.toStringz)) {
                    foreach (brush; incBrushList) {
                        if (igSelectable(brush.name().toStringz)) {
                            setCurrentBrush(brush);
                        }
                    }
                    igEndCombo();
                }
            igPopID();

            igEndGroup();
        igPopStyleVar(2);

        igSameLine(0,4);
        drawTeacherTargetOption(brushTool);
        return false;
    }
    override VertexToolMode mode() { return VertexToolMode.Brush; };
    override string icon() { return "";}
    override string description() { return _("Brush Tool");}
}

private void drawTeacherTargetOption(BrushTool brushTool) {
    if (brushTool is null) return;

    Part teacher = incBrushGetTeacherPart();

    igSameLine(0, 4);
    ImVec2 previewSize = ImVec2(72, 72);
    igPushID("BRUSH_TEACHER_TARGET");
        if (teacher !is null && teacher.textures.length > 0 && teacher.textures[0]) {
            incTextureSlotUntitled("TeacherPreview", teacher.textures[0], previewSize, 32, ImGuiWindowFlags.None, false);
        } else {
            ImVec4 bg = *igGetStyleColorVec4(ImGuiCol.ChildBg);
            bg.w = 0.15f;
            igPushStyleColor(ImGuiCol.ChildBg, bg);
            igBeginChild("Teacher Part", previewSize, true, ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse | ImGuiWindowFlags.AlwaysUseWindowPadding);
                incText(_("Teacher Part"));
            igEndChild();
            igPopStyleColor();
        }

        if (igBeginDragDropTarget()) {
            const(ImGuiPayload)* payload = igAcceptDragDropPayload("_PUPPETNTREE");
            if (payload !is null) {
                if (Node* nodePtr = cast(Node*)payload.Data) {
                    if (Part part = cast(Part)(*nodePtr)) {
                        incBrushSetTeacherPart(part);
                    }
                }
            }
            igEndDragDropTarget();
        }

        igSameLine(0, 0);
        if (igBeginPopupContextItem("TeacherTargetContext", ImGuiPopupFlags.MouseButtonRight)) {
            igSameLine(0, 4);
            if (igMenuItem(__("Clear"))) {
                incBrushClearTeacherPart();
            }
            igEndPopup();
        }

    igPopID();
}










