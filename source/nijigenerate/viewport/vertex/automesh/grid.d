module nijigenerate.viewport.vertex.automesh.grid;

import i18n;
import nijigenerate.viewport.vertex.automesh.automesh;
import nijigenerate.viewport.vertex.automesh.meta;
import std.json : JSONValue, JSONType; // for JSON building
import nijigenerate.viewport.common.mesh;
import nijigenerate.widgets;
import nijilive.core;
import nijilive.core.nodes.deformer.grid : GridDeformer;
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

@AMProcessor("grid", "Grid", 200)
class GridAutoMeshProcessor : AutoMeshProcessor, IAutoMeshReflect {

    @AMParam(AutoMeshLevel.Advanced, "scale_x", "X Scale", "Axis scale factors (X)", "array")
    @AMArray(0, 2, 0.01)
    float[] scaleX = [-0.1, 0.0, 0.5, 1.0, 1.1];
    @AMParam(AutoMeshLevel.Advanced, "scale_y", "Y Scale", "Axis scale factors (Y)", "array")
    @AMArray(0, 2, 0.01)
    float[] scaleY = [-0.1, 0.0, 0.5, 1.0, 1.1];

    @AMParam(AutoMeshLevel.Simple, "mask_threshold", "Mask threshold", "Alpha binarize cutoff", "drag", 1, 200, 1)
    float maskThreshold = 15;
    @AMParam(AutoMeshLevel.Simple, "x_segments", "X Segments", "Grid density X", "drag", 2, 20, 1)
    float xSegments = 2;
    @AMParam(AutoMeshLevel.Simple, "y_segments", "Y Segments", "Grid density Y", "drag", 2, 20, 1)
    float ySegments = 2;
    @AMParam(AutoMeshLevel.Simple, "margin", "Margin", "Outer margin factor", "drag", 0, 1, 0.1)
    float margin = 0.1;
    // Unified alpha preview state
    private AlphaPreviewState _alphaPreview;
public:
    mixin AutoMeshClassInfo!();
    // Bring in unified reflection/UI
    mixin AutoMeshReflection!();
    private void rebuildScalesFromSegments() {
        scaleY.length = 0; scaleX.length = 0;
        int ys = cast(int)(ySegments < 1 ? 1 : ySegments);
        int xs = cast(int)(xSegments < 1 ? 1 : xSegments);
        if (margin != 0) { scaleY ~= -margin; scaleX ~= -margin; }
        foreach (y; 0 .. ys + 1) scaleY ~= cast(float)y / cast(float)ys;
        foreach (x; 0 .. xs + 1) scaleX ~= cast(float)x / cast(float)xs;
        if (margin != 0) { scaleY ~= 1 + margin; scaleX ~= 1 + margin; }
    }
    // Hook for mixin to react to param changes
    void ngPostParamWrite(string id) {
        if (id == "x_segments" || id == "y_segments" || id == "margin") {
            rebuildScalesFromSegments();
        }
    }

    private void rebuildGridMesh(IncMesh mesh, float minX, float maxX, float minY, float maxY, vec2 center) {
        mesh.clear();
        mesh.axes = [[], []];

        scaleY.sort!((a, b) => a < b);
        foreach (y; scaleY)
            mesh.axes[0] ~= (minY * (1 - y) + maxY * y) - center.y;

        scaleX.sort!((a, b) => a < b);
        foreach (x; scaleX)
            mesh.axes[1] ~= (minX * (1 - x) + maxX * x) - center.x;

        MeshData meshData;
        meshData.gridAxes = mesh.axes[];
        meshData.regenerateGrid();
        mesh.copyFromMeshData(meshData);
    }

    // IAutoMeshReflect provided by mixin
    // schema/values/writeValues provided by mixin
    override
    IncMesh autoMesh(Deformable target, IncMesh mesh, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0) {
        auto ai = getAlphaInput(target);
        if (ai.w <= 0 || ai.h <= 0) return mesh;

        auto imbin = ai.img.sliced[0 .. $, 0 .. $, 3];
        foreach (y; 0 .. imbin.shape[0])
        foreach (x; 0 .. imbin.shape[1])
            imbin[y, x] = imbin[y, x] < cast(ubyte)maskThreshold ? 0 : 255;

        int minXi = ai.w, minYi = ai.h, maxXi = -1, maxYi = -1;
        foreach (y; 0 .. imbin.shape[0])
        foreach (x; 0 .. imbin.shape[1])
            if (imbin[y, x] > 0) {
                int xi = cast(int)x;
                int yi = cast(int)y;
                if (xi < minXi) minXi = xi;
                if (yi < minYi) minYi = yi;
                if (xi > maxXi) maxXi = xi;
                if (yi > maxYi) maxYi = yi;
            }
        if (maxXi < 0 || maxYi < 0) return mesh;

        vec2 imgCenter = alphaImageCenter(ai);
        rebuildGridMesh(mesh, cast(float)minXi, cast(float)maxXi, cast(float)minYi, cast(float)maxYi, imgCenter);
        mapImageCenteredMeshToTargetLocal(mesh, target, ai);
        return mesh;
    }

    void configureLegacyUI() {
        void editScale(ref float[] scales) {
            int deleteIndex = -1;
            if (igBeginChild("###AXIS_ADJ", ImVec2(0, 240))) {
                if (scales.length > 0) {
                    int ix;
                    foreach(i, ref pt; scales) {
                        ix++;

                        // Allow slight extrapolation to support margins
                        vec2 range = vec2(-1, 2);

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
            // Rebuild axis scales from segment counts and margin.
            scaleY.length = 0;
            scaleX.length = 0;
            int ys = cast(int) (ySegments < 1 ? 1 : ySegments);
            int xs = cast(int) (xSegments < 1 ? 1 : xSegments);
            if (margin != 0) { scaleY ~= -margin; scaleX ~= -margin; }
            foreach (y; 0 .. ys + 1) {
                scaleY ~= cast(float)y / cast(float)ys;
            }
            foreach (x; 0 .. xs + 1) {
                scaleX ~= cast(float)x / cast(float)xs;
            }
            if (margin != 0) { scaleY ~= 1 + margin; scaleX ~= 1 + margin; }
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

    // Legacy reflection impl (kept for reference; not used)
    string schemaLegacy() {
        JSONValue obj = JSONValue(JSONType.object);
        obj["type"] = "GridAutoMeshProcessor";
        JSONValue simple = JSONValue(JSONType.array);
        JSONValue adv = JSONValue(JSONType.array);
        // Simple
        JSONValue it;
        it["id"] = "mask_threshold"; it["label"] = "Mask threshold"; it["type"] = "float"; simple.array ~= it; it = JSONValue.init;
        it["id"] = "x_segments"; it["label"] = "X Segments"; it["type"] = "float"; simple.array ~= it; it = JSONValue.init;
        it["id"] = "y_segments"; it["label"] = "Y Segments"; it["type"] = "float"; simple.array ~= it; it = JSONValue.init;
        it["id"] = "margin"; it["label"] = "Margin"; it["type"] = "float"; simple.array ~= it; it = JSONValue.init;
        // Advanced
        it["id"] = "scale_x"; it["label"] = "X Scale"; it["type"] = "float[]"; adv.array ~= it; it = JSONValue.init;
        it["id"] = "scale_y"; it["label"] = "Y Scale"; it["type"] = "float[]"; adv.array ~= it;
        obj["Simple"] = simple;
        obj["Advanced"] = adv;
        obj["presets"] = JSONValue(JSONType.array);
        return obj.toString();
    }
    string valuesLegacy(string levelName) {
        JSONValue v = JSONValue(JSONType.object);
        if (levelName == "Advanced") {
            JSONValue sx = JSONValue(JSONType.array); foreach(x; scaleX) sx.array ~= JSONValue(cast(double)x); v["scale_x"] = sx;
            JSONValue sy = JSONValue(JSONType.array); foreach(y; scaleY) sy.array ~= JSONValue(cast(double)y); v["scale_y"] = sy;
        } else {
            v["mask_threshold"] = JSONValue(cast(double)maskThreshold);
            v["x_segments"] = JSONValue(cast(double)xSegments);
            v["y_segments"] = JSONValue(cast(double)ySegments);
            v["margin"] = JSONValue(cast(double)margin);
        }
        return v.toString();
    }
    bool applyPresetLegacy(string name) { return false; }
    bool writeValuesLegacy(string levelName, string updatesJson) {
        import std.json : parseJSON;
        auto u = parseJSON(updatesJson);
        bool any;
        if (levelName == "Advanced") {
            if ("scale_x" in u) { auto arr = u["scale_x"]; if (arr.type == JSONType.array) { scaleX.length=0; foreach(e; arr.array) if (e.type==JSONType.float_||e.type==JSONType.integer) scaleX ~= cast(float)e.floating; any=true; } }
            if ("scale_y" in u) { auto arr = u["scale_y"]; if (arr.type == JSONType.array) { scaleY.length=0; foreach(e; arr.array) if (e.type==JSONType.float_||e.type==JSONType.integer) scaleY ~= cast(float)e.floating; any=true; } }
        } else {
            if ("mask_threshold" in u && (u["mask_threshold"].type==JSONType.float_||u["mask_threshold"].type==JSONType.integer)) { maskThreshold = cast(float)u["mask_threshold"].floating; any=true; }
            if ("x_segments" in u && (u["x_segments"].type==JSONType.float_||u["x_segments"].type==JSONType.integer)) { xSegments = cast(float)u["x_segments"].floating; any=true; }
            if ("y_segments" in u && (u["y_segments"].type==JSONType.float_||u["y_segments"].type==JSONType.integer)) { ySegments = cast(float)u["y_segments"].floating; any=true; }
            if ("margin" in u && (u["margin"].type==JSONType.float_||u["margin"].type==JSONType.integer)) { margin = cast(float)u["margin"].floating; any=true; }
        }
        return any;
    }
};
