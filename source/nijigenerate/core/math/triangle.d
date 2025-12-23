module nijigenerate.core.math.triangle;

import nijigenerate.core.math.vertex;
import nijigenerate.viewport.common.mesh;
import nijilive.math.triangle;
import nijilive.math : Vec2Array;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;

import std.typecons;
import std.algorithm;
import std.algorithm.iteration : uniq;
import std.algorithm : sort;
import std.math : isClose;
import std.array;
import std.traits;
import core.exception;

private Vec2Array toVec2Array(T)(T[] vertices) {
    static if (is(T == Vec2Array))
        return vertices;
    else static if (is(T == vec2))
        return Vec2Array(vertices);
    else static if (is(T == MeshVertex*)) {
        Vec2Array result = Vec2Array(vertices.length);
        foreach (i, vtx; vertices) {
            result[i] = vtx.position;
        }
        return result;
    }
    else {
        static assert(false, "Unsupported vertex array type: "~T.stringof);
    }
}

Deformation* deformByDeformationBinding(DeformationParameterBinding binding, DeformationParameterBinding srcBinding, vec2u index, bool flipHorz = false) {
    if (!binding || !srcBinding) return null;
    if (auto drawable = cast(Drawable)binding.getTarget().node) {
        if (auto srcDrawable = cast(Drawable)srcBinding.getTarget().node) {
            auto mesh = new IncMesh(drawable.getMesh());
            Deformation deform = srcBinding.getValue(index);
            return deformByDeformationBinding(mesh.vertices, srcDrawable, deform, flipHorz);
        }
    } else if (auto grid = cast(GridDeformer)binding.getTarget().node) {
        if (auto srcGrid = cast(GridDeformer)srcBinding.getTarget().node) {
            Deformation deform = srcBinding.getValue(index);
            return deformByDeformationBinding(grid.vertices, srcGrid, deform, flipHorz);
        }
    } else if (auto deformable = cast(PathDeformer)binding.getTarget().node) {
        if (auto srcDeformable = cast(PathDeformer)srcBinding.getTarget().node) {
            Deformation deform = srcBinding.getValue(index);
            return deformByDeformationBinding(deformable.vertices, srcDeformable, deform, flipHorz);
        }
    }
    return null;
}

Deformation* deformByDeformationBinding(T)(T[] vertices, DeformationParameterBinding binding, vec2u index, bool flipHorz = false) {
    auto vecVertices = toVec2Array(vertices);
    if (!binding) {
        return null;
    }
    if (auto part = cast(Drawable)binding.getTarget().node) {
        Deformation deform = binding.getValue(index);
        return deformByDeformationBinding(vecVertices, part, deform, flipHorz);
    } else if (auto grid = cast(GridDeformer)binding.getTarget().node) {
        Deformation deform = binding.getValue(index);
        return deformByDeformationBinding(vecVertices, grid, deform, flipHorz);
    } else if (auto deformable = cast(PathDeformer)binding.getTarget().node) {
        Deformation deform = binding.getValue(index);
        return deformByDeformationBinding(vecVertices, deformable, deform, flipHorz);
    }
    return null;
}

Deformation* deformByDeformationBinding(S: Drawable)(Vec2Array vecVertices, S part, Deformation deform, bool flipHorz = false) {

    // Check whether deform has more than 1 triangle.
    // If not, returns default Deformation which has dummpy offsets.
    if (deform.vertexOffsets.length < 3 || vecVertices.length < 3 || part.getMesh().vertices.length < 3) {
        Vec2Array vertexOffsets;
        for (int i = 0; i < vecVertices.length; i++)
            vertexOffsets ~= vec2(0, 0);
        return new Deformation(vertexOffsets);
    }

    auto origVertices = vecVertices.dup;

    // find triangle which covers specified point. 
    // If no triangl is found, nearest triangl for the point is selected.
    int[] findSurroundingTriangle(vec2 pt, ref MeshData bindingMesh) {
        bool isPointInTriangle(vec2 pt, int[] triangle) {
            float sign (ref vec2 p1, ref vec2 p2, ref vec2 p3) {
                return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
            }
            vec2 p1 = bindingMesh.vertices[triangle[0]];
            vec2 p2 = bindingMesh.vertices[triangle[1]];
            vec2 p3 = bindingMesh.vertices[triangle[2]];

            auto d1 = sign(pt, p1, p2);
            auto d2 = sign(pt, p2, p3);
            auto d3 = sign(pt, p3, p1);

            auto hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
            auto hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

            return !(hasNeg && hasPos);
        }
        int i = 0;
        int[] triangle = [0, 1, 2];
        while (i < bindingMesh.indices.length) {
            triangle[0] = bindingMesh.indices[i];
            triangle[1] = bindingMesh.indices[i+1];
            triangle[2] = bindingMesh.indices[i+2];
            if (isPointInTriangle(pt, triangle)) {
                return triangle;
            }
            i += 3;
        }
        return null;
    }
    int[] findNearestTriangle(vec2 pt, ref MeshData bindingMesh) {
        int i = 0;
        int[] triangle = [0, 1, 2];
        float nearestDistance = -1;
        int nearestIndex = 0;
        while (i < bindingMesh.indices.length) {
            triangle[0] = bindingMesh.indices[i];
            triangle[1] = bindingMesh.indices[i+1];
            triangle[2] = bindingMesh.indices[i+2];
            auto d1 = (pt - bindingMesh.vertices[triangle[0]]).lengthSquared;
            auto d2 = (pt - bindingMesh.vertices[triangle[1]]).lengthSquared;
            auto d3 = (pt - bindingMesh.vertices[triangle[2]]).lengthSquared;
            auto dmin = min(d1, d2, d3);
            if (nearestDistance < 0 || dmin < nearestDistance) {
                nearestDistance = dmin;
                nearestIndex = i;
            }
            i += 3;
        }
        return [bindingMesh.indices[nearestIndex], 
                bindingMesh.indices[nearestIndex + 1], 
                bindingMesh.indices[nearestIndex + 2]];
    }
    // Calculate offset of point in coordinates of triangle.
    vec2 calcOffsetInTriangleCoords(vec2 pt, ref MeshData bindingMesh, ref int[] triangle) {
        auto p1 = bindingMesh.vertices[triangle[0]];
        if (pt == p1)
            return vec2(0, 0);
        auto p2 = bindingMesh.vertices[triangle[1]];
        auto p3 = bindingMesh.vertices[triangle[2]];
        vec2 axis0 = p2 - p1;
        float axis0len = axis0.length;
        axis0 /= axis0.length;
        vec2 axis1 = p3 - p1;
        float axis1len = axis1.length;
        axis1 /= axis1.length;
        vec3 raxis1 = mat3([axis0.x, axis0.y, 0, -axis0.y, axis0.x, 0, 0, 0, 1]) * vec3(axis1, 1);
        float cosA = raxis1.x;
        float sinA = raxis1.y;
        mat3 H = mat3([axis0len > 0? 1/axis0len: 0,                           0, 0,
                        0,                           axis1len > 0? 1/axis1len: 0, 0,
                        0,                                                     0, 1]) * 
                    mat3([1, -cosA/sinA, 0, 
                        0,     1/sinA, 0, 
                        0,          0, 1]) * 
                    mat3([ axis0.x, axis0.y, 0, 
                        -axis0.y, axis0.x, 0, 
                                0,       0, 1]) * 
                    mat3([1, 0, -(p1).x, 
                        0, 1, -(p1).y, 
                        0, 0,       1]);
        return (H * vec3(pt.x, pt.y, 1)).xy;
    }

    // Apply transform for mesh
    Vec2Array transformMesh(ref MeshData bindingMesh, Deformation deform) {
        Vec2Array result;
        if (bindingMesh.vertices.length != deform.vertexOffsets.length) {
            result.length = bindingMesh.vertices.length;
            return result;
        }
//            assert(bindingMesh.vertices.length == deform.vertexOffsets.length);
        foreach (i, v; bindingMesh.vertices) {
            result ~= v + deform.vertexOffsets[i];
        }
        return result;
    }

    // Calculate position of the vertex using coordinates of the triangle.      
    vec2 transformPointInTriangleCoords(vec2 pt, vec2 offset, Vec2Array vertices, ref int[] triangle) {
        auto p1 = vertices[triangle[0]];
        auto p2 = vertices[triangle[1]];
        auto p3 = vertices[triangle[2]];
        vec2 axis0 = p2 - p1;
        vec2 axis1 = p3 - p1;
        return p1 + axis0 * offset.x + axis1 * offset.y;
    }

    MeshData bindingMesh = part.getMesh();
    Deformation* newDeform = new Deformation;

    auto targetMesh = transformMesh(bindingMesh, deform);
    foreach (i, v; vecVertices) {
        vec2 pt = position(v);
        if (flipHorz)
            pt.x = -pt.x;
        int[] triangle = findSurroundingTriangle(pt, bindingMesh);
        vec2 newPos;
        if (triangle is null)
            triangle = findNearestTriangle(pt, bindingMesh);
        vec2 ofs = calcOffsetInTriangleCoords(pt, bindingMesh, triangle);
        newPos = transformPointInTriangleCoords(pt, ofs, targetMesh, triangle);
        if (flipHorz)
            newPos.x = -newPos.x;
        newDeform.vertexOffsets ~= newPos - position(origVertices[i]);
    }
    return newDeform;
}

Deformation* deformByDeformationBinding(T, S: Drawable)(T[] vertices, S part, Deformation deform, bool flipHorz = false) {
    return deformByDeformationBinding(toVec2Array(vertices), part, deform, flipHorz);
}

Deformation* deformByDeformationBinding(S: PathDeformer)(Vec2Array vecVertices, S deformable, Deformation deform, bool flipHorz = false) {
    // Check whether deform has more than 1 triangle.
    // If not, returns default Deformation which has dummpy offsets.
    if (deform.vertexOffsets.length < 2 || vecVertices.length < 2 || deformable.vertices.length < 2) {
        Vec2Array vertexOffsets;
        for (int i = 0; i < vecVertices.length; i++)
            vertexOffsets ~= vec2(0, 0);
        return new Deformation(vertexOffsets);
    }

    auto origControlPoints     = deformable.vertices.dup;
    auto deformedControlPoints = deformable.vertices.dup;
    foreach (i; 0..origControlPoints.length) {
        deformedControlPoints[i] += deform.vertexOffsets[i];
        if (flipHorz) {
            origControlPoints[i].x *= -1;
            deformedControlPoints[i].x *= -1;
        }
    }
    auto originalCurve = deformable.createCurve(origControlPoints);
    auto deformedCurve = deformable.createCurve(deformedControlPoints);

    Deformation* newDeform = new Deformation;

    foreach (i, v; vecVertices) {
        auto cVertex = position(v);
        float t = originalCurve.closestPoint(cVertex);
        vec2 closestPointOriginal = originalCurve.point(t);
        vec2 tangentOriginal = originalCurve.derivative(t).normalized;
        vec2 normalOriginal = vec2(-tangentOriginal.y, tangentOriginal.x);
        float originalNormalDistance = dot(cVertex - closestPointOriginal, normalOriginal); 
        float tangentialDistance = dot(cVertex - closestPointOriginal, tangentOriginal);

        // Find the corresponding point on the deformed Bezier curve
        vec2 closestPointDeformedA = deformedCurve.point(t); // 修正: deformedCurve を使用
        vec2 tangentDeformed = deformedCurve.derivative(t).normalized; // 修正: deformedCurve を使用
        vec2 normalDeformed = vec2(-tangentDeformed.y, tangentDeformed.x);

        // Adjust the vertex to maintain the same normal and tangential distances
        vec2 deformedVertex = closestPointDeformedA + normalDeformed * originalNormalDistance + tangentDeformed * tangentialDistance;

        newDeform.vertexOffsets ~= deformedVertex - cVertex;
    }
    return newDeform;
}

Deformation* deformByDeformationBinding(T, S: PathDeformer)(T[] vertices, S deformable, Deformation deform, bool flipHorz = false) {
    return deformByDeformationBinding(toVec2Array(vertices), deformable, deform, flipHorz);
}

Deformation* deformByDeformationBinding(S: GridDeformer)(Vec2Array vecVertices, S deformable, Deformation deform, bool flipHorz = false) {
    Vec2Array gridVerts = deformable.vertices.dup;
    Vec2Array offsets = deform.vertexOffsets.dup;
    if (gridVerts.length < 4 || offsets.length != gridVerts.length) {
        Vec2Array zeroOffsets;
        zeroOffsets.length = vecVertices.length;
        foreach (i; 0 .. zeroOffsets.length) zeroOffsets[i] = vec2(0, 0);
        auto def = new Deformation;
        def.vertexOffsets = zeroOffsets;
        return def;
    }

    float[] xsRaw;
    float[] ysRaw;
    xsRaw.length = gridVerts.length;
    ysRaw.length = gridVerts.length;
    foreach (i, v; gridVerts) {
        xsRaw[i] = v.x;
        ysRaw[i] = v.y;
    }
    xsRaw.sort();
    ysRaw.sort();
    auto xs = xsRaw.uniq.array;
    auto ys = ysRaw.uniq.array;
    size_t cols = xs.length;
    size_t rows = ys.length;
    if (cols < 2 || rows < 2 || cols * rows != gridVerts.length) {
        Vec2Array zeroOffsets;
        zeroOffsets.length = vecVertices.length;
        foreach (i; 0 .. zeroOffsets.length) zeroOffsets[i] = vec2(0, 0);
        auto def = new Deformation;
        def.vertexOffsets = zeroOffsets;
        return def;
    }

    Vec2Array samplePositions;
    samplePositions.length = vecVertices.length;
    foreach (i, vertex; vecVertices) {
        samplePositions[i] = position(vertex);
        if (flipHorz) {
            samplePositions[i].x = -samplePositions[i].x;
        }
    }

    float clampValue(float value, float minValue, float maxValue) {
        if (value < minValue) return minValue;
        if (value > maxValue) return maxValue;
        return value;
    }

    bool locateInterval(const float[] axis, float value, out size_t cell, out float weight) {
        if (axis.length < 2) return false;
        enum float maxRel = 1e-4f;
        enum float maxAbs = 1e-5f;
        if (value <= axis[0]) {
            if (isClose(value, axis[0], maxRel, maxAbs)) {
                cell = 0;
                weight = 0;
                return true;
            }
            return false;
        }
        if (value >= axis[$ - 1]) {
            if (isClose(value, axis[$ - 1], maxRel, maxAbs)) {
                cell = axis.length - 2;
                weight = 1;
                return true;
            }
            return false;
        }
        foreach (i; 0 .. axis.length - 1) {
            auto a = axis[i];
            auto b = axis[i + 1];
            if (value >= a && value <= b) {
                auto span = b - a;
                weight = span > 0 ? (value - a) / span : 0;
                weight = clampValue(weight, 0.0f, 1.0f);
                cell = i;
                return true;
            }
        }
        return false;
    }

    vec2 bilinear(vec2 p00, vec2 p10, vec2 p01, vec2 p11, float u, float v) {
        auto a = p00 * (1 - u) + p10 * u;
        auto b = p01 * (1 - u) + p11 * u;
        return a * (1 - v) + b * v;
    }

    Deformation* result = new Deformation;
    result.vertexOffsets.length = vecVertices.length;

    foreach (i, query; samplePositions) {
        size_t cellX, cellY;
        float u, v;
        if (!locateInterval(xs, query.x, cellX, u) || !locateInterval(ys, query.y, cellY, v)) {
            result.vertexOffsets[i] = vec2(0, 0);
            continue;
        }

        auto idx00 = cellY * cols + cellX;
        auto idx10 = cellY * cols + (cellX + 1);
        auto idx01 = (cellY + 1) * cols + cellX;
        auto idx11 = (cellY + 1) * cols + (cellX + 1);

        auto def00 = gridVerts[idx00] + offsets[idx00];
        auto def10 = gridVerts[idx10] + offsets[idx10];
        auto def01 = gridVerts[idx01] + offsets[idx01];
        auto def11 = gridVerts[idx11] + offsets[idx11];

        auto defPos = bilinear(def00, def10, def01, def11, u, v);
        if (flipHorz) {
            defPos.x = -defPos.x;
        }
        vec2 original = position(vecVertices[i]);
        result.vertexOffsets[i] = defPos - original;
    }

    return result;
}

Deformation* deformByDeformationBinding(T, S: GridDeformer)(T[] vertices, S deformable, Deformation deform, bool flipHorz = false) {
    return deformByDeformationBinding(toVec2Array(vertices), deformable, deform, flipHorz);
}


auto triangulate(T)(T[] vertices, vec4 bounds) {
    vec2 min, max;
    min = bounds.xy;
    max = bounds.zw;

    // Pad (fudge factors are a hack to work around contains() instability, TODO: fix)
    vec2 range = max - min;
    min -= range + vec2(range.y, range.x) + vec2(0.123, 0.125);
    max += range + vec2(range.y, range.x) + vec2(0.127, 0.129);

    vec3u[] tris;
    vec3u[] tri2edge;
    vec2u[] edge2tri;

    Vec2Array vtx;
    vtx.length = 4;

    // Define initial state (two tris)
    vtx[0] = vec2(min.x, min.y);
    vtx[1] = vec2(min.x, max.y);
    vtx[2] = vec2(max.x, max.y);
    vtx[3] = vec2(max.x, min.y);
    tris ~= vec3u(0, 1, 3);
    tris ~= vec3u(1, 2, 3);
    tri2edge ~= vec3u(0, 1, 2);
    tri2edge ~= vec3u(3, 4, 1);
    edge2tri ~= vec2u(0, 0);
    edge2tri ~= vec2u(0, 1);
    edge2tri ~= vec2u(0, 0);
    edge2tri ~= vec2u(1, 1);
    edge2tri ~= vec2u(1, 1);

    // Helpers
    float sign(vec2 p1, vec2 p2, vec2 p3) {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    }

    bool contains(vec3u tri, vec2 pt) {
        float d1, d2, d3;
        bool hasNeg, hasPos;

        d1 = sign(pt, vtx[tri.x], vtx[tri.y]);
        d2 = sign(pt, vtx[tri.y], vtx[tri.z]);
        d3 = sign(pt, vtx[tri.z], vtx[tri.x]);

        hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

        return !(hasNeg && hasPos);
    }

    void replaceE2T(ref vec2u e2t, uint from, uint to) {
        if (e2t.x == from) {
            e2t.x = to;
            if (e2t.y == from) e2t.y = to;
        } else if (e2t.y == from) {
            e2t.y = to;
        } else assert(false, "edge mismatch");
    }

    void orientTri(uint tri, uint edge) {
        vec3u t2e = tri2edge[tri];
        vec3u pt = tris[tri];
        if (t2e.x == edge) {
            return;
        } else if (t2e.y == edge) {
            tri2edge[tri] = vec3u(t2e.y, t2e.z, t2e.x);
            tris[tri] = vec3u(pt.y, pt.z, pt.x);
        } else if (t2e.z == edge) {
            tri2edge[tri] = vec3u(t2e.z, t2e.x, t2e.y);
            tris[tri] = vec3u(pt.z, pt.x, pt.y);
        } else {
            assert(false, "triangle does not own edge");
        }
    }

    void splitEdges() {
        uint edgeCnt = cast(uint)edge2tri.length;
        for(uint e = 0; e < edgeCnt; e++) {
            vec2u tr = edge2tri[e];

            if (tr.x != tr.y) continue; // Only handle outer edges

            orientTri(tr.x, e);

            uint t1 = tr.x;
            uint t2 = cast(uint)tris.length;
            uint l = tris[t1].x;
            uint r = tris[t1].y;
            uint z = tris[t1].z;
            uint m = cast(uint)vtx.length;
            vtx ~= (vtx[l] + vtx[r]) / 2;

            uint xe = cast(uint)edge2tri.length;
            uint me = xe + 1;
            uint re = tri2edge[t1].y;

            tris[t1].y = m;
            tri2edge[t1].y = me;
            tris ~= vec3u(m, r, z);
            tri2edge ~= vec3u(xe, re, me);
            edge2tri ~= vec2u(t2, t2);
            edge2tri ~= vec2u(t1, t2);
            replaceE2T(edge2tri[re], t1, t2);
        }
    }

    bool inCircle(vec2 pa, vec2 pb, vec2 pc, vec2 pd) {
        debug(delaunay) writefln("in_circle(%s, %s, %s, %s)", pa, pb, pc, pd);
        float adx = pa.x - pd.x;
        float ady = pa.y - pd.y;
        float bdx = pb.x - pd.x;
        float bdy = pb.y - pd.y;

        float adxbdy = adx * bdy;
        float bdxady = bdx * ady;
        float oabd = adxbdy - bdxady;

        if (oabd <= 0) return false;

        float cdx = pc.x - pd.x;
        float cdy = pc.y - pd.y;

        float cdxady = cdx * ady;
        float adxcdy = adx * cdy;
        float ocad = cdxady - adxcdy;

        if (ocad <= 0) return false;

        float bdxcdy = bdx * cdy;
        float cdxbdy = cdx * bdy;

        float alift = adx * adx + ady * ady;
        float blift = bdx * bdx + bdy * bdy;
        float clift = cdx * cdx + cdy * cdy;

        float det = alift * (bdxcdy - cdxbdy) + blift * ocad + clift * oabd;

        debug(delaunay) writefln("det=%s", det);
        return det > 0;
    }

    splitEdges();
    splitEdges();
    splitEdges();
    splitEdges();

    uint dropVertices = cast(uint)vtx.length;

    // Add vertices, preserving Delaunay condition
    foreach(orig_i, vertex; vertices) {
        uint i = cast(uint)orig_i + dropVertices;
        debug(delaunay) writefln("Add @%d: %s", i, vertex.position);
        vtx ~= vertex.position;
        bool found = false;

        uint[] affectedEdges;

        foreach(a_, tri; tris) {
            if (!contains(tri, vertex.position)) continue;

            /*
                        x
                Y-----------------X
                \`,            '/    XYZ = original vertices
                \ `q   a   p' /     a = original triangle
                    \  `,    '  /      bc = new triangles
                    \   `i'   /       xyz = original edges
                    y \ b | c / z      pqr = new edges
                    \  r  /
                        \ | /
                        \|/
                        Z
            */

            // Subdivide containing triangle
            // New triangles
            uint a = cast(uint)a_;
            uint b = cast(uint)tris.length;
            uint c = b + 1;
            tris[a] = vec3u(tri.x, tri.y, i);
            tris ~= vec3u(tri.y, tri.z, i); // b
            tris ~= vec3u(tri.z, tri.x, i); // c

            debug(delaunay) writefln("*** Tri %d: %s Edges: %s", a, tris[a], tri2edge[a]);

            // New inner edges
            uint p = cast(uint)edge2tri.length;
            uint q = p + 1;
            uint r = q + 1;

            // Get outer edges
            uint x = tri2edge[a].x;
            uint y = tri2edge[a].y;
            uint z = tri2edge[a].z;

            // Update triangle to edge mappings
            tri2edge[a] = vec3u(x, q, p);
            tri2edge ~= vec3u(y, r, q);
            tri2edge ~= vec3u(z, p, r);

            debug(delaunay) writefln("  * Tri a %d: %s Edges: %s", a, tris[a], tri2edge[a]);
            debug(delaunay) writefln("  + Tri b %d: %s Edges: %s", b, tris[b], tri2edge[b]);
            debug(delaunay) writefln("  + Tri c %d: %s Edges: %s", c, tris[c], tri2edge[c]);

            // Save new edges
            edge2tri ~= vec2u(c, a);
            edge2tri ~= vec2u(a, b);
            edge2tri ~= vec2u(b, c);
            debug(delaunay) writefln("  + Edg p %d: Tris %s", p, edge2tri[p]);
            debug(delaunay) writefln("  + Edg q %d: Tris %s", q, edge2tri[q]);
            debug(delaunay) writefln("  + Edg r %d: Tris %s", r, edge2tri[r]);

            // Update two outer edges
            debug(delaunay) writefln("  - Edg y %d: Tris %s", y, edge2tri[y]);
            replaceE2T(edge2tri[y], a, b);
            debug(delaunay) writefln("  + Edg y %d: Tris %s", y, edge2tri[y]);
            debug(delaunay) writefln("  - Edg z %d: Tris %s", y, edge2tri[z]);
            replaceE2T(edge2tri[z], a, c);
            debug(delaunay) writefln("  + Edg z %d: Tris %s", z, edge2tri[z]);

            // Keep track of what edges we have to look at
            affectedEdges ~= [x, y, z, p, q, r];

            found = true;
            break;
        }
        if (!found) {
            debug(delaunay) writeln("FAILED!");
            break;
        }

        bool[] checked;
        checked.length = edge2tri.length;

        for (uint j = 0; j < affectedEdges.length; j++) {
            uint e = affectedEdges[j];
            vec2u t = edge2tri[e];

            debug(delaunay) writefln(" ## Edge %d: T %s: %s %s", e, t, tris[t.x], tris[t.y]);

            if (t.x == t.y) {
                debug(delaunay) writefln("  + Outer edge");
                continue; // Outer edge
            }

            // Orient triangles so 1st edge is shared
            orientTri(t.x, e);
            orientTri(t.y, e);

            assert(tris[t.x].x == tris[t.y].y, "triangles do not share edge");
            assert(tris[t.y].x == tris[t.x].y, "triangles do not share edge");

            uint a = tris[t.x].x;
            uint c = tris[t.x].y;
            uint d = tris[t.x].z;
            uint b = tris[t.y].z;

            // Delaunay check
            if (!inCircle(vtx[b], vtx[a], vtx[c], vtx[d])) {
                // We're good
                debug(delaunay) writefln("  + Meets condition");
                continue;
            }

            debug(delaunay) writefln("  - Flip!");

            // Flip edge
            /*
                c          c
                /|\      r / \ q
                / | \      / x \
            d x|y b -> d-----b
                \ | /      \ y /
                \|/      s \ / p
                a          a
            */
            uint r = tri2edge[t.x].y;
            uint s = tri2edge[t.x].z;
            uint p = tri2edge[t.y].y;
            uint q = tri2edge[t.y].z;

            tris[t.x] = vec3u(d, b, c);
            tris[t.t] = vec3u(b, d, a);
            tri2edge[t.x] = vec3u(e, q, r);
            tri2edge[t.y] = vec3u(e, s, p);
            replaceE2T(edge2tri[q], t.y, t.x);
            replaceE2T(edge2tri[s], t.x, t.y);

            // Mark it as checked
            checked[e] = true;

            // Check the neighboring edges
            if (!checked[p]) affectedEdges ~= p;
            if (!checked[q]) affectedEdges ~= q;
            if (!checked[r]) affectedEdges ~= r;
            if (!checked[s]) affectedEdges ~= s;
        }
    }
    tris = tris.filter!((t)=>t.x >= dropVertices && t.y >= dropVertices && t.z >= dropVertices).array;
    if (dropVertices > 0 && dropVertices < vtx.length) {
        auto trimmed = Vec2Array(vtx.length - dropVertices);
        foreach (i; 0 .. trimmed.length) {
            trimmed[i] = vtx[i + dropVertices];
        }
        vtx = trimmed;
    }
    foreach (ref t; tris) {
        t.x -= dropVertices;
        t.y -= dropVertices;
        t.z -= dropVertices;
        if (t.x >= vtx.length || t.y >= vtx.length || t.z >= vtx.length) {
//            import std.stdio;
//            writefln("Triangulate: Error: %s exceeds %d", t, dropVertices);
        }
    }
    return tuple(vtx, tris);
}

vec4 getBounds(T)(ref T vertices) {
    vec4 bounds = vec4(float.max, float.max, -float.max, -float.max);
    foreach (v; vertices) {
        bounds = vec4(min(bounds.x, v.x), min(bounds.y, v.y), max(bounds.z, v.x), max(bounds.w, v.y));
    }
    bounds.x = floor(bounds.x);
    bounds.y = floor(bounds.y);
    bounds.z = ceil(bounds.z);
    bounds.w = ceil(bounds.w);
    return bounds;
}

void fillPoly(T, S, U, V)(T texture, ulong width, ulong height, vec4 bounds, S[] vertices , U[] indices, ulong index, V value) if (isNumeric!U) {
    if (vertices.length < 3) return;
    vec2[3] tvertices = [
        position(vertices[indices[3*index]]),
        position(vertices[indices[3*index+1]]),
        position(vertices[indices[3*index+2]])
    ];

    vec4 tbounds = getBounds(tvertices);
    int bwidth  = cast(int)(ceil(tbounds.z) - floor(tbounds.x) + 1);
    int bheight = cast(int)(ceil(tbounds.w) - floor(tbounds.y) + 1);
    int top  = cast(int)floor(tbounds.y);
    int left = cast(int)floor(tbounds.x);
    foreach (y; 0..bheight) {
        foreach (x; 0..bwidth) {
            vec2 pt = vec2(left + x, top + y);
            if (isPointInTriangle(pt, Vec2Array(tvertices[]))) {
                pt-= bounds.xy;
                if (cast(int)(pt.y * width + pt.x) >= texture.length) {
                    texture[cast(int)(pt.y * width + pt.x)] = value;
                } else {
                    texture[cast(int)(pt.y * width + pt.x)] = value;
                }
            }
        }
    }
}

void fillPoly(T, S, U, V)(T texture, ulong width, ulong height, vec4 bounds, S[] vertices , U[] indices, ulong index, V value) if (is(U: vec3u)) {
    if (vertices.length < 3 || indices.length < index) return;
    vec2[3] tvertices;
    if (index >= indices.length || 
        indices[index].x >= vertices.length || 
        indices[index].y >= vertices.length || 
        indices[index].z >= vertices.length) return;
    tvertices = [
        position(vertices[indices[index].x]),
        position(vertices[indices[index].y]),
        position(vertices[indices[index].z])
    ];

    vec4 tbounds = getBounds(tvertices);
    int bwidth  = cast(int)(ceil(tbounds.z) - floor(tbounds.x) + 1);
    int bheight = cast(int)(ceil(tbounds.w) - floor(tbounds.y) + 1);
    int top  = cast(int)floor(tbounds.y);
    int left = cast(int)floor(tbounds.x);
    foreach (y; max(0, bounds.y)..min(bheight, height)) {
        foreach (x; max(0, bounds.x)..min(bwidth, width)) {
            vec2 pt = vec2(left + x, top + y);
            if (isPointInTriangle(pt, Vec2Array(tvertices[]))) {
                pt-= bounds.xy;
                if (cast(int)(pt.y * width + pt.x) >= texture.length) {
//                    import std.stdio;
//                    writefln("Error: index out of bounds at %0.1f, %0.1f where bounds=%s, tbounds=%s", pt.x, pt.y, bounds, tbounds);
                } else {
                    texture[cast(int)(pt.y * width + pt.x)] = value;
                }
            }
        }
    }
}

bool pointInPolygon(T, S)(T p, S[] poly, uint groupId) {

    /**
        check lines are crossing on x axis
    */
    pragma(inline, true)
    bool isCrossingXaxis(float y, S p1, S p2) {
        if (p1.position.y > p2.position.y)
            swap(p1, p2);
        return p1.position.y <= y && y <= p2.position.y && p1.position.y != p2.position.y;
    }

    /**
        Gets the point on the X axis that y crosses p1 and p2
    */
    pragma(inline, true)
    float getCrossX(float y, S p1, S p2) {
        return p1.position.x + (y - p1.position.y) / (p2.position.y - p1.position.y) * (p2.position.x - p1.position.x);
    }

    /**
        Gets whether the crossing direction is "up" or "down"
    */
    pragma(inline, true)
    bool getCrossDir(S p1, S p2) {
        return p1.position.y < p2.position.y ? true : false;
    }

    debug assert(poly.length % 2 == 0);
    
    if (groupId > 0 && !groupIdEquals(p, groupId)) return false;
    // Sunday's algorithm
    ptrdiff_t crossings = 0;
    for (size_t i = 0; i < poly.length; i += 2) {
        vec2 p1 = poly[i].xy;
        vec2 p2 = poly[i + 1].xy;
        if (isCrossingXaxis(p.position.y, p1, p2)) {

            // check point is on the left side of the line
            float crossX = getCrossX(p.position.y, p1, p2);
            
            // Check direction of line
            bool dir = getCrossDir(p1, p2);

            if (p.position.x < crossX) {
                if (dir)    crossings++;
                else        crossings--;
            }
        }
    }
    return crossings != 0;
}
