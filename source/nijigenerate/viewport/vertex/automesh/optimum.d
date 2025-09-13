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
import mir.rc.array : RCArray, rcarray;
import nijigenerate.core.cv.distancetransform;
import nijigenerate.core.cv.contours;
import std.algorithm;
alias stdFilter = std.algorithm.iteration.filter;
import std.algorithm.iteration: map, reduce, uniq;
alias stdUniq = std.algorithm.iteration.uniq;
import mir.ndslice;
import mir.ndslice : reshape; // for ND-slice reshape
import mir.math.stat: mean;
alias mirAny = mir.algorithm.iteration.any;
debug(automesh_opt) import std.stdio;
import std.array;
import std.typecons;
import bindbc.imgui;
import nijigenerate.core.cv.image;
import core.exception;
import nijigenerate.viewport.vertex.automesh.alpha_provider;
import nijigenerate.viewport.vertex.automesh.common;
import nijigenerate.viewport.vertex.automesh.contours : ContourAutoMeshProcessor;

class OptimumAutoMeshProcessor : AutoMeshProcessor {
    float LARGE_THRESHOLD = 400;
    float LENGTH_THRESHOLD = 100;
    float RATIO_THRESHOLD = 0.2;
    float SHARP_EXPANSION_FACTOR = 0.01;
    float NONSHARP_EXPANSION_FACTOR = 0.05;
    float NONSHARP_CONTRACTION_FACTOR = 0.05;
    float[] SCALES = [0.5, 0.];
    float MIN_DISTANCE = 10;
    float MASK_THRESHOLD = 1;
    float DIV_PER_PART = 12;

    string presetName;
    // Unified alpha preview state
    private AlphaPreviewState _alphaPreview;
public:
    override IncMesh autoMesh(Drawable target, IncMesh mesh, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0) {

        // Convert contours to a vec2 array
        auto contoursToVec2s(ContourType)(ContourType contours) {
            vec2[] result;
            bool[ulong] visited;
            ulong nextIndex = 0;
            ulong findNearest(C)(ref C contour) {
                ulong index = ulong.max;
                float minDist = float.infinity;
                foreach(i; 0 .. contours.length) {
                    if (i in visited) continue;
                    if (contours[i].length == 0) continue;
                    float dist = vec2(contour[contour.length - 1].y - contours[i][0].y,
                                      contour[contour.length - 1].x - contours[i][0].x).length;
                    if (dist < minDist) {
                        index = i;
                        minDist = dist;
                    }
                }
                return index;
            }
            debug(automesh_opt) {
                foreach(i, c; contours) {
                    writefln("contour: %d, %d", i, c.length);
                }
            }
            while (nextIndex != ulong.max && visited.length < contours.length) { // Not safe
                visited[nextIndex] = true;
                auto contour = contours[nextIndex];
                debug(automesh_opt) writefln("shape: %d", contour.length);
                foreach(idx; 0 .. contour.length) {
                    result ~= vec2(contour[idx].x, contour[idx].y);
                }
                debug(automesh_opt) writef(" findNearest: %d(%s)", nextIndex, visited[nextIndex]);
                nextIndex = findNearest(contour);
                debug(automesh_opt) writefln("->%d(%s)", nextIndex, (nextIndex in visited) ? visited[nextIndex] : false);
            }
            return result;
        }

        auto calcMoment(vec2[] contour) {
            auto moment = contour.reduce!((a, b){ return a + b; })();
            return moment / contour.length;
        }

        auto scaling(vec2[] contour, vec2 moment, float scale, int erode_dilate) {
            // Scaling function applied to contour points
            return contour.map!((c) { return (c - moment) * scale + moment; })().array;
        }
        
        auto horizontalMirrored(vec2[] sampled) {
            // Mirrors sampled points horizontally using axisHoriz
            float side = 0;
            vec2[] mirrored;
            foreach(idx; 0 .. sampled.length) {
                vec2 c = sampled[idx];
                bool[ulong] used;
                if (side == 0) {
                    side = sign(c.x - axisHoriz);
                    mirrored ~= sampled[idx];
                } else if (sign(c.x - axisHoriz) != side) {
                    auto flipped = vec2(axisHoriz * 2 - c.x, c.y);
                    auto index = sampled.map!((a) => (a - flipped).lengthSquared).minIndex();
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
                foreach(i, vertex; contour) {
                    if (minDistance < 0 || vertex.x - axisHoriz < minDistance) {
                        base = i;
                        minDistance = vertex.x - axisHoriz;
                    }
                }
            }
            */
            sampled ~= contour[base];
            foreach(idx; 1 .. contour.length) {
                vec2 prev = sampled[$ - 1];
                vec2 c = contour[(idx + base) % contour.length];
                if ((c - prev).lengthSquared > rate * rate) {
                    sampled ~= c;
                }
            }
            return sampled;
        }

        // --- Additional helper functions ---
        vec2 calculateNormalVector(vec2 p1, vec2 p2) {
            // Calculate normalized perpendicular vector from p1 to p2
            float vx = p2.y - p1.y;
            float vy = p1.x - p2.x;
            float norm = sqrt(vx * vx + vy * vy);
            return (norm == 0) ? vec2(0, 0) : vec2(vx / norm, vy / norm);
        }

        vec2[] sampleExpandedFromThinned(vec2[] thinnedPoints, float expDist) {
            // Expand points along normal direction
            auto n = thinnedPoints.length;
            if(n < 3)
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
            // Contract points along normal direction
            auto n = thinnedPoints.length;
            if(n < 3)
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
            // Scale contour around its centroid
            vec2 centroid = contour.sum / contour.length;
            vec2[] pts;
            foreach(p; contour) {
                pts ~= vec2(centroid.x + scale * (p.x - centroid.x),
                            centroid.y + scale * (p.y - centroid.y));
            }
            return pts.stdUniq.array;
        }
        // --- End of helper functions ---

        Part part = cast(Part)target;

        // Prepare Image (alpha-only) either from Part texture or Provider alpha
        auto ai = getAlphaInput(target);
        if (ai.w <= 0 || ai.h <= 0 || ai.img is null) return mesh;
        auto img = ai.img;
        int texW = ai.w, texH = ai.h;
        if (mirrorHoriz) {
            axisHoriz += texW / 2;
        }
        if (mirrorVert) {
            axisVert += texH / 2;
        }
        
        float step = 1;
        auto gray = img.sliced[0 .. $, 0 .. $, 3]; // Use the alpha channel for boundary search
        auto imbin = gray;
        foreach(y; 0 .. imbin.shape[0]) {
            foreach(x; 0 .. imbin.shape[1]) {
                imbin[y, x] = imbin[y, x] < cast(ubyte)MASK_THRESHOLD ? 0 : 255;
            }
        }

        // Duplicate monochrome image from imbin
        auto dupMono(T)(T imbin) {
            ubyte[] d = new ubyte[imbin.shape[0] * imbin.shape[1]];
            foreach(y; 0 .. imbin.shape[0]) {
                foreach(x; 0 .. imbin.shape[1]) {
                    d[y * imbin.shape[1] + x] = cast(ubyte)imbin[y, x];
                }
            }
            auto res = new Image(imbin.shape[1], imbin.shape[0], ImageFormat.IF_MONO, BitDepth.BD_8, d);
            return res.sliced[0 .. $, 0 .. $, 0];
        }

        // --- Modified calculateWidthMap function ---
        // Instead of filling a region mask with a polygon, extract the sub-image from imbin within the bounding box
        // defined by regionContour, so that the actual shape in imbin is used for processing.
        auto calculateWidthMap(T)(T imbin, vec2i[] regionContour) {
            // Step 1: Calculate the bounding box from regionContour
            int xmin = regionContour[0].x;
            int xmax = regionContour[0].x;
            int ymin = regionContour[0].y;
            int ymax = regionContour[0].y;
            foreach(p; regionContour) {
                if(p.x < xmin) xmin = p.x;
                if(p.x > xmax) xmax = p.x;
                if(p.y < ymin) ymin = p.y;
                if(p.y > ymax) ymax = p.y;
            }
            int regionWidth = xmax - xmin + 1;
            int regionHeight = ymax - ymin + 1;

            // Step 2: Extract the sub-image from imbin corresponding to the bounding box
            auto regionImbin = imbin[ymin .. (ymax + 1), xmin .. (xmax + 1)];

            // Copy the sub-image into a contiguous mutable array
            ubyte[] regionData = new ubyte[regionWidth * regionHeight];
            foreach(y; 0 .. regionHeight) {
                foreach(x; 0 .. regionWidth) {
                    regionData[y * regionWidth + x] = regionImbin[y, x];
                }
            }

            // Make a copy of regionData for distance transform to preserve the original data
            ubyte[] originalRegionData = regionData.dup;

            // Step 3: Perform skeletonization on the sub-image using regionData
            auto regionImg = new Image(regionWidth, regionHeight, ImageFormat.IF_MONO, BitDepth.BD_8, regionData);
            auto skel = regionImg.sliced[0 .. regionHeight, 0 .. regionWidth, 0];
            skeletonizeImage(skel);
            auto skelPath = extractPath(skel, regionWidth, regionHeight);

            // Step 4: Convert originalRegionData to a 2D ND-slice and perform distance transform
            int shapeErr = 0;
            auto regionSlice = originalRegionData.sliced.reshape([cast(ptrdiff_t)regionHeight, cast(ptrdiff_t)regionWidth], shapeErr);
            Slice!(float*, 2) dt;
            Slice!(int*, 3) nearest;
            nijigenerate.core.cv.distancetransform.distanceTransform(regionSlice, dt, nearest);

            float[] widthMap;
            foreach(s; skelPath) {
                // Guard against out-of-bounds: valid indices are [0 .. shape-1]
                if (s.y < 0 || s.x < 0) continue;
                if (s.y >= dt.shape[0] || s.x >= dt.shape[1]) continue;
                if (dt[s.y, s.x] > 0) {
                    widthMap ~= 2 * dt[s.y, s.x];
                    debug(automesh_opt) writef("w, h = %d x %d, ", regionWidth, regionHeight);
                    debug(automesh_opt) writefln(" distance: %.2f", dt[s.y, s.x]);
                }
            }
            // To convert skeleton coordinates back to original image coordinates, use:
            // auto skelPathOriginal = skelPath.map!(p => vec2i(p.x + xmin, p.y + ymin)).array;
            return widthMap;
        }
        // --- End of calculateWidthMap function ---

        vec2 imgCenter = vec2(texW / 2, texH / 2);
        float size_avg = (texW + texH) / 2.0;
        float min_distance = max(max(texW, texH) / DIV_PER_PART, MIN_DISTANCE);

        vec2[] vertices;
        vec2[] vB1;
        double sumWidth = 0;
        double length = 0;
        double widthMapLength = 0;

        // Region extraction: using findContours instead of bwlabel block
        vec2i[][] regionContours;
        ContourHierarchy[] regionHierarchy;
        findContours(imbin.idup, regionContours, regionHierarchy, RetrievalMode.EXTERNAL, ApproximationMethod.SIMPLE);
        debug(automesh_opt_full) writefln("regionContours=%s", regionContours);
        debug(automesh_opt) writefln("Region contours=%d, Region hierarchy=%d", regionContours.length, regionHierarchy.length);

        typeof(regionContours) contourList;
        int regionCount = 0;
        foreach (region; regionContours) {
            regionCount++;
            contourList ~= region;
            // Apply calculateWidthMap for each region
            debug(automesh_opt) writefln("calculateWidthMap for region %d", regionCount);
            auto widthMap = calculateWidthMap(imbin, region);
            widthMapLength += widthMap.length;
            debug(automesh_opt) writefln("  region %d: widthMapLength=%0.2f", regionCount, widthMapLength);
            debug(automesh_opt_full) writefln("widthMap=%s", widthMap);
            float[] validWidth = widthMap.stdFilter!((x) => x > 0).array;
            sumWidth += validWidth.sum;
            length   += validWidth.length;
        }
        double avgWidth = sumWidth / length;
        double ratio = sumWidth / widthMapLength;
        debug(automesh_opt) { writefln("found=%d: avgW=%0.2f, len=%0.2f, avgW/len=%0.2f, ratio=%0.2f", contourList.length, avgWidth, length, avgWidth / length, ratio); }

        debug(automesh_opt) { writefln("contours=%d", contourList.length); }
        auto contourVec = contoursToVec2s(contourList);
        debug(automesh_opt) { writefln("contourVec=%d", contourVec.length); }

        if (contourVec.length < 3) return mesh;

        mesh.clear();

        bool sharpFlag = (avgWidth < LARGE_THRESHOLD) &&
                         ((length < LENGTH_THRESHOLD) || ((avgWidth / length) < RATIO_THRESHOLD));

        // Reduce vertices by resampling (with consideration for flip flag)
        vB1 ~= resampling(contourVec, min_distance, mirrorHoriz, axisHoriz, mirrorVert, axisVert);

        // Type A: sharp shapes
        if (sharpFlag) {
            auto vA = sampleExpandedFromThinned(vB1, size_avg * SHARP_EXPANSION_FACTOR);
            vertices ~= vA;
        } else {
            // Type B: unsharp shapes
            // B-1: add original resampled points
            // B-2: add vertices expanded in normal direction
            // B-3: add vertices contracted in normal direction
            // B-4: add vertices scaled around the centroid
            float[] scales = SCALES.dup;

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
                        if (samplingFlag <= 0) {
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

        vec4 bounds = vec4(0, 0, texW, texH);
        auto vert_ind = triangulate(vertices, bounds);
        vertices = vert_ind[0];
        auto tris = vert_ind[1];

        // Create "compensated" image from imbin for completeUncoveredArea
        auto compensated = dupMono(imbin);
        foreach(y; 0 .. compensated.shape[0]) {
            foreach(x; 0 .. compensated.shape[1]) {
                compensated[y, x] = compensated[y, x] != 0 ? 255 : 0;
            }
        }

        bool completeUncoveredArea(T)(T compensated, vec2[] vertices, vec3u[] tris, vec2[] contourVec, float min_distance, out vec2[] outVertices, out vec3u[] outTris) {
            import mir.ndslice.topology;
            int err;
            auto compensated1D = compensated.reshape([-1], err);
            fillPoly(compensated1D, texW, texH, bounds, vertices, tris, 0, cast(ubyte)0);
            int initialRemainingArea = compensated1D.map!(x => x != 0 ? 255 : 0).sum;
            if (initialRemainingArea == 0) { 
                outVertices = vertices;
                outTris = tris;
                return false;
            }
            
            // Select candidate points that are sufficiently far from existing vertices
            vec2[] filteredCandidates;
            foreach(p; contourVec) {
                bool skip = false;
                if (vertices.length > 0) {
                    foreach(v; vertices) {
                        if(distance(v, p) < min_distance * 0.5) {
                            skip = true;
                            break;
                        }
                    }
                }
                int x = cast(int)round(p.x);
                int y = cast(int)round(p.y);
                if(x >= 0 && x < texW && y >= 0 && y < texH) {
                    int idx = y * texW + x;
                    if(compensated1D[idx] == 255 && !skip)
                        filteredCandidates ~= p;
                }
            }
            if(filteredCandidates.length == 0) {
                outVertices = vertices;
                outTris = tris;
                return false;
            }
            
            // From the candidate points, select the one with the maximum number of uncovered pixels in its window
            int bestScore = -1;
            vec2 bestCandidate = filteredCandidates[0];
            int windowSize = cast(int)min_distance;
            foreach(p; filteredCandidates) {
                int x = cast(int)round(p.x);
                int y = cast(int)round(p.y);
                int x1 = max(0, x - windowSize);
                int y1 = max(0, y - windowSize);
                int x2 = min(texW - 1, x + windowSize);
                int y2 = min(texH - 1, y + windowSize);
                int score = 0;
                for (int j = y1; j <= y2; j++) {
                    for (int i = x1; i <= x2; i++) {
                        int idx = j * texW + i;
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
            vec2[] newFinalVertices = vertices ~ [ bestCandidate ];
            auto newVertsInd = triangulate(newFinalVertices, vec4(0, 0, texW, texH));
            auto newTriangles = newVertsInd[1];
            auto newVertices = newVertsInd[0];
            if(newTriangles !is null) {
                foreach(i, tri; newTriangles) {
                    fillPoly(compensated1D, texW, texH, bounds, newVertices, newTriangles, i, cast(ubyte)0);                        
                }
                int remainingArea = compensated1D.map!(x => x != 0 ? 255 : 0).sum;
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
        newMesh.importVertsAndTris(vertices.map!((x){ return x - imgCenter; }).array, tris);
        mapImageCenteredMeshToTargetLocal(newMesh, target, ai);
        newMesh.refresh();
        return newMesh;
    }

    override void configure() {
        if (!presetName) {
            presetName = "Normal parts";
        }

        incText(_("Presets"));
        igIndent();
        if(igBeginCombo(__("Presets"), __(presetName))) {
            if (igSelectable(__("Normal parts"))) {
                presetName = "Normal parts";
                // Binarization / density
                MASK_THRESHOLD = 15;
                DIV_PER_PART = 12;
                MIN_DISTANCE = 16;
                SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
                // Heuristics
                LARGE_THRESHOLD = 400;
                LENGTH_THRESHOLD = 100;
                RATIO_THRESHOLD = 0.20;
                // Expand/contract factors
                SHARP_EXPANSION_FACTOR = 0.010;
                NONSHARP_EXPANSION_FACTOR = 0.050;
                NONSHARP_CONTRACTION_FACTOR = 0.050;
            }
            if (igSelectable(__("Detailed mesh"))) {
                presetName = "Detailed mesh";
                MASK_THRESHOLD = 15;
                DIV_PER_PART = 16; // denser
                MIN_DISTANCE = 12;
                SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
                LARGE_THRESHOLD = 380;
                LENGTH_THRESHOLD = 90;
                RATIO_THRESHOLD = 0.18;
                SHARP_EXPANSION_FACTOR = 0.008;
                NONSHARP_EXPANSION_FACTOR = 0.045;
                NONSHARP_CONTRACTION_FACTOR = 0.045;
            }
            if (igSelectable(__("Large parts"))) {
                presetName = "Large parts";
                MASK_THRESHOLD = 15;
                DIV_PER_PART = 8; // sparser
                MIN_DISTANCE = 24;
                SCALES = [1, 1.1, 0.9, 0.7, 0.4, 0.2, 0.1, 0];
                LARGE_THRESHOLD = 600;
                LENGTH_THRESHOLD = 200;
                RATIO_THRESHOLD = 0.30;
                SHARP_EXPANSION_FACTOR = 0.015;
                NONSHARP_EXPANSION_FACTOR = 0.080;
                NONSHARP_CONTRACTION_FACTOR = 0.080;
            }
            if (igSelectable(__("Small parts"))) {
                presetName = "Small parts";
                MASK_THRESHOLD = 15;
                DIV_PER_PART = 18; // denser
                MIN_DISTANCE = 12;
                SCALES = [1, 1.1, 0.6, 0.2];
                LARGE_THRESHOLD = 300;
                LENGTH_THRESHOLD = 80;
                RATIO_THRESHOLD = 0.15;
                SHARP_EXPANSION_FACTOR = 0.008;
                NONSHARP_EXPANSION_FACTOR = 0.040;
                NONSHARP_CONTRACTION_FACTOR = 0.040;
            }
            if (igSelectable(__("Thin and minimum parts"))) {
                presetName = "Thin and minimum parts";
                MASK_THRESHOLD = 1; // pick up very thin lines
                DIV_PER_PART = 24;  // highest density
                MIN_DISTANCE = 4;
                SCALES = [1];
                LARGE_THRESHOLD = 200;
                LENGTH_THRESHOLD = 60;
                RATIO_THRESHOLD = 0.10;
                SHARP_EXPANSION_FACTOR = 0.006;
                NONSHARP_EXPANSION_FACTOR = 0.030;
                NONSHARP_CONTRACTION_FACTOR = 0.030;
            }
            if (igSelectable(__("Preserve edges"))) {
                presetName = "Preserve edges";
                MASK_THRESHOLD = 15;
                DIV_PER_PART = 20; // dense but conservative
                MIN_DISTANCE = 8;
                SCALES = [1, 1.2, 0.8];
                LARGE_THRESHOLD = 350;
                LENGTH_THRESHOLD = 80;
                RATIO_THRESHOLD = 0.15;
                SHARP_EXPANSION_FACTOR = 0.010;
                NONSHARP_EXPANSION_FACTOR = 0.030;
                NONSHARP_CONTRACTION_FACTOR = 0.030;
            }
            igEndCombo();
        }
        igUnindent();

        igPushID("CONFIGURE_OPTIONS");
        // Simple parameters that drive the algorithm (no child window to avoid extra padding)
        if (incBeginCategory(__("Simple"))) {
            // Alpha mask binarization
            incText(_("Mask threshold"));
            igIndent();
            igPushID("MASK_THRESHOLD");
                igSetNextItemWidth(96);
                if (incDragFloat(
                    "mask_threshold", &MASK_THRESHOLD, 1,
                    1, 200, "%.2f", ImGuiSliderFlags.NoRoundToFormat)
                ) {
                    // no-op
                }
            igPopID();
            igUnindent();

            // Vertex density relative to part size
            incText(_("Vertex density (div per part)"));
            igIndent();
            igPushID("DIV_PER_PART");
                igSetNextItemWidth(96);
                if (incDragFloat(
                    "div_per_part", &DIV_PER_PART, 0.5,
                    4, 64, "%.1f", ImGuiSliderFlags.NoRoundToFormat)
                ) {
                    // used in sampling distance derivation
                }
            igPopID();
            igUnindent();
        }
        incEndCategory();

        // Advanced parameters, for fine tuning (single child or none to avoid nested scrollbars)
        if (incBeginCategory(__("Advanced"))) {
            // Keep content inline; avoid inner child windows to prevent multi-scrollbars
            // Absolute minimum distance clamp
            incText(_("Distance between vertices"));
            igIndent();
                incText(_("Minimum"));
                igIndent();
                    igPushID("MIN_DISTANCE");
                        igSetNextItemWidth(96);
                        if (incDragFloat(
                            "min_distance", &MIN_DISTANCE, 1,
                            1, 200, "%.2f", ImGuiSliderFlags.NoRoundToFormat)
                        ) {
                            // no-op
                        }
                    igPopID();
                igUnindent();
            igUnindent();

            // Sharp/unsharp heuristics
            incText(_("Sharpness heuristics"));
            igSameLine(0, 4);
            igTextDisabled("(?)");
            incTooltip(_("Classifies shape as 'sharp' to choose vertex strategy.\n- large_threshold: upper bound for average thickness along skeleton.\n- length_threshold: upper bound for skeleton length (short = sharper).\n- ratio_threshold: upper bound for avg_thickness_per_point (smaller = sharper)."));
            igIndent();
                igPushID("SHARP_THRESH");
                    igSetNextItemWidth(120);
                    incDragFloat("large_threshold", &LARGE_THRESHOLD, 5, 50, 2000, "%.0f", ImGuiSliderFlags.NoRoundToFormat);
                    incTooltip(_("Avg thickness threshold. Lower = more parts treated as sharp."));
                    igSetNextItemWidth(120);
                    incDragFloat("length_threshold", &LENGTH_THRESHOLD, 5, 20, 2000, "%.0f", ImGuiSliderFlags.NoRoundToFormat);
                    incTooltip(_("Skeleton length threshold. Lower = shorter shapes treated as sharp."));
                    igSetNextItemWidth(120);
                    incDragFloat("ratio_threshold", &RATIO_THRESHOLD, 0.01, 0.01, 1.0, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
                    incTooltip(_("Avg thickness per point threshold (avg/len). Lower = thinner shapes treated as sharp."));
                igPopID();
            igUnindent();

            incText(_("Expand/Contract factors"));
            igSameLine(0, 4);
            igTextDisabled("(?)");
            incTooltip(_("Offsets vertices outward/inward from the thinned contour.\n- sharp_expand: outward offset for sharp shapes.\n- unsharp_expand: outward offset for non-sharp shapes.\n- unsharp_contract: inward offset for non-sharp shapes."));
            igIndent();
                igPushID("FACTORS");
                    igSetNextItemWidth(120);
                    incDragFloat("sharp_expand", &SHARP_EXPANSION_FACTOR, 0.005, 0.0, 0.2, "%.3f", ImGuiSliderFlags.NoRoundToFormat);
                    incTooltip(_("Outward offset for sharp shapes (scaled by part size)."));
                    igSetNextItemWidth(120);
                    incDragFloat("unsharp_expand", &NONSHARP_EXPANSION_FACTOR, 0.005, 0.0, 0.5, "%.3f", ImGuiSliderFlags.NoRoundToFormat);
                    incTooltip(_("Outward offset for non-sharp shapes (scaled by part size)."));
                    igSetNextItemWidth(120);
                    incDragFloat("unsharp_contract", &NONSHARP_CONTRACTION_FACTOR, 0.005, 0.0, 0.5, "%.3f", ImGuiSliderFlags.NoRoundToFormat);
                    incTooltip(_("Inward offset for non-sharp shapes (scaled by part size)."));
                igPopID();
            igUnindent();

            // Scales list (no child window; inline list)
            int deleteIndex = -1;
            incText("Scales");
            igIndent();
                igPushID("SCALES");
                    if (SCALES.length > 0) {
                        foreach(i, ref s; SCALES) {
                            igSetNextItemWidth(96);
                            igPushID(cast(int)i);
                                incDragFloat("scale", &SCALES[i], 0.01, 0, 2, "%.2f", ImGuiSliderFlags.NoRoundToFormat);
                                igSameLine(0, 0);
                                if (i == SCALES.length - 1) {
                                    incDummy(ImVec2(-52, 32));
                                    igSameLine(0, 0);
                                    if (incButtonColored("", ImVec2(24, 24))) deleteIndex = cast(int)i;
                                    igSameLine(0, 0);
                                    if (incButtonColored("", ImVec2(24, 24))) SCALES ~= 1.0;
                                } else {
                                    incDummy(ImVec2(-28, 32));
                                    igSameLine(0, 0);
                                    if (incButtonColored("", ImVec2(24, 24))) deleteIndex = cast(int)i;
                                }
                            igPopID();
                        }
                    } else {
                        incDummy(ImVec2(-28, 24));
                        igSameLine(0, 0);
                        if (incButtonColored("", ImVec2(24, 24))) SCALES ~= 1.0;
                    }
                igPopID();
            igUnindent();
            incTooltip(_("Specifying scaling factor to apply for contours. If multiple scales are specified, vertices are populated per scale factors."));
            if (deleteIndex != -1) SCALES = SCALES.remove(cast(uint)deleteIndex);
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
        return "";
    }
};
