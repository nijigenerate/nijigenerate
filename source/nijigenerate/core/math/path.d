module nijigenerate.core.math.path;

import inmath;
import mir.ndslice;
import std.algorithm;
import core.thread.fiber;
import std.typecons : Tuple, tuple;
import std.array : array;

vec2u[] extractPath(T)(T skeleton, int width, int height)
{
    if (skeleton.length == 0) return [];
    
    // Global visited array for connected components (NDslice)
    ubyte[] globalVisitedData = new ubyte[height * width];
    int err;
    auto globalVisited = globalVisitedData.sliced.reshape([height, width], err);
    vec2u[] longestPathOverall;

    // Helper to convert 2D coordinates into a 1D index
    int encode(vec2u p) {
        return p.y * width + p.x;
    }

    // Collect connected component from the given point via BFS (queue uses head index for speed)
    vec2u[] getComponent(vec2u p) {
        vec2u[] comp;
        vec2u[] queue;
        size_t head = 0;
        queue ~= p;
        globalVisited[p.y, p.x] = 1;
        while(head < queue.length)
        {
            auto cur = queue[head];
            head++;
            comp ~= cur;
            for (int di = -1; di <= 1; di++) {
                for (int dj = -1; dj <= 1; dj++) {
                    if(di == 0 && dj == 0) continue;
                    int ny = cur.y + di;
                    int nx = cur.x + dj;
                    if(ny < 0 || ny >= height || nx < 0 || nx >= width)
                        continue;
                    if(skeleton[ny, nx] && !globalVisited[ny, nx]) {
                        globalVisited[ny, nx] = 1;
                        queue ~= vec2u(nx, ny);
                    }
                }
            }
        }
        return comp;
    }

    // Find neighbors within the component quickly using compMap
    vec2u[] getNeighbors(vec2u pt, bool[int] compMap) {
        vec2u[] neighbors;
        for (int di = -1; di <= 1; di++) {
            for (int dj = -1; dj <= 1; dj++) {
                if (di == 0 && dj == 0) continue;
                vec2u np = vec2u(pt.x + dj, pt.y + di);
                if (compMap.get(encode(np), false))
                    neighbors ~= np;
            }
        }
        return neighbors;
    }

    // Use BFS to find the farthest point from src within the component and its path
    Tuple!(vec2u, vec2u[]) bfsDiameter(vec2u src, vec2u[] comp, bool[int] compMap) {
        vec2u[] queue;
        int[] dist = new int[height * width];
        foreach (i; 0 .. dist.length)
            dist[i] = -1;
        vec2u[int] pred; // associative array storing the predecessor for each point

        int head = 0;
        queue ~= src;
        dist[encode(src)] = 0;
        while(head < queue.length) {
            auto cur = queue[head];
            head++;
            foreach (n; getNeighbors(cur, compMap)) {
                int enc = encode(n);
                if (dist[enc] == -1) {
                    dist[enc] = dist[encode(cur)] + 1;
                    pred[enc] = cur;
                    queue ~= n;
                }
            }
        }
        int maxDist = 0;
        vec2u farthest = src;
        foreach (pt; comp) {
            int d = dist[encode(pt)];
            if(d > maxDist) {
                maxDist = d;
                farthest = pt;
            }
            Fiber.yield;
        }
        vec2u[] pathReversed;
        auto cur = farthest;
        while(true) {
            pathReversed ~= cur;
            if(cur.x == src.x && cur.y == src.y)
                break;
            cur = pred[encode(cur)];
        }
        auto path = pathReversed.reverse.array;
        return tuple(farthest, path);
    }

    // Scan the entire skeleton and compute the diameter (longest path) per connected component
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (skeleton[i, j] && !globalVisited[i, j]) {
                // Get a new connected component
                auto comp = getComponent(vec2u(j, i));
                // Mark each point in the component via associative array (O(1) access)
                bool[int] compMap;
                foreach (pt; comp) {
                    // Ensure pt is a valid coordinate
                    if (pt.x < 0 || pt.x >= width || pt.y < 0 || pt.y >= height)
                        continue;
                    compMap[encode(pt)] = true;
                }
                Fiber.yield();
                // Find endpoints (one neighbor) using compMap instead of a linear scan
                vec2u[] endpoints;
                foreach (pt; comp) {
                    auto nbrs = getNeighbors(pt, compMap);
                    if (nbrs.length == 1)
                        endpoints ~= pt;
                    Fiber.yield;
                }
                // If endpoints exist, use one; otherwise use the first point as the diameter seed
                vec2u startForDiameter = (endpoints.length > 0) ? endpoints[0] : comp[0];
                auto result1 = bfsDiameter(startForDiameter, comp, compMap);
                auto result2 = bfsDiameter(result1[0], comp, compMap);
                if (result2[1].length > longestPathOverall.length)
                    longestPathOverall = result2[1];
            }
        }
    }
    return longestPathOverall;
}
