module nijigenerate.viewport.vertex.automesh.grid;

import i18n;
import nijigenerate.viewport.vertex.automesh.automesh;
import nijigenerate.viewport.common.mesh;
import nijigenerate.widgets;
import nijilive.core;
import inmath;
import dcv.core;
import dcv.imgproc;
import dcv.measure;
import mir.ndslice;
import mir.math.stat: mean;
import std.algorithm;
import std.algorithm.iteration: map, reduce;
import std.stdio;
import std.array;
import bindbc.imgui;

class GridAutoMeshProcessor : AutoMeshProcessor {
    float[] ScaleX = [-0.1, 0.0, 0.5, 1.0, 1.1];
    float[] ScaleY = [-0.1, 0.0, 0.5, 1.0, 1.1];
    float maskThreshold = 15;
    float xSegments = 2, ySegments = 2;
    float margin = 0.1;
public:
    override
    IncMesh autoMesh(const Drawable target, const IncMesh mesh, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0) const {
        Part part = cast(Part)target;
        if (!part)
            return new IncMesh(mesh);

        Texture texture = part.textures[0];
        if (!texture)
            return new IncMesh(mesh);
        ubyte[] data = texture.getTextureData();
        auto img = new Image(texture.width, texture.height, ImageFormat.IF_RGB_ALPHA);
        copy(data, img.data);
        
        auto gray = img.sliced[0..$, 0..$, 3]; // Use transparent channel for boundary search
        auto imbin = gray;
        foreach (y; 0..imbin.shape[0]) {
            foreach (x; 0..imbin.shape[1]) {
                imbin[y, x] = imbin[y, x] < cast(ubyte)maskThreshold? 0: 255;
            }
        }
        vec2 imgCenter = vec2(texture.width / 2, texture.height / 2);
        IncMesh newMesh = new IncMesh(mesh);
        newMesh.clear();

        float minX = texture.width();
        float minY = texture.height();
        float maxX = 0;
        float maxY = 0;
        foreach (y; 0..imbin.shape[0]) {
            foreach (x; 0..imbin.shape[1]) {
                if (imbin[y, x] > 0) {
                    minX = min(x, minX);
                    minY = min(y, minY);
                    maxX = max(x, maxX);
                    maxY = max(y, maxY);
                }
            }
        }

        auto dcomposite = cast(DynamicComposite)target;
        MeshData meshData;
        auto scaleY = ScaleY.dup;
        auto scaleX = ScaleX.dup;
        newMesh.axes = [[], []];
        scaleY.sort!((a, b)=> a<b);
        foreach (y; scaleY) {
            newMesh.axes[0] ~= (minY * y + maxY * (1 - y)) - imgCenter.y;
            if (dcomposite !is null) {
                newMesh.axes[0][$ - 1] += dcomposite.textureOffset.y;
            }
        }
        scaleX.sort!((a, b)=> a<b);
        foreach (x; scaleX) {
            newMesh.axes[1] ~= (minX * x + maxX * (1 - x)) - imgCenter.x;
            if (dcomposite !is null) {
                newMesh.axes[1][$ - 1] += dcomposite.textureOffset.x;
            }
        }
        meshData.gridAxes = newMesh.axes[];
        meshData.regenerateGrid();
        newMesh.copyFromMeshData(meshData);

        return newMesh;
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
            ScaleY.length = 0;
            ScaleX.length = 0;
            if (margin != 0) {
                ScaleY ~= -margin;
                ScaleX ~= -margin;
            }
            foreach (y; 0..(ySegments+1)) {
                ScaleY ~= y / ySegments;
            }
            foreach (x; 0..(xSegments+1)) {                
                ScaleX ~= x / xSegments;
            }
            if (margin != 0) {
                ScaleY ~= 1 + margin;
                ScaleX ~= 1 + margin;
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
                editScale(ScaleX);
            }
            incEndCategory();
            if (incBeginCategory(__("Y Scale"))) {
                editScale(ScaleY);
            }
            incEndCategory();
        igPopID();

    }

    override
    string icon() {
        return "";
    }
};