module nijigenerate.viewport.vertex.automesh.optimum;

import i18n;
import nijigenerate.viewport.vertex.automesh.automesh;
import nijigenerate.viewport.common.mesh;
import nijigenerate.widgets;
import nijigenerate.core.math.skeletonize;
import nijigenerate.core.math.path;
import nijilive.core;
import inmath;
import dcv.core;
import dcv.imgproc;
import dcv.measure;
import dcv.morphology;
import std.algorithm;
alias stdFilter = std.algorithm.iteration.filter;
import std.algorithm.iteration: map, reduce, uniq;
alias stdUniq = std.algorithm.iteration.uniq;
import mir.ndslice;
import mir.math.stat: mean;
import std.stdio;
import std.array;
import bindbc.imgui;

class OptimumAutoMeshProcessor : AutoMeshProcessor {
    float LARGE_THRESHOLD = 400;
    float LENGTH_THRESHOLD = 100;
    float RATIO_THRESHOLD = 0.2;
    float SHARP_EXPANSION_FACTOR = 0.03;
    float NONSHARP_EXPANSION_FACTOR = 0.05;
    float NONSHARP_CONTRACTION_FACTOR = 0.05;
    float[] SCALES = [0.0, 0.5];
    float MIN_DISTANCE = 10;
    float SIZE_AVG = 100;
    float MASK_THRESHOLD = 1;

    string presetName;
public:
    override IncMesh autoMesh(Drawable target, IncMesh mesh, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0) {
        auto contoursToVec2s(ContourType)(ref ContourType contours) {
            vec2[] result;
            foreach (contour; contours) {
                if (contour.length < 10)
                    continue;

                foreach (idx; 1..contour.shape[0]) {
                    result ~= vec2(contour[idx, 1], contour[idx, 0]);
                }
            }
            return result;
        }

        auto calcMoment(vec2[] contour) {
            auto moment = contour.reduce!((a, b){return a+b;})();
            return moment / contour.length;
        }

        auto scaling(vec2[] contour, vec2 moment, float scale, int erode_dilate) {
            float cx = 0, cy = 0;
            return contour.map!((c) { return (c - moment)*scale + moment; })().array;
        }
        
        auto resampling(vec2[] contour, double rate, bool mirrorHoriz, float axisHoriz, bool mirrorVert, float axisVert) {
            vec2[] sampled;
            ulong base = 0;
            if (mirrorHoriz) {
                float minDistance = -1;
                foreach (i, vertex; contour) {
                    if (minDistance < 0 || vertex.x - axisHoriz < minDistance) {
                        base = i;
                        minDistance = vertex.x - axisHoriz;
                    }
                }
            }
            sampled ~= contour[base];
            float side = 0;
            foreach (idx; 1..contour.length) {
                vec2 prev = sampled[$-1];
                vec2 c    = contour[(idx + base)%$];
                if ((c-prev).lengthSquared > rate*rate) {
                    if (mirrorHoriz) {
                        if (side == 0) {
                            side = sign(c.x - axisHoriz);
                        } else if (sign(c.x - axisHoriz) != side) {
                            continue;
                        }
                    }
                    sampled ~= c;
                }
            }
            return sampled;
        }

        Part part = cast(Part)target;
        if (!part)
            return mesh;

        Texture texture = part.textures[0];
        if (!texture)
            return mesh;
        ubyte[] data = texture.getTextureData();
        auto img = new Image(texture.width, texture.height, ImageFormat.IF_RGB_ALPHA);
        copy(data, img.data);
        
        float step = 1;

        auto gray = img.sliced[0..$, 0..$, 3]; // Use transparent channel for boundary search
        auto imbin = gray;
        foreach (y; 0..imbin.shape[0]) {
            foreach (x; 0..imbin.shape[1]) {
                imbin[y, x] = imbin[y, x] < cast(ubyte)MASK_THRESHOLD? 0: 255;
            }
        }
        auto compensated = imbin.slice.dilate(radialKernel!ubyte(5)).threshold!ubyte(1, 255).erode(radialKernel!ubyte(5));
        writefln("compensated:%s <=> imbin: %s(%d)", compensated.shape, imbin.shape, imbin.shape[0] * imbin.shape[1]);
        foreach (y; 0..imbin.shape[0]) {
            foreach (x; 0..imbin.shape[1]) {
                imbin[y, x] = compensated[y, x];
            }
        }
        
        auto labels = bwlabel(imbin);
        bool[] labelFound = [false];
        long maxLabel = 0;
        foreach (y; 0..labels.shape[0]) {
            foreach (x; 0..labels.shape[1]) {
                if (labels[y, x] > maxLabel) {
                    maxLabel = labels[y, x];
                    while (labelFound.length <= maxLabel)
                        labelFound ~= false;
                } 
                if (compensated[y, x] == 0) {
                    labelFound[labels[y, x]] = true;
                }
            }
        }
        mesh.clear();

        // calculate skeleton
        auto dupMono(T)(T imbin) {
            ubyte[] data = new ubyte[imbin.shape[0] * imbin.shape[1]];
            foreach (y; 0..imbin.shape[0]) {
                foreach (x; 0..imbin.shape[1]) {
                    data[y*imbin.shape[1]+x] = cast(ubyte)imbin[y, x];
                }
            }
            Image resultImage = new Image(imbin.shape[1], imbin.shape[0], ImageFormat.IF_MONO, BitDepth.BD_8, data);
            return resultImage.sliced[0..$, 0..$, 0];
        }

        auto calculateWidthMap(T)(T imbin, vec2u[] skeleton) {
            auto distTransform = distanceTransform(imbin);
            // widthMap は1次元配列、サイズは画像全画素数（width * height）
            int[] widthMap;
            foreach (s; skeleton) {
                widthMap ~= distTransform[s.y, s.x] * 2; 
            }
            return widthMap;
        }


        vec2 calculateNormalVector(vec2 p1, vec2 p2) {
            float vx = p2.y - p1.y;
            float vy = p1.x - p2.x;
            float norm = sqrt(vx * vx + vy * vy);
            return (norm == 0) ? vec2(0, 0) : vec2(vx / norm, vy / norm);
        }

        vec2[] sampleExpandedFromThinned(vec2[] thinnedPoints, float expDist) {
            auto n = thinnedPoints.length;
            if (n < 3)
                return thinnedPoints;
            vec2[] expanded;
            for (size_t i = 0; i < n; i++) {
                auto pPrev = thinnedPoints[(i + n - 1) % n];
                auto pNext = thinnedPoints[(i + 1) % n];
                auto normal = calculateNormalVector(pPrev, pNext);
                expanded ~= vec2(thinnedPoints[i].x - normal.x * expDist,
                                 thinnedPoints[i].y - normal.y * expDist);
            }
            return expanded.stdUniq.array;
        }

        vec2[] sampleContractedFromThinned(vec2[] thinnedPoints, float contDist, float factor = 1) {
            auto n = thinnedPoints.length;
            if (n < 3)
                return thinnedPoints;
            vec2[] contracted;
            for (size_t i = 0; i < n; i++) {
                auto pPrev = thinnedPoints[(i + n - 1) % n];
                auto pNext = thinnedPoints[(i + 1) % n];
                auto normal = calculateNormalVector(pPrev, pNext);
                contracted ~= vec2(thinnedPoints[i].x + normal.x * (contDist * factor),
                                   thinnedPoints[i].y + normal.y * (contDist * factor));
            }
            return contracted.stdUniq.array;
        }

        vec2[] sampleCentroidContractedContour(vec2[] contour, float scale, float minDistance) {
            vec2 centroid = contour.sum;
            centroid /= contour.length;
            vec2[] pts;
            foreach(p; contour) {
                pts ~= vec2(centroid.x + scale * (p.x - centroid.x),
                            centroid.y + scale * (p.y - centroid.y));
            }
            return pts.stdUniq.array;
        }

        vec2 imgCenter = vec2(texture.width / 2, texture.height / 2);
        float size_avg = (texture.width + texture.height) / 2.0;
        float min_distance = max(int(max(texture.width, texture.height)/10), 10);

        vec2[] vertices;
        vec2[] vB1;
        double sumWidth = 0;
        double length = 0;
        double widthMapLength = 0;

        writefln("labels=%d", labelFound.length);

        // calculate distanceTransform
        auto skel = dupMono(imbin);
        skeletonizeImage(skel);
        auto skelPath = extractPath(skel, texture.width, texture.height); // flipped width and height because dcv.skeletonize2D returns transposed image.

        auto widthMap = calculateWidthMap(imbin, skelPath);
        int[] validWidth = widthMap.stdFilter!((x)=>x > 0).array;
        sumWidth += validWidth.sum;
        length   += validWidth.length;
        widthMapLength += widthMap.length;
        double avgWidth = sumWidth / length;
        double ratio    = sumWidth / widthMapLength;

        writefln("avgW=%0.2f, len=%0.2f, avgW/len=%0.2f, ratio=%0.2f", avgWidth, length, avgWidth / length, ratio);
        bool sharpFlag = (avgWidth < LARGE_THRESHOLD) &&
                        ((length < LENGTH_THRESHOLD) || ((avgWidth / length) < RATIO_THRESHOLD));

        foreach (label, found; labelFound) {
            if (!found)
                continue;
            foreach (y; 0..imbin.shape[0]) {
                foreach (x; 0..imbin.shape[1]) {
                    imbin[y, x] = (labels[y, x] == label && imbin[y, x] == 0)? 255: 0;
                }
            }

            // switch based on sharpness of the target shape.

            auto contours = findContours(imbin);
            auto contourVec = contoursToVec2s(contours);

            if (contourVec.length == 0)
                continue;

            // reduce vertices by resampling (with consideration for flip flag)

            vB1 ~= resampling(contourVec, min_distance, mirrorHoriz, axisHoriz, mirrorVert, axisVert);
            if (mirrorHoriz) {
                auto flipped = vB1.map!((a) => vec2(imgCenter.x + axisHoriz - (a.x - imgCenter.x - axisHoriz), a.y));
                foreach (f; flipped) {
                    auto index = contourVec.map!((a)=>(a - f).lengthSquared).minIndex();
                    vB1 ~= contourVec[index];
                }
            }
        }

        // Type A: sharp shapes
        if (sharpFlag) {
            auto vA = sampleExpandedFromThinned(vB1, size_avg * SHARP_EXPANSION_FACTOR);
            vertices ~= vA;
        } else {
            // Type B: unsharp shapes
            // B-1 adds original resampled shapes.
            // B-2 adds vertices expanded in normal direction
            // B-3 adds vertices shrinked in normal direction
            // B-4 adds vertices scaled around centroid.
            float[] scales;
            // scaling for larger parts
            scales = SCALES;

            vertices ~= vB1;
            auto vB2 = sampleExpandedFromThinned(vB1, size_avg * NONSHARP_EXPANSION_FACTOR);
            vertices ~= vB2;
            auto vB3 = sampleContractedFromThinned(vB1, size_avg * NONSHARP_CONTRACTION_FACTOR, 1.0);
            vertices ~= vB3;
            foreach(scale; SCALES) {
                auto vCentroid = sampleCentroidContractedContour(vB1, scale, min_distance);
                vertices ~= vCentroid;
            }
        }

        foreach (v; vertices) {
            mesh.vertices ~= new MeshVertex(v - imgCenter, []);
        }

        if (auto dcomposite = cast(DynamicComposite)target) {
            foreach (vertex; mesh.vertices) {
                vertex.position += dcomposite.textureOffset;
            }
        }
        
        return mesh.autoTriangulate();
    };

    override void configure() {
        if (!presetName) {
            presetName = "Normal parts";
        }

        incText(_("Presets"));
        igIndent();
        if(igBeginCombo(__("Presets"), __(presetName))) {
            if (igSelectable(__("Normal parts"))) {
                presetName = "Normal parts";
                MASK_THRESHOLD = 15;
                MIN_DISTANCE = 16;
                SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
            }
            if (igSelectable(__("Detailed mesh"))) {
                presetName = "Detailed mesh";
                MASK_THRESHOLD = 15;
                MIN_DISTANCE = 16;
                SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
            }
            if (igSelectable(__("Large parts"))) {
                presetName = "Large parts";
                MASK_THRESHOLD = 15;
                MIN_DISTANCE = 24;
                SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
            }
            if (igSelectable(__("Small parts"))) {
                presetName = "Small parts";
                MASK_THRESHOLD = 15;
                MIN_DISTANCE = 12;
                SCALES = [1, 1.1, 0.6, 0.2];

            }
            if (igSelectable(__("Thin and minimum parts"))) {
                presetName = "Thin and minimum parts";
                MASK_THRESHOLD = 1;
                MIN_DISTANCE = 4;
                SCALES = [1];

            }
            if (igSelectable(__("Preserve edges"))) {
                presetName = "Preserve edges";
                MASK_THRESHOLD = 15;
                MIN_DISTANCE = 8;
                SCALES = [1, 1.2, 0.8];

            }
            igEndCombo();
        }
        igUnindent();

        igPushID("CONFIGURE_OPTIONS");
        if (incBeginCategory(__("Details"))) {

            if (igBeginChild("###CONTOUR_OPTIONS", ImVec2(0, 320))) {

                incText(_("Mask threshold"));
                igIndent();
                igPushID("MASK_THRESHOLD");
                    igSetNextItemWidth(64);
                    if (incDragFloat(
                        "mask_threshold", &MASK_THRESHOLD, 1,
                        1, 200, "%.2f", ImGuiSliderFlags.NoRoundToFormat)
                    ) {
                        MASK_THRESHOLD = MASK_THRESHOLD;
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

                                    // Do not allow existing points to cross over
                                    vec2 range = vec2(0, 2);

                                    igSetNextItemWidth(80);
                                    igPushID(cast(int)i);
                                        if (incDragFloat(
                                            "adj_offset", &SCALES[i], 0.01,
                                            range.x, range.y, "%.2f", ImGuiSliderFlags.NoRoundToFormat)
                                        ) {
                                            // do something
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


    }

    override
    string icon() {
        return "A";
    }
};