module nijigenerate.viewport.vertex.automesh.grid;

import i18n;
import nijigenerate.viewport.vertex.automesh.automesh;
import nijigenerate.viewport.common.mesh;
import nijigenerate.widgets;
import nijilive.core;
import inmath;
import nijigenerate.core.cv;
import mir.ndslice;
import mir.math.stat: mean;
import std.algorithm;
import std.algorithm.iteration: map, reduce;
//import std.stdio;
import std.array;
import bindbc.imgui;
import nijigenerate.viewport.vertex.automesh.alpha_provider;
import nijigenerate.viewport.vertex.automesh.common : getAlphaInput, alphaImageCenter, mapImageCenteredMeshToTargetLocal;

class GridAutoMeshProcessor : AutoMeshProcessor {
    float[] scaleX = [-0.1, 0.0, 0.5, 1.0, 1.1];
    float[] scaleY = [-0.1, 0.0, 0.5, 1.0, 1.1];
    float maskThreshold = 15;
    float xSegments = 2, ySegments = 2;
    float margin = 0.1;
    // Unified alpha preview state
    private AlphaPreviewState _alphaPreview;
public:
    override
    IncMesh autoMesh(Drawable target, IncMesh mesh, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0) {
        // 1) 最初の処理だけ分岐: AlphaInput の取得
        auto ai = getAlphaInput(target);
        if (ai.w <= 0 || ai.h <= 0) return mesh;

        // 2) 共通処理: マスクの二値化と外接矩形の算出
        auto imbin = ai.img.sliced[0 .. $, 0 .. $, 3];
        foreach (y; 0 .. imbin.shape[0])
        foreach (x; 0 .. imbin.shape[1])
            imbin[y, x] = imbin[y, x] < cast(ubyte)maskThreshold ? 0 : 255;

        int minX = ai.w, minY = ai.h, maxX = -1, maxY = -1;
        foreach (y; 0 .. imbin.shape[0])
        foreach (x; 0 .. imbin.shape[1])
            if (imbin[y, x] > 0) {
                int xi = cast(int)x;
                int yi = cast(int)y;
                if (xi < minX) minX = xi;
                if (yi < minY) minY = yi;
                if (xi > maxX) maxX = xi;
                if (yi > maxY) maxY = yi;
            }
        if (maxX < 0 || maxY < 0) return mesh; // マスク無し

        // 3) 共通処理: グリッド軸作成（画像中心基準）
        mesh.clear();
        vec2 imgCenter = alphaImageCenter(ai);

        MeshData meshData;
        mesh.axes = [[], []];

        scaleY.sort!((a, b) => a < b);
        foreach (y; scaleY)
            mesh.axes[0] ~= (minY * y + maxY * (1 - y)) - imgCenter.y;

        scaleX.sort!((a, b) => a < b);
        foreach (x; scaleX)
            mesh.axes[1] ~= (minX * x + maxX * (1 - x)) - imgCenter.x;

        meshData.gridAxes = mesh.axes[];
        meshData.regenerateGrid();
        mesh.copyFromMeshData(meshData);

        // 4) 共通処理: 対象ローカル座標へマッピング
        mapImageCenteredMeshToTargetLocal(mesh, target, ai);
        return mesh;
    }

    override
    void configure() {
        void editScale(ref float[] scales) {
            int deleteIndex = -1;
            if (igBeginChild("###AXIS_ADJ", ImVec2(0, 240))) {
                if (scales.length > 0) {
                    int ix;
                    foreach(i, ref pt; scales) {
                        ix++;

                        // Do not allow existing points to cross over
                        vec2 range = vec2(0, 2);

                        igSetNextItemWidth(80);
                        igPushID(cast(int)i);
                            if (incDragFloat(
                                "adj_offset", &scales[i], 0.01,
                                range.x, range.y, "%.2f", ImGuiSliderFlags.NoRoundToFormat)
                            ) {
                                // do something
                            }
                            igSameLine(0, 0);

                            if (i == scales.length - 1) {
                                incDummy(ImVec2(-52, 32));
                                igSameLine(0, 0);
                                if (incButtonColored("", ImVec2(24, 24))) {
                                    deleteIndex = cast(int)i;
                                }
                                igSameLine(0, 0);
                                if (incButtonColored("", ImVec2(24, 24))) {
                                    scales ~= 1.0;
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
                        scales ~= 1.0;
                    }
                }
            }
            igEndChild();
            if (deleteIndex != -1) {
                scales = scales.remove(cast(uint)deleteIndex);
            }
        }

        void divideAxes() {
            scaleY.length = 0;
            scaleX.length = 0;
            if (margin != 0) {
                scaleY ~= -margin;
                scaleX ~= -margin;
            }
            foreach (y; 0..(ySegments+1)) {
                scaleY ~= y / ySegments;
            }
            foreach (x; 0..(xSegments+1)) {                
                scaleX ~= x / xSegments;
            }
            if (margin != 0) {
                scaleY ~= 1 + margin;
                scaleX ~= 1 + margin;
            }
        }

        igPushID("CONFIGURE_OPTIONS");

            if (incBeginCategory(__("Auto Segmentation"))) {
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

                incText(_("X Segments"));
                igIndent();
                    igPushID("XSEGMENTS");
                        igSetNextItemWidth(64);
                        if (incDragFloat(
                            "x_segments", &xSegments, 1,
                            2, 20, "%.0f", ImGuiSliderFlags.NoRoundToFormat)
                        ) {
                            divideAxes();
                        }
                    igPopID();
                igUnindent();

                incText(_("Y Segments"));
                igIndent();
                    igPushID("YSEGMENTS");
                        igSetNextItemWidth(64);
                        if (incDragFloat(
                            "y_segments", &ySegments, 1,
                            2, 20, "%.0f", ImGuiSliderFlags.NoRoundToFormat)
                        ) {
                            divideAxes();
                        }
                    igPopID();
                igUnindent();

                incText(_("Margin"));
                igIndent();
                    igPushID("MARGIN");
                        igSetNextItemWidth(64);
                        if (incDragFloat(
                            "margin", &margin, 0.1,
                            0, 1, "%.2f", ImGuiSliderFlags.NoRoundToFormat)
                        ) {
                            divideAxes();
                        }
                    igPopID();
                igUnindent();
            }
            incEndCategory();

            if (incBeginCategory(__("X Scale"))) {
                editScale(scaleX);
            }
            incEndCategory();
            if (incBeginCategory(__("Y Scale"))) {
                editScale(scaleY);
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
        return "";
    }
};
