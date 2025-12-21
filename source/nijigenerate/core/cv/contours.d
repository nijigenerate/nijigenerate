module nijigenerate.core.cv.contours;

import std.array;
import std.algorithm;
import std.range;
import std.conv;
import std.typecons;
import std.math;
import std.algorithm;
import mir.rc.array;    // use mir.rc.array
import inmath;         // vec2i lives in inmath
import core.thread.fiber;

/****************************************************
 * D implementation equivalent to OpenCV cv::findContours (improved)
 *
 * Main improvements:
 * 1. Reduced allocations: reserve/reuse dynamic arrays
 * 2. Optimized inner loops: use a direction mapping table for 8-neighbor search
 * 3. Faster interior fill: switch to scanline fill
 * 4. More efficient hierarchy build: precompute contour bounding boxes to reduce containment checks
 ****************************************************/

/// Contour retrieval modes
enum RetrievalMode {
    EXTERNAL,  // outer contours only
    LIST,      // all contours (ignore hierarchy)
    CCOMP,     // two-level hierarchy (outer and holes)
    TREE       // full hierarchy
}

/// Contour approximation methods
enum ApproximationMethod {
    NONE,      // keep all points
    SIMPLE,    // simple removal of redundant collinear points
    TC89_L1,   // Douglas-Peucker (TC89_L1)
    TC89_KCOS  // Douglas-Peucker (TC89_KCOS)
}

/// Hierarchy info (OpenCV-compatible format)
struct ContourHierarchy {
    int next;   // next contour at same level
    int prev;   // previous contour at same level
    int child;  // first child contour
    int parent; // parent contour
};

/// Simple bounding box struct
struct BoundingBox {
    int minX, minY, maxX, maxY;
};

/// Check if coordinates are within image bounds
bool inBounds(int x, int y, int width, int height) {
    return (x >= 0 && x < width && y >= 0 && y < height);
}

/// Offsets for 8 directions (right-hand rule), clockwise order
/// [ 0: E, 1: NE, 2: N, 3: NW, 4: W, 5: SW, 6: S, 7: SE ]
static immutable int[2][8] DIRECTIONS = [
    [0, 1],
    [-1, 1],
    [-1, 0],
    [-1, -1],
    [0, -1],
    [1, -1],
    [1, 0],
    [1, 1]
];

/// Mapping table to get the index from (dx,dy) instantly
/// Index: [dy+1][dx+1] (center invalid: -1)
enum int[3][3] DIR_MAP = [
    [ 3, 4, 5 ],  // dy = -1, dx = -1,0,1
    [ 2, -1, 6 ], // dy =  0, dx = -1,0,1 (center invalid)
    [ 1, 0, 7 ]   // dy =  1, dx = -1,0,1
];

/// Suzuki-Abe contour tracing (right-hand rule)
/// Improvements:
/// - Reserve contour array up front to reduce reallocations
/// - Use DIR_MAP for direction checks to reduce loop iterations
vec2i[] suzukiAbeContour(T)(in T image, ref int[][] labels, int label, int startX, int startY, int width, int height) {
    vec2i b = vec2i(startX, startY);
    vec2i c = b;
    // Initial p is above the start point
    vec2i p = vec2i(startX, startY - 1);
    vec2i[] contour;
    // Initial reserve (rough estimate)
    contour.reserve(256);
    contour ~= b;
    labels[startY][startX] = label;
    bool firstIteration = true;

    while (true) {
        // Use table lookup for relative position (dx,dy) of p to c
        int dx = cast(int)p.x - cast(int)c.x;
        int dy = cast(int)p.y - cast(int)c.y;
        int startDir = 0;
        // Get from table. Center (0,0) yields -1, so use 0 in that case
        int tableVal = (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) ? DIR_MAP[dy + 1][dx + 1] : -1;
        if(tableVal != -1)
            startDir = (tableVal + 1) % 8;
        else
            startDir = 0;

        bool found = false;
        vec2i next;
        int nextDir = startDir;
        // Search 8 directions
        for (size_t i = 0; i < 8; i++) {
            int idx = (startDir + i) % 8;
            int nx = c.x + DIRECTIONS[idx][0];
            int ny = c.y + DIRECTIONS[idx][1];
            
            if (!inBounds(nx, ny, width, height))
                continue;
            // Interior (-1) is not revisited; already-labeled pixels with current contour id are allowed
            if (image[ny, nx] != 0 && (labels[ny][nx] == 0 || labels[ny][nx] == label || (nx == b.x && ny == b.y && !firstIteration))) {
                next = vec2i(nx, ny);
                nextDir = idx;
                found = true;
                break;
            }
        }
        if (!found)
            break;

        // Update p: next candidate is shifted to (nextDir + 7) mod 8 direction
        {
            int pdx = DIRECTIONS[(nextDir + 7) % 8][0];
            int pdy = DIRECTIONS[(nextDir + 7) % 8][1];
            p = vec2i(c.x + pdx, c.y + pdy);
        }
        c = next;

        // Skip label update on revisits
        if (!(c.x == b.x && c.y == b.y))
            labels[c.y][c.x] = label;

        contour ~= c;

        if (!firstIteration && c.x == b.x && c.y == b.y)
            break;

        firstIteration = false;
    }

    return contour;
}

/// Fill contour interior using a scanline method.
/// Improvement: instead of calling pointInPolygon for each pixel in the bounding box,
/// compute contour intersections on each scanline and fill spans between them.
void fillContourInterior(ref int[][] labels, vec2i[] contour, int width, int height) {
    // Compute bounding box
    int minX = contour[0].x, maxX = contour[0].x;
    int minY = contour[0].y, maxY = contour[0].y;
    foreach (pt; contour) {
        minX = min(minX, pt.x);
        maxX = max(maxX, pt.x);
        minY = min(minY, pt.y);
        maxY = max(maxY, pt.y);
    }
    // Ignore out-of-range and scan within the bounding box
    for (int y = minY; y <= maxY; y++) {
        double[] inters;
        // For each contour edge, compute intersections with scanline y
        for (size_t i = 0; i < contour.length; i++) {
            vec2i a = contour[i];
            vec2i b = contour[(i + 1) % contour.length];
            // Check whether the edge crosses the scanline
            if ((a.y <= y && b.y > y) || (a.y > y && b.y <= y)) {
                // x coordinate of the intersection (linear interpolation)
                double atX = a.x + (cast(double)(y - a.y) / (b.y - a.y)) * (b.x - a.x);
                inters ~= atX;
            }
        }
        // If there are at least two intersections, sort and fill in pairs
        if (inters.length >= 2) {
            inters.sort();
            // Fill each pair
            for (size_t i = 0; i < inters.length - 1; i += 2) {
                int start = cast(int)ceil(inters[i]);
                int end   = cast(int)floor(inters[i + 1]);
                // Fill pixels inside the bounding box that are unprocessed
                for (int x = start; x <= end; x++) {
                    if (inBounds(x, y, width, height) && labels[y][x] == 0)
                        labels[y][x] = -1;
                }
            }
        }
    }
}

/// Point-in-polygon test for p using standard ray casting (helper).
bool pointInPolygon(vec2i p, vec2i[] polygon) {
    size_t n = polygon.length;
    int cnt = 0;
    for (size_t i = 0; i < n; i++) {
        vec2i a = polygon[i];
        vec2i b = polygon[(i + 1) % n];
        if ((a.y > p.y) != (b.y > p.y)) {
            double atX = a.x + (cast(double)(p.y - a.y) / (b.y - a.y)) * (b.x - a.x);
            if (p.x < atX)
                cnt++;
        }
    }
    return (cnt & 1) == 1;
}

/// Determine containment between contours and build a hierarchy.
/// Improvement: precompute each contour's bounding box and use it as a containment prefilter.
void buildHierarchy(vec2i[][] contours, out ContourHierarchy[] hier) {
    size_t n = contours.length;
    hier.length = n;
    foreach (ref h; hier) {
        h.next = -1;
        h.prev = -1;
        h.child = -1;
        h.parent = -1;
    }
    
    // Compute bounding boxes for each contour
    BoundingBox[] boxes;
    boxes.length = n;
    foreach (i, contour; contours) {
        int minX = contour[0].x, maxX = contour[0].x;
        int minY = contour[0].y, maxY = contour[0].y;
        foreach (pt; contour) {
            minX = min(minX, pt.x);
            maxX = max(maxX, pt.x);
            minY = min(minY, pt.y);
            maxY = max(maxY, pt.y);
        }
        boxes[i] = BoundingBox(minX, minY, maxX, maxY);
    }
    
    for (size_t i = 0; i < n; i++) {
        vec2i rep = contours[i][0];
        int parentIndex = -1;
        // Start with this contour's bounding box
        auto bb = boxes[i];
        for (size_t j = 0; j < n; j++) {
            if (i == j) continue;
            // Check whether rep lies inside j's bounding box
            auto bbj = boxes[j];
            if (rep.x < bbj.minX || rep.x > bbj.maxX || rep.y < bbj.minY || rep.y > bbj.maxY)
                continue;
            // Check whether rep is actually inside the polygon
            if (pointInPolygon(rep, contours[j])) {
                // Narrow parent candidates by contour size (proxy for area)
                if (parentIndex == -1 || contours[j].length < contours[parentIndex].length)
                    parentIndex = cast(int)j;
            }
        }
        hier[i].parent = parentIndex;
        if (parentIndex != -1) {
            if (hier[parentIndex].child == -1)
                hier[parentIndex].child = cast(int)i;
            else {
                int sib = hier[parentIndex].child;
                while (hier[sib].next != -1)
                    sib = hier[sib].next;
                hier[sib].next = cast(int)i;
                hier[i].prev = sib;
            }
        }
    }
}

/// Contour approximation using Douglas-Peucker.
vec2i[] ramerDouglasPeucker(vec2i[] points, double epsilon) {
    if (points.length < 3)
        return points.dup;
    double maxDist = 0;
    int index = 0;
    for (size_t i = 1; i < points.length - 1; i++) {
        double d = distanceToSegment(points[i], points[0], points[$ - 1]);
        if (d > maxDist) {
            maxDist = d;
            index = cast(int)i;
        }
    }
    if (maxDist > epsilon) {
        auto left = ramerDouglasPeucker(points[0 .. index + 1], epsilon);
        auto right = ramerDouglasPeucker(points[index .. $], epsilon);
        return left ~ right[1 .. $];
    } else {
        return [points[0], points[$ - 1]];
    }
}

/// Compute distance between point p and segment (p1, p2).
double distanceToSegment(vec2i p, vec2i p1, vec2i p2) {
    double A = p.x - p1.x;
    double B = p.y - p1.y;
    double C = p2.x - p1.x;
    double D = p2.y - p1.y;
    double dot = A * C + B * D;
    double lenSq = C * C + D * D;
    double param = (lenSq != 0) ? dot / lenSq : -1;
    double xx, yy;
    if (param < 0) {
        xx = p1.x;
        yy = p1.y;
    } else if (param > 1) {
        xx = p2.x;
        yy = p2.y;
    } else {
        xx = p1.x + param * C;
        yy = p1.y + param * D;
    }
    return sqrt((p.x - xx) * (p.x - xx) + (p.y - yy) * (p.y - yy));
}

/// Contour approximation (supports multiple methods).
/// SIMPLE: remove redundant points on straight lines
/// TC89_L1 / TC89_KCOS: apply Douglas-Peucker (epsilon fixed to 2.0 here)
vec2i[] approximateContour(vec2i[] contour, ApproximationMethod method) {
    if (method == ApproximationMethod.NONE)
        return contour;
    else if (method == ApproximationMethod.SIMPLE) {
        if (contour.length < 3)
            return contour.dup;
        vec2i[] result;
        // Reserve in advance
        result.reserve(contour.length);
        result ~= contour[0];
        for (size_t i = 1; i < contour.length - 1; i++) {
            vec2i prev = contour[i - 1];
            vec2i curr = contour[i];
            vec2i next = contour[i + 1];
            if ((curr.x - prev.x) * (next.y - curr.y) != (curr.y - prev.y) * (next.x - curr.x))
                result ~= curr;
        }
        result ~= contour[$ - 1];
        return result;
    } else if (method == ApproximationMethod.TC89_L1 || method == ApproximationMethod.TC89_KCOS) {
        return ramerDouglasPeucker(contour, 2.0);
    }
    return contour;
}

/// Main findContours routine.
/// binaryImage: 3D array created with mir.rc.array (access via binaryImage[y, x])
/// labels: internal labels (initial value 0 = unprocessed)
void findContours(T)(in T binaryImage, out vec2i[][] contours, out ContourHierarchy[] hierarchyOut,
                     RetrievalMode mode = RetrievalMode.LIST,
                     ApproximationMethod method = ApproximationMethod.SIMPLE) {
    int height = cast(int)binaryImage.shape[0];
    int width  = cast(int)binaryImage.shape[1];
    
    // Allocate labels with required size up front
    int[][] labels;
    labels.length = height;
    foreach (ref row; labels)
        row.length = width;
    
    int label = 1;
    vec2i[][] localContours = [];
    ContourHierarchy[] hierarchyList;
    
    // Scan from the top-left (outer contour detection)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (binaryImage[y, x] != 0 && labels[y][x] == 0) {
                int left = (x - 1 >= 0) ? binaryImage[y, x - 1] : 0;
                if (left == 0) {
                    auto contour = suzukiAbeContour(binaryImage, labels, label, x, y, width, height);
                    // Fill interior region using scanline method
                    fillContourInterior(labels, contour, width, height);
                    contour = approximateContour(contour, method);
                    localContours ~= contour;
                    label++;
                } else {
                    labels[y][x] = -1;
                }
            }
            Fiber.yield;
        }
    }
    contours = localContours;
    
    // Build hierarchy (TREE/CCOMP modes)
    if (mode == RetrievalMode.TREE || mode == RetrievalMode.CCOMP) {
        buildHierarchy(contours, hierarchyList);
    } else {
        hierarchyList.length = contours.length;
        foreach (ref h; hierarchyList)
            h = ContourHierarchy(-1, -1, -1, -1);
    }
    
    // For EXTERNAL mode, return only contours without parents
    if (mode == RetrievalMode.EXTERNAL) {
        vec2i[][] extContours;
        ContourHierarchy[] extHierarchy;
        for (size_t i = 0; i < contours.length; i++) {
            if (hierarchyList[i].parent == -1) {
                extContours ~= contours[i];
                extHierarchy ~= hierarchyList[i];
            }
        }
        contours = extContours;
        hierarchyList = extHierarchy;
    }
    hierarchyOut = hierarchyList;
}

unittest {
    /*************************************
     * Sample: create a binary image using mir.rc.array
     * Image size: 5 x 8, channels: 1
     * Access via binaryImage[y, x]
     *************************************/
    auto binaryImage = rcarray!int([
        [[0],[0],[0],[0],[0],[0],[0],[0]],
        [[0],[1],[1],[1],[0],[1],[1],[0]],
        [[0],[1],[0],[1],[0],[1],[1],[0]],
        [[0],[1],[1],[1],[0],[0],[0],[0]],
        [[0],[0],[0],[0],[0],[0],[0],[0]]
    ]);
    
    vec2i[][] contours;
    ContourHierarchy[] hierarchy;
    
    // Use RETR_TREE for hierarchy, apply Douglas-Peucker with TC89_L1
    findContours(binaryImage, contours, hierarchy, RetrievalMode.TREE, ApproximationMethod.TC89_L1);
    
    // Simple assertions
    assert(contours.length > 0, "輪郭が検出されていません");
    assert(hierarchy.length == contours.length, "階層情報の数が輪郭数と一致していません");
    
    foreach (h; hierarchy) {
        assert(h.parent >= -1 && h.parent < cast(int)hierarchy.length, "不正な親インデックスがあります");
    }
}
