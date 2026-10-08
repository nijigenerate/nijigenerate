module nijigenerate.core.math.welding;

import nijilive.math : vec2, mat4;
import std.algorithm : sort;
import std.math : isFinite;

struct WeldingMeshRefinement {
    vec2[] vertices;
    ushort[] indices;
    size_t addedVertices;
}

struct WeldingIndexPair {
    ptrdiff_t[] forward;
    ptrdiff_t[] reverse;
}

/** Preserve existing edges across vertex edits; removed vertices are never re-matched. */
WeldingIndexPair ngRemapWeldingLinks(const(ptrdiff_t)[] forward, const(ptrdiff_t)[] reverse,
    const(ptrdiff_t)[] oldToNew, size_t newCount) {
    WeldingIndexPair result;
    result.forward.length = newCount;
    result.reverse.length = reverse.length;
    result.forward[] = -1;
    result.reverse[] = -1;
    foreach (i, target; forward) {
        if (i < oldToNew.length && oldToNew[i] >= 0 && oldToNew[i] < newCount &&
            target >= 0 && target < reverse.length && reverse[target] >= 0)
            result.forward[oldToNew[i]] = target;
    }
    foreach (i, source; reverse) {
        if (source >= 0 && source < oldToNew.length && oldToNew[source] >= 0 && oldToNew[source] < newCount &&
            source < forward.length && forward[source] >= 0)
            result.reverse[i] = oldToNew[source];
    }
    // If a dense-side representative was removed, keep its surviving group.
    foreach (i, target; result.forward) {
        if (target >= 0 && result.reverse[target] < 0) result.reverse[target] = cast(ptrdiff_t)i;
    }
    return result;
}

private size_t[2][] boundaryEdges(const(vec2)[] vertices, const(ushort)[] indices) {
    import std.math : round;
    size_t[long[2]] canonical;
    auto ids = new size_t[vertices.length];
    foreach (i, point; vertices) {
        long[2] key = [cast(long)round(point.x * 10000), cast(long)round(point.y * 10000)];
        if (auto previous = key in canonical) ids[i] = *previous;
        else { ids[i] = i; canonical[key] = i; }
    }
    size_t[size_t[2]] counts;
    size_t[2][size_t[2]] representatives;
    for (size_t t = 0; t + 2 < indices.length; t += 3) {
        auto a = indices[t], b = indices[t + 1], c = indices[t + 2];
        if (a >= vertices.length || b >= vertices.length || c >= vertices.length) continue;
        auto ab = vertices[b] - vertices[a], ac = vertices[c] - vertices[a];
        import std.math : abs;
        if (abs(ab.x * ac.y - ab.y * ac.x) <= 0.0001f) continue;
        size_t[3] triangle = [a, b, c];
        foreach (i; 0 .. 3) {
            size_t x = triangle[i], y = triangle[(i + 1) % 3];
            size_t[2] key = ids[x] < ids[y] ? [ids[x], ids[y]] : [ids[y], ids[x]];
            counts[key]++;
            representatives[key] = [x, y];
        }
    }
    size_t[2][] result;
    foreach (key, count; counts) if (count == 1) result ~= representatives[key];
    result.sort!((a, b) => a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]));
    return result;
}

/** Insert missing seam samples into boundary edges without moving existing vertices. */
WeldingMeshRefinement ngRefineWeldingMesh(const(vec2)[] fineWorld, const(ushort)[] fineIndices,
    const(vec2)[] coarseWorld, const(vec2)[] coarseLocal, const(ushort)[] coarseIndices,
    mat4 worldToCoarse, float distanceLimit = 4) {
    import nijilive.math : vec4;
    import std.algorithm : min, max;
    WeldingMeshRefinement result;
    result.vertices = coarseLocal.dup;
    result.indices = coarseIndices.dup;
    auto world = coarseWorld.dup;
    bool[] boundary;
    boundary.length = fineWorld.length;
    auto spacing = new float[fineWorld.length];
    spacing[] = float.infinity;
    foreach (edge; boundaryEdges(fineWorld, fineIndices)) {
        boundary[edge[0]] = boundary[edge[1]] = true;
        auto delta = fineWorld[edge[1]] - fineWorld[edge[0]];
        auto length = delta.x * delta.x + delta.y * delta.y;
        foreach (index; edge) spacing[index] = min(spacing[index], length);
    }
    auto limit = distanceLimit * distanceLimit;
    auto anchors = new bool[coarseWorld.length];
    foreach (j, existing; coarseWorld) foreach (point; fineWorld) {
        auto delta = point - existing;
        if (delta.x * delta.x + delta.y * delta.y < limit) { anchors[j] = true; break; }
    }
    bool insideCoarse(vec2 point) {
        for (size_t t = 0; t + 2 < coarseIndices.length; t += 3) {
            auto a = coarseIndices[t], b = coarseIndices[t + 1], c = coarseIndices[t + 2];
            if (a >= coarseWorld.length || b >= coarseWorld.length || c >= coarseWorld.length) continue;
            auto ab = coarseWorld[b] - coarseWorld[a], ac = coarseWorld[c] - coarseWorld[a];
            auto p = point - coarseWorld[a];
            auto determinant = ab.x * ac.y - ab.y * ac.x;
            import std.math : abs;
            if (abs(determinant) <= 0.0001f) continue;
            auto u = (p.x * ac.y - p.y * ac.x) / determinant;
            auto v = (ab.x * p.y - ab.y * p.x) / determinant;
            if (u >= 0 && v >= 0 && u + v <= 1) return true;
        }
        return false;
    }
    foreach (i, point; fineWorld) {
        if (!boundary[i] || !isFinite(point.x) || !isFinite(point.y)) continue;
        bool matched;
        foreach (existing; world) {
            auto delta = point - existing;
            if (delta.x * delta.x + delta.y * delta.y < limit) { matched = true; break; }
        }
        if (matched) continue;
        // A sampled curve can lie farther from its coarse chord than the vertex
        // matching tolerance. Extend only the exterior contour beside an existing
        // seam anchor, using the fine contour's local sampling interval.
        float reach = limit;
        if (isFinite(spacing[i]) && !insideCoarse(point)) {
            foreach (edge; boundaryEdges(coarseWorld, coarseIndices)) {
                if (!anchors[edge[0]] && !anchors[edge[1]]) continue;
                auto start = coarseWorld[edge[0]], delta = coarseWorld[edge[1]] - start;
                auto length = delta.x * delta.x + delta.y * delta.y;
                if (length <= 0) continue;
                auto offset = point - start;
                auto t = min(1f, max(0f, (offset.x * delta.x + offset.y * delta.y) / length));
                auto error = point - (start + delta * t);
                if (error.x * error.x + error.y * error.y < spacing[i]) {
                    reach = max(reach, spacing[i]);
                    break;
                }
            }
        }
        float nearest = reach;
        size_t[2] selected;
        bool found;
        foreach (edge; boundaryEdges(world, result.indices)) {
            auto start = world[edge[0]], delta = world[edge[1]] - start;
            auto length = delta.x * delta.x + delta.y * delta.y;
            if (length <= 0) continue;
            auto offset = point - start;
            auto t = min(1f, max(0f, (offset.x * delta.x + offset.y * delta.y) / length));
            if (t <= 0.0001f || t >= 0.9999f) continue;
            auto error = point - (start + delta * t);
            auto distance = error.x * error.x + error.y * error.y;
            if (distance < nearest) { nearest = distance; selected = edge; found = true; }
        }
        if (!found || result.vertices.length >= ushort.max) continue;
        auto vertex = cast(ushort)result.vertices.length;
        for (size_t t = 0; t + 2 < result.indices.length; t += 3) {
            bool split;
            foreach (j; 0 .. 3) {
                auto a = result.indices[t + j], b = result.indices[t + (j + 1) % 3];
                if (!((a == selected[0] && b == selected[1]) ||
                      (a == selected[1] && b == selected[0]))) continue;
                auto c = result.indices[t + (j + 2) % 3];
                result.indices[t .. t + 3] = [a, vertex, c];
                result.indices ~= [vertex, b, c];
                split = true;
                break;
            }
            if (split) break;
        }
        world ~= point;
        result.vertices ~= (worldToCoarse * vec4(point, 0, 1)).xy;
        result.addedVertices++;
    }
    return result;
}

/** Match seam vertices to their nearest anchor, retaining the target seam on mesh updates. */
ptrdiff_t[] ngMatchWeldingVertices(const(vec2)[] source, const(vec2)[] target,
    const(ptrdiff_t)[] previous = null, float distanceLimit = 4) {
    struct Candidate {
        size_t source;
        size_t target;
        float distance;
    }
    bool[] allowed;
    allowed.length = target.length;
    if (previous is null) {
        allowed[] = true;
    } else {
        foreach (index; previous) {
            if (index >= 0 && index < target.length) allowed[index] = true;
        }
    }
    Candidate[] candidates;
    foreach (i, a; source) {
        if (!isFinite(a.x) || !isFinite(a.y)) continue;
        foreach (j, b; target) {
            if (!allowed[j] || !isFinite(b.x) || !isFinite(b.y)) continue;
            auto delta = a - b;
            auto distance = delta.x * delta.x + delta.y * delta.y;
            if (distance < distanceLimit * distanceLimit) candidates ~= Candidate(i, j, distance);
        }
    }
    candidates.sort!((a, b) => a.distance < b.distance ||
        (a.distance == b.distance && (a.source < b.source ||
            (a.source == b.source && a.target < b.target))));
    auto indices = new ptrdiff_t[source.length];
    indices[] = -1;
    foreach (candidate; candidates) {
        if (indices[candidate.source] >= 0) continue;
        indices[candidate.source] = cast(ptrdiff_t)candidate.target;
    }
    return indices;
}

unittest {
    auto source = [vec2(0, 0), vec2(0.1f, 0), vec2(10, 0)];
    auto target = [vec2(0, 0), vec2(10, 0)];
    assert(ngMatchWeldingVertices(source, target) == [0, 0, 1]);
    assert(ngMatchWeldingVertices(source, target, [1]) == [-1, -1, 1]);
    assert(ngMatchWeldingVertices(source, []) == [-1, -1, -1]);
    assert(ngMatchWeldingVertices([vec2(float.nan, 0)], target) == [-1]);
}
