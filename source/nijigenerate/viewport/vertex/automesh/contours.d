module nijigenerate.viewport.vertex.automesh.contours;

import i18n;
import nijigenerate.viewport.vertex.automesh.automesh;
import nijigenerate.viewport.vertex.automesh.meta;
import std.json : JSONValue, JSONType, parseJSON;
import nijigenerate.viewport.common.mesh;
import nijigenerate.widgets;
import nijilive.core;
import nijilive.math : Vec2Array;
import inmath;
import nijigenerate.core.cv; // replaced from dcv-like APIs
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

@AMProcessor("contour", "Contour", 100)
class ContourAutoMeshProcessor : AutoMeshProcessor, IAutoMeshReflect {
    @AMParam(AutoMeshLevel.Simple, "sampling_step", "Sampling rate", "Contour sampling rate", "drag", 1, 200, 1)
    float SAMPLING_STEP = 32;
    const float SMALL_THRESHOLD = 256;
    @AMParam(AutoMeshLevel.Simple, "mask_threshold", "Mask threshold", "Alpha binarize cutoff", "drag", 1, 200, 1)
    float maskThreshold = 15;
    @AMParam(AutoMeshLevel.Advanced, "min_distance", "Minimum distance", "Minimum distance between vertices", "drag", 1, 200, 1)
    float MIN_DISTANCE = 16;
    @AMParam(AutoMeshLevel.Advanced, "max_distance", "Maximum distance", "Maximum distance between vertices", "drag", 1, 200, 1)
    float MAX_DISTANCE = -1;
    @AMParam(AutoMeshLevel.Advanced, "scales", "Scales", "Contour scales", "array")
    @AMArray(0, 2, 0.01)
    float[] SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1];
    string presetName;

    // Presets annotated for reflection
    @AMPreset("Normal parts")
    static void presetNormal(ContourAutoMeshProcessor p) {
        p.SAMPLING_STEP = 50;
        p.maskThreshold = 15;
        p.MIN_DISTANCE = 16;
        p.MAX_DISTANCE = p.SAMPLING_STEP * 2;
        p.SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
        p.presetName = "Normal parts";
    }
    @AMPreset("Detailed mesh")
    static void presetDetailed(ContourAutoMeshProcessor p) {
        p.SAMPLING_STEP = 32;
        p.maskThreshold = 15;
        p.MIN_DISTANCE = 16;
        p.MAX_DISTANCE = p.SAMPLING_STEP * 2;
        p.SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
        p.presetName = "Detailed mesh";
    }
    @AMPreset("Large parts")
    static void presetLarge(ContourAutoMeshProcessor p) {
        p.SAMPLING_STEP = 80;
        p.maskThreshold = 15;
        p.MIN_DISTANCE = 24;
        p.MAX_DISTANCE = p.SAMPLING_STEP * 2;
        p.SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
        p.presetName = "Large parts";
    }
    @AMPreset("Small parts")
    static void presetSmall(ContourAutoMeshProcessor p) {
        p.SAMPLING_STEP = 24;
        p.maskThreshold = 15;
        p.MIN_DISTANCE = 12;
        p.MAX_DISTANCE = p.SAMPLING_STEP * 2;
        p.SCALES = [1, 1.1, 0.6, 0.2];
        p.presetName = "Small parts";
    }
    @AMPreset("Thin and minimum parts")
    static void presetThin(ContourAutoMeshProcessor p) {
        p.SAMPLING_STEP = 12;
        p.maskThreshold = 1;
        p.MIN_DISTANCE = 4;
        p.MAX_DISTANCE = p.SAMPLING_STEP * 2;
        p.SCALES = [1];
        p.presetName = "Thin and minimum parts";
    }
    @AMPreset("Preserve edges")
    static void presetEdges(ContourAutoMeshProcessor p) {
        p.SAMPLING_STEP = 24;
        p.maskThreshold = 15;
        p.MIN_DISTANCE = 8;
        p.MAX_DISTANCE = p.SAMPLING_STEP * 2;
        p.SCALES = [1, 1.2, 0.8];
        p.presetName = "Preserve edges";
    }
    // Unified alpha preview state (used by mixin)
    private AlphaPreviewState _alphaPreview;
public:
    mixin AutoMeshClassInfo!();
    // Bring in unified reflection/UI
    mixin AutoMeshReflection!();
    override IncMesh autoMesh(Deformable target, IncMesh mesh, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0) {
        import nijilive.core.nodes.deformer.grid : GridDeformer;
        if (cast(GridDeformer)target) return mesh;
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
            Vec2Array result;
            foreach (p; contours) result ~= vec2(p.x, p.y);
            return result;
        }
        auto calcMoment(Vec2Array contour) {
            auto moment = contour.toArray().reduce!((a, b) { return a + b; })();
            return moment / contour.length;
        }
        auto scaling(Vec2Array contour, vec2 moment, float scale, int erode_dilate) {
            return contour.toArray().map!((c) { return (c - moment) * scale + moment; }).array;
        }
        auto resampling(Vec2Array contour, double rate, bool mirrorHoriz_, float axisHoriz_, bool mirrorVert_, float axisVert_) {
            Vec2Array sampled;
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
                foreach (vec2 c; contour3) {
                    if (mesh.vertices.length > 0) {
                        auto minDistance = mesh.vertices
                            .map!((v) { return ((c - imgCenter) - v.position).length; })
                            .reduce!((a, b) { return a < b ? a : b; });
                        if (minDistance > MIN_DISTANCE)
                            mesh.vertices ~= new MeshVertex(c - imgCenter, []);
                    } else {
                        mesh.vertices ~= new MeshVertex(c - imgCenter, []);
                    }
                }
            }
        }

        auto outMesh = mesh.autoTriangulate();
        mapImageCenteredMeshToTargetLocal(outMesh, target, ai);
        return outMesh;
    }

    /// Build mesh from arbitrary nodes by projecting alpha (includes Mask/Shape)
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
            Vec2Array result;
            foreach (p; contours) result ~= vec2(p.x, p.y);
            return result;
        }
        auto calcMoment(Vec2Array contour) {
            auto moment = contour.toArray().reduce!((a, b) { return a + b; })();
            return moment / contour.length;
        }
        auto scaling(Vec2Array contour, vec2 moment, float scale, int erode_dilate) {
            return contour.toArray().map!((c) { return (c - moment) * scale + moment; }).array;
        }
        auto resampling(Vec2Array contour, double rate, bool mirrorHoriz_, float axisHoriz_, bool mirrorVert_, float axisVert_) {
            Vec2Array sampled;
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
                foreach (vec2 c; contour3) {
                    if (mesh.vertices.length > 0) {
                        auto minDistance = mesh.vertices
                            .map!((v) { return ((c - imgCenter) - v.position).length; })
                            .reduce!((a, b) { return a < b ? a : b; });
                        if (minDistance > MIN_DISTANCE)
                            mesh.vertices ~= new MeshVertex(c - imgCenter, []);
                    } else {
                        mesh.vertices ~= new MeshVertex(c - imgCenter, []);
                    }
                }
            }
        }

        return mesh.autoTriangulate();
    }
    // configure() provided by AutoMeshReflection mixin; keep MAX_DISTANCE guard in autoMesh
    override 
    string icon() {
        return "";
    }
    // IAutoMeshReflect provided by mixin
};
