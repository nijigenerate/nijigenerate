module nijigenerate.viewport.vertex.automesh.optimum;

import i18n;
import nijigenerate.viewport.vertex.automesh.automesh;
import nijigenerate.viewport.common.mesh;
import nijigenerate.widgets;
import nijigenerate.core.math.skeletonize;
import nijigenerate.core.math.path;
import nijigenerate.core.math.triangle;
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
alias mirAny = mir.algorithm.iteration.any;
debug(automesh_opt) import std.stdio;
import std.array;
import std.typecons;
import bindbc.imgui;

class OptimumAutoMeshProcessor : AutoMeshProcessor {
    float LARGE_THRESHOLD = 400;
    float LENGTH_THRESHOLD = 100;
    float RATIO_THRESHOLD = 0.2;
    float SHARP_EXPANSION_FACTOR = 0.01;
    float NONSHARP_EXPANSION_FACTOR = 0.05;
    float NONSHARP_CONTRACTION_FACTOR = 0.05;
    float[] SCALES = [0.5, 0.];
    float MIN_DISTANCE = 10;
    float SIZE_AVG = 100;
    float MASK_THRESHOLD = 1;
    float DIV_PER_PART = 12;

    string presetName;
public:
    override IncMesh autoMesh(Drawable target, IncMesh mesh, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0) {
        auto contoursToVec2s(ContourType)(ContourType contours) {
            vec2[] result;
            bool[ulong] visited;
            ulong nextIndex = 0;
            ulong findNearest(C)(ref C contour) {
                ulong index = ulong.max;
                float minDist = float.infinity;
                foreach(i; 0..contours.length) {
                    if (i in visited) continue;
                    if (contours[i].length == 0) continue;
                    float dist = vec2(contour[$-1, 1] - contours[i][0,1], contour[$-1, 0] - contours[i][0,0]).length;
                    if (dist < minDist) {
                        index = i;
                        minDist = dist;
                    }
                }
                return index;
            }
            debug(automesh_opt) {
                foreach(i, c; contours) {
                    writefln("contour: %d, %s", i, c.shape);
                }
            }
            while (nextIndex != ulong.max && visited.length < contours.length) { // Not safe
                visited[nextIndex] = true;
                auto contour = contours[nextIndex];
                debug(automesh_opt) writefln("shape: %s", contour.shape);
                foreach (idx; 0..contour.shape[0]) {
                    result ~= vec2(contour[idx, 1], contour[idx, 0]);
                }
                debug(automesh_opt) writef(" findNearest: %d(%s)", nextIndex, visited[nextIndex]);
                nextIndex = findNearest(contour);
                debug(automesh_opt) writefln("->%d(%s)", nextIndex, (nextIndex in visited)? visited[nextIndex]: false);
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
        
        auto horizontalMirrored(vec2[] sampled) {
            float side = 0;
            vec2[] mirrored;
            foreach (idx; 0..sampled.length) {
                vec2 c    = sampled[idx];
                bool[ulong] used;
                if (side == 0) {
                    side = sign(c.x - axisHoriz);
                    mirrored ~= sampled[idx];
                } else if (sign(c.x - axisHoriz) != side) {
                    auto flipped = vec2(axisHoriz * 2 - c.x, c.y);
                    auto index = sampled.map!((a)=>(a - flipped).lengthSquared).minIndex();
                    if (index !in used) {
                        mirrored ~= vec2(axisHoriz * 2 - sampled[index].x, sampled[index].y);
                        used[index] = true;
                    }
                } else {
                    mirrored ~= sampled[idx];
                }
            }
            return mirrored.stdUniq.array;
        }

        auto resampling(vec2[] contour, double rate, bool mirrorHoriz, float axisHoriz, bool mirrorVert, float axisVert) {
            vec2[] sampled;
            ulong base = 0;
            /*
            if (mirrorHoriz) {
                float minDistance = -1;
                foreach (i, vertex; contour) {
                    if (minDistance < 0 || vertex.x - axisHoriz < minDistance) {
                        base = i;
                        minDistance = vertex.x - axisHoriz;
                    }
                }
            }
            */
            sampled ~= contour[base];
            foreach (idx; 1..contour.length) {
                vec2 prev = sampled[$-1];
                vec2 c    = contour[(idx + base)%$];
                if ((c-prev).lengthSquared > rate*rate) {
                    sampled ~= c;
                }
            }
            if (mirrorHoriz) {
                return horizontalMirrored(sampled);
            } else
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
        if (mirrorHoriz) {
            axisHoriz += texture.width / 2;
        }
        if (mirrorVert) {
            axisVert += texture.height / 2;
        }
        
        float step = 1;

        auto gray = img.sliced[0..$, 0..$, 3]; // Use transparent channel for boundary search
        auto imbin = gray;
        foreach (y; 0..imbin.shape[0]) {
            foreach (x; 0..imbin.shape[1]) {
                imbin[y, x] = imbin[y, x] < cast(ubyte)MASK_THRESHOLD? 0: 255;
            }
        }

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

        auto compensated_cont = imbin.slice.dilate(radialKernel!ubyte(5)).threshold!ubyte(1, 255).erode(radialKernel!ubyte(5));
        auto compensated = dupMono(imbin);

        foreach (y; 0..compensated.shape[0]) {
            foreach (x; 0..compensated.shape[1]) {
                compensated[y, x] = compensated_cont[y, x] != 0 ? 255: 0;
            }
        }
        
        auto calculateWidthMap(T)(T imbin, vec2u[] skeleton) {
            auto distTransform = distanceTransform(imbin);
            // widthMap は1次元配列、サイズは画像全画素数（width * height）
            int[] widthMap;
            foreach (s; skeleton) {
                if (distTransform[s.y, s.x])
                    widthMap ~= distTransform[s.y, s.x] *(2/2); // dcv (0.3.0) uses  Chamfer Distance (not euclid distance.) To get approximate distance, divide by 2.
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
                expanded ~= vec2(thinnedPoints[i].x + normal.x * expDist,
                                 thinnedPoints[i].y + normal.y * expDist);
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
                contracted ~= vec2(thinnedPoints[i].x - normal.x * (contDist * factor),
                                   thinnedPoints[i].y - normal.y * (contDist * factor));
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
        float min_distance = max(max(texture.width, texture.height)/DIV_PER_PART, MIN_DISTANCE);

        vec2[] vertices;
        vec2[] vB1;
        double sumWidth = 0;
        double length = 0;
        double widthMapLength = 0;

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
                if (imbin[y, x] == 0) {
                    labelFound[labels[y, x]] = true;
                }
            }
        }
        debug(automesh_opt) writefln("bwlabels=%d", labelFound.stdFilter!(x=>x).array.length);

        typeof(findContours(imbin)[0])[] contourList;
        int numFound = 0;
        foreach (label, found; labelFound) {
            if (!found)
                continue;
            numFound ++;
            foreach (y; 0..imbin.shape[0]) {
                foreach (x; 0..imbin.shape[1]) {
                    imbin[y, x] = (labels[y, x] == label && imbin[y, x] == 0)? 0: 255;
                }
            }
            // calculate distanceTransform
            auto skel = dupMono(imbin);
            skeletonizeImage(skel);
            auto skelPath = extractPath(skel, texture.width, texture.height);

            auto widthMap = calculateWidthMap(imbin, skelPath);
            widthMapLength += widthMap.length;
            debug(automesh_opt) writefln("  label %d: widthMapLength=%0.2f", label, widthMapLength);
            debug(automesh_opt_full) writefln("path=%s", zip(skelPath, widthMap).map!((t)=>"%s=%s".format(t[0], t[1])).array);
            int[] validWidth = widthMap.stdFilter!((x)=>x > 0).array;
            sumWidth += validWidth.sum;
            length   += validWidth.length;

            // switch based on sharpness of the target shape.

        }
        double avgWidth = sumWidth / length;
        double ratio    = sumWidth / widthMapLength;
        debug(automesh_opt) { writefln("found=%d: avgW=%0.2f, len=%0.2f, avgW/len=%0.2f, ratio=%0.2f", numFound, avgWidth, length, avgWidth / length, ratio); }

        writefln("findContours start");
        auto contours = findContours(compensated);
        writefln("findContours done");
        foreach (c; contours) {
            contourList ~= c;
        }
        debug(automesh_opt) { writefln("contours=%d", contours.length); }
        auto contourVec = contoursToVec2s(contourList);
        debug(automesh_opt) { writefln("contourVec=%d", contourVec.length); }

        if (contourVec.length < 3) return mesh;

        mesh.clear();

        bool sharpFlag = (avgWidth < LARGE_THRESHOLD) &&
                        ((length < LENGTH_THRESHOLD) || ((avgWidth / length) < RATIO_THRESHOLD));


        // reduce vertices by resampling (with consideration for flip flag)

        vB1 ~= resampling(contourVec, min_distance, mirrorHoriz, axisHoriz, mirrorVert, axisVert);

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
                auto sampledAtRate(vec2[] contours, float rate) {
                    vec2[] sampled;
                    float samplingFlag = 0;
                    foreach (v; contours) {
                        if (samplingFlag<= 0) {
                            sampled ~= v;
                            samplingFlag += 1.0;
                        }
                        samplingFlag -= rate;
                    }
                    return sampled;
                }
                vCentroid = sampledAtRate(vCentroid, scale);
                if (mirrorHoriz)
                    vCentroid = horizontalMirrored(vCentroid);
                vertices ~= vCentroid;
            }
            vertices = vertices.stdUniq.array;
        }

        vec4 bounds = vec4(0, 0, texture.width, texture.height);
        auto vert_ind = triangulate(vertices, bounds);
        vertices = vert_ind[0];
        auto tris = vert_ind[1];


        bool completeUncoveredArea(T)(T compensated, vec2[] vertices, vec3u[] tris, vec2[] contourVec, float min_distance, out vec2[] outVertices, out vec3u[] outTris) {
            import mir.ndslice.topology;
            int err;
            auto compensated1D = compensated.reshape([-1], err);
            fillPoly(compensated1D, texture.width, texture.height, bounds, vertices, tris, 0, cast(ubyte)0);
            int initialRemainingArea = compensated1D.map!(x=>x!=0?255:0).sum;
            if (initialRemainingArea == 0) { 
                outVertices = vertices;
                outTris = tris;
                return false;
            }
            
            // 既存頂点群から十分離れている候補のみ選択
            vec2[] filteredCandidates;
            foreach(p; contourVec) {
                bool skip = false;
                if(vertices.length > 0) {
                    foreach(v; vertices) {
                        if(distance(v, p) < min_distance * 0.5) {
                            skip = true;
                            break;
                        }
                    }
                }
                int x = cast(int)round(p.x);
                int y = cast(int)round(p.y);
                if(x >= 0 && x < texture.width && y >= 0 && y < texture.height) {
                    int idx = y * texture.width + x;
                    if(compensated1D[idx] == 255 && !skip)
                        filteredCandidates ~= p;
                }
            }
            if(filteredCandidates.length == 0) {
                outVertices = vertices;
                outTris = tris;
                return false;
            }
            
            // 候補点の中から窓内の uncovered ピクセル数が最大の点を選択
            int bestScore = -1;
            vec2 bestCandidate = filteredCandidates[0];
            int windowSize = cast(int)min_distance;
            foreach(p; filteredCandidates) {
                int x = cast(int)round(p.x);
                int y = cast(int)round(p.y);
                int x1 = max(0, x - windowSize);
                int y1 = max(0, y - windowSize);
                int x2 = min(texture.width - 1, x + windowSize);
                int y2 = min(texture.height - 1, y + windowSize);
                int score = 0;
                for (int j = y1; j <= y2; j++) {
                    for (int i = x1; i <= x2; i++) {
                        int idx = j * texture.width + i;
                        if (compensated1D[idx] == 255)
                            score++;
                    }
                }
                if (score > bestScore) {
                    bestScore = score;
                    bestCandidate = p;
                }
            }
            if(bestScore < 0) {
                outVertices = vertices;
                outTris = tris;
                return false;
            }
            // bestCandidate を追加候補点として finalVertices に追加
            vec2[] newFinalVertices = vertices ~ [ bestCandidate ];
            // 新たな三角形分割
            auto newVertsInd = triangulate(newFinalVertices, vec4(0, 0, texture.width, texture.height));
            auto newTriangles = newVertsInd[1];
            auto newVertices = newVertsInd[0];
            if(newTriangles !is null) {
                foreach(i, tri; newTriangles) {
                    fillPoly(compensated1D, texture.width, texture.height, bounds, newVertices, newTriangles, i, cast(ubyte)0);                        
                }
                int remainingArea = compensated1D.map!(x=>x!=0?255:0).sum;
                if(remainingArea < initialRemainingArea) {
                    outVertices = newVertices;
                    outTris = newTriangles;
                    return true;
                }
            }
            outVertices = vertices;
            outTris = tris;
            return false;
        }

        for (int i = 0; i < 5; i ++) {
            bool updated = completeUncoveredArea(compensated, vertices, tris, contourVec, min_distance, vertices, tris);
            debug(automesh_opt) { writefln("complete uncovered:%s", updated); }
            if (!updated) break;
        }

        if (vertices.length < 3) return mesh;

        IncMesh newMesh = new IncMesh(mesh);
        newMesh.changed = true;
        newMesh.vertices.length = 0;
        newMesh.importVertsAndTris(vertices.map!((x){
            auto v = x-imgCenter;
            if (auto dcomposite = cast(DynamicComposite)target) {
                v += dcomposite.textureOffset;
            }
            return v;
        }).array, tris);
        newMesh.refresh();
        return newMesh;
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
        return "";
    }
};