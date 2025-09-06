module nijigenerate.viewport.vertex.automesh.contours;

import i18n;
import nijigenerate.viewport.vertex.automesh.automesh;
import nijigenerate.viewport.common.mesh;
import nijigenerate.widgets;
import nijilive.core;
import inmath;
import nijigenerate.core.cv; // dcv系から置換
import mir.ndslice;
import mir.math.stat: mean;
import std.algorithm;
import std.algorithm.iteration: map, reduce;
//import std.stdio;
import std.array;
import bindbc.imgui;
import nijigenerate.viewport.vertex.automesh.alpha_provider;
import nijigenerate.viewport.vertex.automesh.common;
import nijigenerate.project : incSelectedNodes;

class ContourAutoMeshProcessor : AutoMeshProcessor {
    float SAMPLING_STEP = 32;
    const float SMALL_THRESHOLD = 256;
    float maskThreshold = 15;
    float MIN_DISTANCE = 16;
    float MAX_DISTANCE = -1;
    float[] SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1];
    string presetName;
    // Unified alpha preview state
    private AlphaPreviewState _alphaPreview;
    public:
    // 不要な単回利用のヘルパーは削除し、autoMesh 本体で処理します。

    override IncMesh autoMesh(Drawable target, IncMesh mesh, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0) {
        if (MAX_DISTANCE < 0) MAX_DISTANCE = SAMPLING_STEP * 2;
        auto ai = getAlphaInput(target);
        if (ai.w == 0 || ai.h == 0 || ai.img is null) return mesh;

        auto imbin = ai.img.sliced[0 .. $, 0 .. $, 3];
        foreach (y; 0 .. imbin.shape[0])
        foreach (x; 0 .. imbin.shape[1])
            imbin[y, x] = imbin[y, x] < cast(ubyte)maskThreshold ? 0 : 255;

        mesh.clear();
        vec2 imgCenter = vec2(ai.w / 2, ai.h / 2);

        vec2i[][] foundContours;
        ContourHierarchy[] hierarchy;
        findContours(imbin, foundContours, hierarchy, RetrievalMode.EXTERNAL, ApproximationMethod.SIMPLE);

        auto contoursToVec2s(ContourType)(ref ContourType contours) {
            vec2[] result;
            foreach (p; contours) result ~= vec2(p.x, p.y);
            return result;
        }
        auto calcMoment(vec2[] contour) {
            auto moment = contour.reduce!((a, b) { return a + b; })();
            return moment / contour.length;
        }
        auto scaling(vec2[] contour, vec2 moment, float scale, int erode_dilate) {
            return contour.map!((c) { return (c - moment) * scale + moment; }).array;
        }
        auto resampling(vec2[] contour, double rate, bool mirrorHoriz_, float axisHoriz_, bool mirrorVert_, float axisVert_) {
            vec2[] sampled;
            ulong base = 0;
            if (mirrorHoriz_) {
                float minDistance = -1;
                foreach (i, vertex; contour) {
                    if (minDistance < 0 || vertex.x - axisHoriz_ < minDistance) {
                        base = i;
                        minDistance = vertex.x - axisHoriz_;
                    }
                }
            }
            sampled ~= contour[base];
            float side = 0;
            foreach (idx; 1 .. contour.length) {
                vec2 prev = sampled[$ - 1];
                vec2 c = contour[(idx + base) % contour.length];
                if ((c - prev).lengthSquared > rate * rate) {
                    if (mirrorHoriz_) {
                        if (side == 0) side = sign(c.x - axisHoriz_);
                        else if (sign(c.x - axisHoriz_) != side) continue;
                    }
                    sampled ~= c;
                }
            }
            return sampled;
        }

        foreach (contour; foundContours) {
            auto contourVec = contoursToVec2s(contour);
            if (contourVec.length == 0) continue;
            float[] scales = SCALES;
            auto moment = calcMoment(contourVec);
            if (MAX_DISTANCE < 0) MAX_DISTANCE = SAMPLING_STEP * 2;
            foreach (double scale; scales) {
                double samplingRate = SAMPLING_STEP;
                samplingRate = min(MAX_DISTANCE / scale, scale > 0 ? samplingRate / (scale * scale) : 1);
                auto contour2 = resampling(contourVec, samplingRate, mirrorHoriz, imgCenter.x + axisHoriz, mirrorVert, imgCenter.y + axisVert);
                auto contour3 = scaling(contour2, moment, scale, 0);
                if (mirrorHoriz) {
                    auto flipped = contour3.map!((a) { return vec2(imgCenter.x + axisHoriz - (a.x - imgCenter.x - axisHoriz), a.y); })();
                    foreach (f; flipped) {
                        auto scaledContourVec = scaling(contourVec, moment, scale, 0);
                        auto index = scaledContourVec.map!((a) { return (a - f).lengthSquared; }).minIndex();
                        contour3 ~= scaledContourVec[index];
                    }
                }
                foreach (vec2 c; contour3) {
                    if (mesh.vertices.length > 0) {
                        auto last = mesh.vertices[$ - 1].position + imgCenter;
                        if ((last - c).lengthSquared < MIN_DISTANCE * MIN_DISTANCE) continue;
                    }
                    mesh.vertices ~= new MeshVertex(c - imgCenter);
                }
            }
        }

        auto outMesh = mesh.autoTriangulate();
        mapImageCenteredMeshToTargetLocal(outMesh, target, ai);
        return outMesh;
    }

    /// 任意ノード配列を対象にプロジェクションAからメッシュ化（Mask/Shape 含む）
    IncMesh autoMesh(Node[] targets, IncMesh mesh, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0)
    {
        auto provider = new GenericProjectionAlphaProvider(targets);
        scope(exit) provider.dispose();
        auto ai = alphaInputFromProviderWithImage(provider);
        if (ai.w == 0 || ai.h == 0 || ai.img is null) return mesh;

        auto imbin = ai.img.sliced[0 .. $, 0 .. $, 3];
        foreach (y; 0 .. imbin.shape[0])
        foreach (x; 0 .. imbin.shape[1])
            imbin[y, x] = imbin[y, x] < cast(ubyte)maskThreshold ? 0 : 255;

        mesh.clear();
        vec2 imgCenter = vec2(ai.w / 2, ai.h / 2);

        vec2i[][] foundContours;
        ContourHierarchy[] hierarchy;
        findContours(imbin, foundContours, hierarchy, RetrievalMode.EXTERNAL, ApproximationMethod.SIMPLE);

        auto contoursToVec2s(ContourType)(ref ContourType contours) {
            vec2[] result;
            foreach (p; contours) result ~= vec2(p.x, p.y);
            return result;
        }
        auto calcMoment(vec2[] contour) {
            auto moment = contour.reduce!((a, b) { return a + b; })();
            return moment / contour.length;
        }
        auto scaling(vec2[] contour, vec2 moment, float scale, int erode_dilate) {
            return contour.map!((c) { return (c - moment) * scale + moment; }).array;
        }
        auto resampling(vec2[] contour, double rate, bool mirrorHoriz_, float axisHoriz_, bool mirrorVert_, float axisVert_) {
            vec2[] sampled;
            ulong base = 0;
            if (mirrorHoriz_) {
                float minDistance = -1;
                foreach (i, vertex; contour) {
                    if (minDistance < 0 || vertex.x - axisHoriz_ < minDistance) {
                        base = i;
                        minDistance = vertex.x - axisHoriz_;
                    }
                }
            }
            sampled ~= contour[base];
            float side = 0;
            foreach (idx; 1 .. contour.length) {
                vec2 prev = sampled[$ - 1];
                vec2 c = contour[(idx + base) % contour.length];
                if ((c - prev).lengthSquared > rate * rate) {
                    if (mirrorHoriz_) {
                        if (side == 0) side = sign(c.x - axisHoriz_);
                        else if (sign(c.x - axisHoriz_) != side) continue;
                    }
                    sampled ~= c;
                }
            }
            return sampled;
        }

        foreach (contour; foundContours) {
            auto contourVec = contoursToVec2s(contour);
            if (contourVec.length == 0) continue;
            float[] scales = SCALES;
            auto moment = calcMoment(contourVec);
            if (MAX_DISTANCE < 0) MAX_DISTANCE = SAMPLING_STEP * 2;
            foreach (double scale; scales) {
                double samplingRate = SAMPLING_STEP;
                samplingRate = min(MAX_DISTANCE / scale, scale > 0 ? samplingRate / (scale * scale) : 1);
                auto contour2 = resampling(contourVec, samplingRate, mirrorHoriz, imgCenter.x + axisHoriz, mirrorVert, imgCenter.y + axisVert);
                auto contour3 = scaling(contour2, moment, scale, 0);
                if (mirrorHoriz) {
                    auto flipped = contour3.map!((a) { return vec2(imgCenter.x + axisHoriz - (a.x - imgCenter.x - axisHoriz), a.y); })();
                    foreach (f; flipped) {
                        auto scaledContourVec = scaling(contourVec, moment, scale, 0);
                        auto index = scaledContourVec.map!((a) { return (a - f).lengthSquared; }).minIndex();
                        contour3 ~= scaledContourVec[index];
                    }
                }
                foreach (vec2 c; contour3) {
                    if (mesh.vertices.length > 0) {
                        auto last = mesh.vertices[$ - 1].position + imgCenter;
                        if ((last - c).lengthSquared < MIN_DISTANCE * MIN_DISTANCE) continue;
                    }
                    mesh.vertices ~= new MeshVertex(c - imgCenter);
                }
            }
        }

        return mesh.autoTriangulate();
    }
    override void configure() {
        if (MAX_DISTANCE < 0)
            MAX_DISTANCE = SAMPLING_STEP * 2;
        if (!presetName) {
            presetName = "Normal parts";
        }
        incText(_("Presets"));
        igIndent();
        if(igBeginCombo(__("Presets"), __(presetName))) {
            if (igSelectable(__("Normal parts"))) {
                presetName = "Normal parts";
                SAMPLING_STEP = 50;
                maskThreshold = 15;
                MIN_DISTANCE = 16;
                MAX_DISTANCE = SAMPLING_STEP * 2;
                SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
            }
            if (igSelectable(__("Detailed mesh"))) {
                presetName = "Detailed mesh";
                SAMPLING_STEP = 32;
                maskThreshold = 15;
                MIN_DISTANCE = 16;
                MAX_DISTANCE = SAMPLING_STEP * 2;
                SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
            }
            if (igSelectable(__("Large parts"))) {
                presetName = "Large parts";
                SAMPLING_STEP = 80;
                maskThreshold = 15;
                MIN_DISTANCE = 24;
                MAX_DISTANCE = SAMPLING_STEP * 2;
                SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
            }
            if (igSelectable(__("Small parts"))) {
                presetName = "Small parts";
                SAMPLING_STEP = 24;
                maskThreshold = 15;
                MIN_DISTANCE = 12;
                MAX_DISTANCE = SAMPLING_STEP * 2;
                SCALES = [1, 1.1, 0.6, 0.2];
            }
            if (igSelectable(__("Thin and minimum parts"))) {
                presetName = "Thin and minimum parts";
                SAMPLING_STEP = 12;
                maskThreshold = 1;
                MIN_DISTANCE = 4;
                MAX_DISTANCE = SAMPLING_STEP * 2;
                SCALES = [1];
            }
            if (igSelectable(__("Preserve edges"))) {
                presetName = "Preserve edges";
                SAMPLING_STEP = 24;
                maskThreshold = 15;
                MIN_DISTANCE = 8;
                MAX_DISTANCE = SAMPLING_STEP * 2;
                SCALES = [1, 1.2, 0.8];
            }
            igEndCombo();
        }
        igUnindent();
        igPushID("CONFIGURE_OPTIONS");
        if (incBeginCategory(__("Details"))) {
            if (igBeginChild("###CONTOUR_OPTIONS", ImVec2(0, 320))) {
                incText(_("Sampling rate"));
                igIndent();
                igPushID("SAMPLING_STEP");
                    igSetNextItemWidth(64);
                    if (incDragFloat(
                        "sampling_rate", &SAMPLING_STEP, 1,
                        1, 200, "%.2f", ImGuiSliderFlags.NoRoundToFormat)
                    ) {
                        SAMPLING_STEP = SAMPLING_STEP;
                        if (MAX_DISTANCE < SAMPLING_STEP)
                            MAX_DISTANCE  = SAMPLING_STEP * 2;
                    }
                igPopID();
                igUnindent();
                incText(_("Mask threshold"));
                igIndent();
                igPushID("MASK_THRESHOLD");
                    igSetNextItemWidth(64);
                    if (incDragFloat(
                        "mask_threshold", &maskThreshold, 1,
                        1, 200, "%.2f", ImGuiSliderFlags.NoRoundToFormat)
                    ) {
                        maskThreshold = maskThreshold;
                    }
                igPopID();
                igUnindent();
                incText(_("Distance between vertices"));
                igIndent();
                    incText(_("Minimum"));
                    igIndent();
                        igPushID("MIN_DISTANCE");
                            igSetNextItemWidth(64);
                            if (incDragFloat(
                                "min_distance", &MIN_DISTANCE, 1,
                                1, 200, "%.2f", ImGuiSliderFlags.NoRoundToFormat)
                            ) {
                                MIN_DISTANCE = MIN_DISTANCE;
                            }
                        igPopID();
                    igUnindent();
                    incText(_("Maximum"));
                    igIndent();
                        igPushID("MAX_DISTANCE");
                            igSetNextItemWidth(64);
                            if (incDragFloat(
                                "min_distance", &MAX_DISTANCE, 1,
                                1, 200, "%.2f", ImGuiSliderFlags.NoRoundToFormat)
                            ) {
                                MAX_DISTANCE = MAX_DISTANCE;
                            }
                        igPopID();
                    igUnindent();
                igUnindent();
                int deleteIndex = -1;
                incText("Scales");
                igIndent();
                    igPushID("SCALES");
                        if (igBeginChild("###AXIS_ADJ", ImVec2(0, 240))) {
                            if (SCALES.length > 0) {
                                int ix;
                                foreach(i, ref pt; SCALES) {
                                    ix++;
                                    vec2 range = vec2(0, 2);
                                    igSetNextItemWidth(80);
                                    igPushID(cast(int)i);
                                        if (incDragFloat(
                                            "adj_offset", &SCALES[i], 0.01,
                                            range.x, range.y, "%.2f", ImGuiSliderFlags.NoRoundToFormat)
                                        ) {
                                        }
                                        igSameLine(0, 0);
                                        if (i == SCALES.length - 1) {
                                            incDummy(ImVec2(-52, 32));
                                            igSameLine(0, 0);
                                            if (incButtonColored("", ImVec2(24, 24))) {
                                                deleteIndex = cast(int)i;
                                            }
                                            igSameLine(0, 0);
                                            if (incButtonColored("", ImVec2(24, 24))) {
                                                SCALES ~= 1.0;
                                            }
                                        } else {
                                            incDummy(ImVec2(-28, 32));
                                            igSameLine(0, 0);
                                            if (incButtonColored("", ImVec2(24, 24))) {
                                                deleteIndex = cast(int)i;
                                            }
                                        }
                                    igPopID();
                                }
                            } else {
                                incDummy(ImVec2(-28, 24));
                                igSameLine(0, 0);
                                if (incButtonColored("", ImVec2(24, 24))) {
                                    SCALES ~= 1.0;
                                }
                            }
                        }
                        igEndChild();
                    igPopID();
                igUnindent();
                incTooltip(_("Specifying scaling factor to apply for contours. If multiple scales are specified, vertices are populated per scale factors."));
                if (deleteIndex != -1) {
                    SCALES = SCALES.remove(cast(uint)deleteIndex);
                }
            }
            igEndChild();
        }
        incEndCategory();
        igPopID();

        igSeparator();
        incText(_("Alpha Preview"));
        igIndent();
        alphaPreviewWidget(_alphaPreview, ImVec2(192, 192));
        igUnindent();
    }
    override 
    string icon() {
        return "";
    }
};
