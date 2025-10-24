module nijigenerate.viewport.vertex.automesh.skeletonize;

import i18n;
import nijigenerate.viewport.vertex.automesh.automesh;
import nijigenerate.viewport.common.mesh;
import nijigenerate.widgets;
import nijilive.core;
import nijigenerate.core.math.skeletonize;
import nijigenerate.core.math.path;
import inmath;
import mir.ndslice;
import mir.ndslice.slice;    // NDslice slice operations
import mir.ndslice.topology; // for reshape(), etc.
import mir.math.stat: mean;
import nijigenerate.core.cv;
import std.algorithm;
//import std.stdio;
import std.math: abs;
import std.conv;
import std.array;
import std.algorithm.iteration: uniq;
import nijigenerate.viewport.vertex.automesh.alpha_provider;
import nijigenerate.core.cv.image;
import nijigenerate.viewport.vertex.automesh.common : getAlphaInput, mapImageCenteredMeshToTargetLocal;

alias Point = vec2u; // vec2u is (uint x, uint y)

/// SkeletonExtractor
/// - Input: image alpha; processes as ubyte grid accessible via img[y][x].
/// - In autoMesh(): binarize → Zhang-Suen skeletonization → path extraction → RDP-like simplification.
class SkeletonExtractor : AutoMeshProcessor {
private:
    float maskThreshold = 15;
    int targetPointCount = 10;
    Point[] controlPoints; // Control points after simplification
    // Unified alpha preview state
    private AlphaPreviewState _alphaPreview;

public:
    override IncMesh autoMesh(Deformable target, IncMesh mesh,
                              bool mirrorHoriz = false, float axisHoriz = 0,
                              bool mirrorVert = false, float axisVert = 0)
    {
        // 1) Branch only for AlphaInput acquisition
        auto ai = getAlphaInput(target);
        if (ai.w <= 0 || ai.h <= 0 || ai.img is null) return mesh;

        // 2) Common: binarization
        auto imbin = ai.img.sliced[0 .. $, 0 .. $, 3].dup;
        foreach (y; 0 .. imbin.shape[0])
        foreach (x; 0 .. imbin.shape[1])
            imbin[y, x] = imbin[y, x] < cast(ubyte)maskThreshold ? 0 : 255;

        // 3) Common: skeletonize → extract path → simplify
        skeletonizeImage(imbin);
        auto path = extractPath(imbin, ai.w, ai.h);
        controlPoints = simplifyByTargetCount(path, targetPointCount);

        // 4) Common: create vertices relative to image center
        mesh.clear();
        vec2 imgCenter = vec2(ai.w / 2, ai.h / 2);
        foreach (Point pt; controlPoints) {
            vec2 position = vec2(pt.x, pt.y);
            mesh.vertices ~= new MeshVertex(position - imgCenter);
        }

        // 5) Common: map to target local
        mapImageCenteredMeshToTargetLocal(mesh, target, ai);
        return mesh.autoTriangulate();
    }

    override void configure() {
        igSeparator();
        incText(_("Alpha Preview"));
        igIndent();
        alphaPreviewWidget(_alphaPreview, ImVec2(192, 192));
        igUnindent();
    }

    /// Get extracted control points
    Point[] getControlPoints() {
        return controlPoints;
    }

private:

    /////////////////////////////////////////////////////////////
    // Simplify to target count (Visvalingam–Whyatt-like)
    Point[] simplifyByTargetCount(Point[] pts, int targetCount) {
        // If already <= target, return copy
        if (pts.length <= targetCount)
            return pts.dup;

        // Iteratively remove interior points (keep endpoints)
        auto simplified = pts.dup;

        // Helper: triangle area from 3 points (a, b, c)
        auto triangleArea = (Point a, Point b, Point c) {
            double ax = cast(double)a.x, ay = cast(double)a.y;
            double bx = cast(double)b.x, by = cast(double)b.y;
            double cx = cast(double)c.x, cy = cast(double)c.y;
            double area = abs(ax*(by - cy) + bx*(cy - ay) + cx*(ay - by));
            return area / 2.0;
        };

        while (simplified.length > targetCount) {
            int removeIndex = 1;
            double minArea = triangleArea(simplified[0], simplified[1], simplified[2]);
            // Find interior point with smallest triangle area
            for (int i = 2; i < simplified.length - 1; i++) {
                double area = triangleArea(simplified[i - 1], simplified[i], simplified[i + 1]);
                if (area < minArea) {
                    minArea = area;
                    removeIndex = i;
                }
            }
            // Remove the smallest-area interior point
            simplified = simplified[0 .. removeIndex] ~ simplified[removeIndex + 1 .. $];
        }
        return simplified;
    }

public:
    override
    string icon() {
        return "\uebbb";
    }
}
