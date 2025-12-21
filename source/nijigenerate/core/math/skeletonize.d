module nijigenerate.core.math.skeletonize;

import mir.ndslice;
import inmath;
import std.algorithm;
import std.array;
import std.algorithm.iteration: uniq;
import core.thread.fiber;


/////////////////////////////////////////////////////////////
// 2. Zhang-Suen thinning algorithm (using NDslice)
void skeletonizeImage(T)(T imbin) {

    int countNeighbors(T)(T image, int i, int j) {
        int count = 0;
        // Offsets from -1 to +1 in the loops
        for (int di = -1; di <= 1; di++) {
            for (int dj = -1; dj <= 1; dj++) {
                if (di == 0 && dj == 0)
                    continue;
                if (image[i + di, j + dj])
                    count++;
            }
        }
        return count;
    }

    /// Return the number of background-to-foreground transitions among the 8 neighbors (p2-p9)
    int countTransitions(T)(T image, int i, int j) {
        ubyte[8] neighbors;
        neighbors[0] = image[i - 1, j];      // p2
        neighbors[1] = image[i - 1, j + 1];  // p3
        neighbors[2] = image[i, j + 1];      // p4
        neighbors[3] = image[i + 1, j + 1];  // p5
        neighbors[4] = image[i + 1, j];      // p6
        neighbors[5] = image[i + 1, j - 1];  // p7
        neighbors[6] = image[i, j - 1];      // p8
        neighbors[7] = image[i - 1, j - 1];  // p9

        int transitions = 0;
        foreach (k; 0 .. 8) {
            if (!neighbors[k] && neighbors[(k + 1) % 8])
                transitions++;
        }
        return transitions;
    }


    // Cache image size locally
    ulong h = imbin.shape[0];
    ulong w = imbin.shape[1];

    // Initial candidate set: all foreground pixels in the inner region (exclude borders)
    vec2u[] candidates;
    for (int i = 1; i < h - 1; i++) {
        for (int j = 1; j < w - 1; j++) {
            if (imbin[i, j])
                candidates ~= vec2u(j, i);  // vec2u(x, y)
        }
    }
    
    bool changedOverall = true;
    
    // Loop until the candidate set is empty or no changes occur
    while (changedOverall) {
        changedOverall = false;
        
        // --- Sub-iteration 1 ---
        vec2u[] toDelete;
        foreach (pt; candidates) {
            int i = pt.y;
            int j = pt.x;
            if (!imbin[i, j])
                continue;
            int bp = countNeighbors(imbin, i, j);
            if (bp < 2 || bp > 6)
                continue;
            int ap = countTransitions(imbin, i, j);
            if (ap != 1)
                continue;
            if (imbin[i - 1, j] && imbin[i, j + 1] && imbin[i + 1, j])
                continue;
            if (imbin[i, j + 1] && imbin[i + 1, j] && imbin[i, j - 1])
                continue;
            // If conditions are met, add to deletion set
            toDelete ~= pt;
        }
        
        vec2u[] newCandidates;
        if (toDelete.length > 0) {
            changedOverall = true;
            // Set deletions to 0 and add neighbors as new candidates
            foreach (pt; toDelete) {
                int i = pt.y;
                int j = pt.x;
                imbin[i, j] = 0;
                // Add neighbors (8 directions) as candidates (dedupe later)
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        int ni = i + di;
                        int nj = j + dj;
                        // Bounds check: exclude borders
                        if (ni < 1 || ni >= h - 1 || nj < 1 || nj >= w - 1)
                            continue;
                        newCandidates ~= vec2u(nj, ni);
                    }
                }
            }
            // Deduplicate: sort by y then x and uniq
            newCandidates.sort!((a, b) =>
                (a.y < b.y) || (a.y == b.y && a.x < b.x)
            );
            newCandidates = newCandidates.uniq.array;
        }
        
        // --- Sub-iteration 2 ---
        vec2u[] toDelete2;
        foreach (pt; newCandidates) {
            int i = pt.y;
            int j = pt.x;
            if (!imbin[i, j])
                continue;
            int bp = countNeighbors(imbin, i, j);
            if (bp < 2 || bp > 6)
                continue;
            int ap = countTransitions(imbin, i, j);
            if (ap != 1)
                continue;
            if (imbin[i - 1, j] && imbin[i, j + 1] && imbin[i, j - 1])
                continue;
            if (imbin[i - 1, j] && imbin[i + 1, j] && imbin[i, j - 1])
                continue;
            toDelete2 ~= pt;
        }
        
        vec2u[] newCandidates2;
        if (toDelete2.length > 0) {
            changedOverall = true;
            foreach (pt; toDelete2) {
                int i = pt.y;
                int j = pt.x;
                imbin[i, j] = 0;
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        int ni = i + di;
                        int nj = j + dj;
                        if (ni < 1 || ni >= h - 1 || nj < 1 || nj >= w - 1)
                            continue;
                        newCandidates2 ~= vec2u(nj, ni);
                    }
                }
            }
            newCandidates2.sort!((a, b) =>
                (a.y < b.y) || (a.y == b.y && a.x < b.x)
            );
            newCandidates2 = newCandidates2.uniq.array;
        }
        
        // Next candidates are those updated in sub-iteration 2
        candidates = newCandidates2;
        // If candidates are empty, rescan all pixels and add remaining foreground
        if (candidates.length == 0) {
            for (int i = 1; i < h - 1; i++) {
                for (int j = 1; j < w - 1; j++) {
                    if (imbin[i, j])
                        candidates ~= vec2u(j, i);
                }
            }
            // If no changes, end the algorithm
            if (candidates.length == 0)
                break;
        }
        Fiber.yield();
    }
}
