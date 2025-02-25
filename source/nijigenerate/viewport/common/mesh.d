/*
    Copyright © 2022, Inochi2D Project
    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.

    Authors:
    - Luna Nielsen
    - Asahi Lina

    in_circle() from poly2tri, licensed under BSD-3:
        Copyright (c) 2009-2018, Poly2Tri Contributors
        https://github.com/jhasse/poly2tri
*/
module nijigenerate.viewport.common.mesh;
import nijigenerate.viewport.common.mesheditor.brushes;
import nijilive;
import nijilive.core.dbg;
import bindbc.opengl;
import std.algorithm.mutation;
import std.algorithm;
import nijigenerate.core.math;
public import nijigenerate.core.math.mesh;

class IncMesh {
private:
    MeshData* data;
    vec2 eOrigin;

    void mImport(bool reset = true)(ref MeshData data, mat4 matrix=mat4.identity) {
        // Reset vertex length
        if (reset) {
            vertices.length = 0;
        } else {
            maxGroupId = 2;
        }
        eOrigin = data.origin;

        // Iterate over flat mesh and extract it in to
        // vertices and "connections"
        MeshVertex*[] iVertices;
        iVertices.length = data.vertices.length;
        foreach(idx, vertex; data.vertices) {
            iVertices[idx] = new MeshVertex((matrix * vec4(vertex, 0, 1)).xy, []);
            if (!reset) iVertices[idx].groupId = 2;
        }

        foreach(i; 0..data.indices.length/3) {
            auto index = data.indices[i*3];
            auto nindex = data.indices[(i*3)+1];
            auto nnindex = data.indices[(i*3)+2];

            if (!iVertices[index].isConnectedTo(iVertices[nindex])) iVertices[index].connect(iVertices[nindex]);
            if (!iVertices[nindex].isConnectedTo(iVertices[nnindex])) iVertices[nindex].connect(iVertices[nnindex]);
            if (!iVertices[nnindex].isConnectedTo(iVertices[index])) iVertices[nnindex].connect(iVertices[index]);
        }
        
        void printConnections(MeshVertex* v) {
            import std.stdio;
            ushort[] conns;
            vec2[] coords;
            foreach(conn; v.connections) {
                foreach(key, value; iVertices) {
                    if (value == conn) {
                        conns ~= cast(ushort)key;
                        coords ~= value.position;
                        break;
                    }
                }
            }
        }

        foreach(i, vertex; iVertices) {
            printConnections(vertex);
            vertices ~= vertex;
        }

        axes = [];
        if (reset) {
            if (data.isGrid()) {
                foreach (axis; data.gridAxes) {
                    float[] newAxis;
                    foreach (axValue; axis) {
                        newAxis ~= axValue;
                    }
                    axes ~= newAxis;
                }
            }
        }

        refresh();
    }

    MeshData mExport() {
        import std.algorithm.searching : canFind;
        MeshData* newData = new MeshData;
        newData.origin = eOrigin;

        ushort[MeshVertex*] indices;
        ushort indiceIdx = 0;
        foreach(vertex; vertices) {
            newData.vertices ~= vertex.position;
            newData.uvs ~= vertex.position;
            indices[vertex] = indiceIdx++;
        }

        bool goesBackToRoot(MeshVertex* root, MeshVertex* vert) {
            foreach(MeshVertex* conn; vert.connections) {
                if (conn == root) return true;
            }
            return false;
        }

        bool hasIndiceSeq(ushort a, ushort b, ushort c) {
            foreach(i; 0..newData.indices.length/3) {
                int score = 0;

                if (newData.indices[(i*3)+0] == a || newData.indices[(i*3)+0] == b || newData.indices[(i*3)+0] == c) score++;
                if (newData.indices[(i*3)+1] == a || newData.indices[(i*3)+1] == b || newData.indices[(i*3)+1] == c) score++;
                if (newData.indices[(i*3)+2] == a || newData.indices[(i*3)+2] == b || newData.indices[(i*3)+2] == c) score++;

                if (score == 3) return true;
            }
            return false;
        }

        bool isAnyEdgeIntersecting(vec2[3] t1, vec2[3] t2) {
            vec2 t1p1, t1p2, t2p1, t2p2;
            static foreach(i; 0..3) {
                static foreach(j; 0..3) {
                    t1p1 = t1[i];
                    t1p2 = t1[(i+1)%3];
                    t2p1 = t2[j];
                    t2p2 = t2[(j+1)%3];

                    if (areLineSegmentsIntersecting(t1p1, t1p2, t2p1, t2p2)) return true;
                }
            }
            return false;
        }

        bool isIntersectingWithTris(vec2[3] t1) {
            foreach(i; 0..newData.indices.length/3) {
                vec2[3] verts = [
                    newData.vertices[newData.indices[(i*3)+0]],
                    newData.vertices[newData.indices[(i*3)+0]],
                    newData.vertices[newData.indices[(i*3)+0]]
                ];
                if (isAnyEdgeIntersecting(t1, verts)) return true;
            }
            return false;
        }

        MeshVertex*[] visited;
        void mExportVisit(MeshVertex* v) {
            visited ~= v;

            MeshVertex* findFreeIndice() {
                foreach (key; indices.keys) {
                    if (indices[key] != newData.indices[$-1] && 
                        indices[key] != newData.indices[$-2] && 
                        indices[key] != newData.indices[$-3] && 
                        !visited.canFind(key)) return cast(MeshVertex*)key;
                }
                return null;
            }

            // Second vertex
            foreach(MeshVertex* conn; v.connections) {
                if (conn == v) continue;

                // Third vertex
                foreach(MeshVertex* conn2; conn.connections) {
                    if (goesBackToRoot(v, conn2)) {

                        // Skip repeat sequences
                        if (hasIndiceSeq(indices[v], indices[conn], indices[conn2])) continue;
                        if (isIntersectingWithTris([v.position, conn.position, conn2.position])) continue;
                        

                        // Add new indices
                        newData.indices ~= [
                            indices[v],
                            indices[conn],
                            indices[conn2]
                        ];
                        break;
                    }
                }
            }

            foreach(MeshVertex* conn; v.connections) {
                if (!visited.canFind(conn)) mExportVisit(conn);
            }
        }

        // Run the export
        foreach(ref vert; vertices) {
            if (!visited.canFind(vert)) {
                mExportVisit(vert);
            }
        }

        import std.stdio;
        if (axes.length >= 2) {
            newData.gridAxes = axes[];
        }
        newData.clearGridIsDirty();

        // Save the data as the new data and refresh
        data = newData;
        reset();
        return *newData;
    }

    vec3[] points;
    vec3[] lines;
    vec3[] wlines;
    void regen() {
        points.length = 0;
        
        // Updates all point positions
        foreach(i, vert; vertices) {
            points ~= vec3(vert.position, 0);
        }
    }

    void regenConnections() {
        import std.algorithm.searching : canFind;

        // setup
        lines.length = 0;
        wlines.length = 0;
        MeshVertex*[] visited;
        
        // our crazy recursive func
        void recurseLines(MeshVertex* cur) {
            visited ~= cur;

            // First add the lines
            foreach(conn; cur.connections) {

                // Skip already scanned connections
                if (!visited.canFind(conn)) {
                    lines ~= [vec3(cur.position, 0), vec3(conn.position, 0)];
                }
            }
            // Then scan the next unvisited point
            foreach(conn; cur.connections) {

                // Skip already scanned connections
                if (!visited.canFind(conn)) {
                    recurseLines(conn);
                }
            }
        }

        foreach(ref vert; vertices) {
            if (!visited.canFind(vert)) {
                recurseLines(vert);
            }
        }
    }

public:
    float selectRadius = 16f;
    MeshVertex*[] vertices;
    float[][] axes;
    bool changed;
    uint maxGroupId = 1;

    /**
        Constructs a new IncMesh
    */
    this(ref MeshData mesh) {
        import_(mesh);
    }

    this(IncMesh src) {
        import_(*src.data);
        selectRadius = src.selectRadius;
        maxGroupId = src.maxGroupId;
    }

    final
    void import_(ref MeshData mesh) {
        data = &mesh;
        mImport(mesh);
    }

    final
    void merge_(ref MeshData mesh, mat4 matrix) {
        mImport!false(mesh, matrix);
    }
    
    /**
        Exports the working mesh to a MeshData object.
    */
    final
    MeshData export_() {
        return mExport();
    }

    final
    size_t getEdgeCount() {
        if (lines.length == 0) {
            regenConnections();
        }
        return lines.length;
    }

    final
    bool hasInvalidGeometry() {
        foreach(vertex; vertices) {

            // Vertex has no possibility of having triangles if it has less than 2 connections
            if (vertex.connections.length < 2) return true;
            
            bool madeValidConnection;
            foreach(ref connA; vertex.connections) {
                foreach(ref connB; vertex.connections) {

                    // We don't count connections to one self.
                    if (connA == connB) continue;

                    // Check if we have a triangle
                    if (connA.isConnectedTo(connB)) madeValidConnection = true;
                }
            }

            if (!madeValidConnection) return true;
        }

        return false;
    }

    final
    size_t getTriCount() {
        if (vertices.length == 0) return 0;
        size_t tris;

        MeshVertex*[] visited;

        size_t vtxidx;
        MeshVertex* vtx = vertices[0];
        import std.algorithm.searching : canFind;

        while(vtxidx+1 < vertices.length) {
            if (vtx.connections.length > 1) {
                
                // Array is cleared every iteration
                MeshVertex*[2][] mutual;
                foreach(ref connA; vtx.connections) {
                    bloop: foreach(ref connB; vtx.connections) {

                        // We don't count connections to one self.
                        if (connA == connB) continue;

                        // If we already know a set of points together with vtx creates a triangle,
                        // don't count it twice!
                        foreach(m; mutual) {
                            if ((m[0] == connA || m[0] == connB) && (m[1] == connA || m[1] == connB)) continue bloop;
                        }

                        // Check whether vtx->A and vtx->B are mutually connected in to a triangle
                        // And that they aren't already visited.
                        if (connA.isConnectedTo(connB) && !visited.canFind(connA) && !visited.canFind(connB)) {
                            mutual ~= [connA, connB];
                            tris++;
                        }
                    }
                }
            }

            vtxidx++;
            vtx = vertices[vtxidx];
            visited ~= vtx;
        }
        
        return tris;
    }

    final
    size_t getVertexCount() {
        return vertices.length;
    }

    /**
        Resets mesh to prior state
    */
    void reset() {
        mImport(*data);
        refresh();
        changed = true;
    }

    /**
        Clears the mesh of everything
    */
    void clear() {
        vertices.length = 0;
        refresh();
        changed = true;
    }

    /**
        Refreshes graphical portion of the mesh
    */
    void refresh() {
        regen();
        regenConnections();
    }

    /**
        Draws the mesh
    */
    void drawLines(mat4 trans = mat4.identity, vec4 color = vec4(0.7, 0.7, 0.7, 1)) {
        if (lines.length > 0) {
            inDbgSetBuffer(lines);
            inDbgDrawLines(color, trans);
        }

        if (wlines.length > 0) {
            inDbgSetBuffer(wlines);
            inDbgDrawLines(vec4(0.7, 0.2, 0.2, 1), trans);
        }
    }

    void drawPoints(mat4 trans = mat4.identity) {
        if (points.length > 0) {
            inDbgSetBuffer(points);
            inDbgPointsSize(10);
            inDbgDrawPoints(vec4(0, 0, 0, 1), trans);
            inDbgPointsSize(6);
            inDbgDrawPoints(vec4(1, 1, 1, 1), trans);
        }
    }

    void drawPointSubset(MeshVertex*[] subset, vec4 color, mat4 trans = mat4.identity, float size=6) {
        vec3[] subPoints;

        if (subset.length == 0) return;

        // Updates all point positions
        foreach(vtx; subset) {
            if (vtx !is null)
                subPoints ~= vec3(vtx.position, 0);
        }
        inDbgSetBuffer(subPoints);
        inDbgPointsSize(size);
        inDbgDrawPoints(color, trans);
    }

    void drawPoint(vec2 point, vec4 color, mat4 trans = mat4.identity, float size=6) {
        inDbgSetBuffer([vec3(point, 0)]);
        inDbgPointsSize(size);
        inDbgDrawPoints(color, trans);
    }

    void draw(mat4 trans = mat4.identity) {
        drawLines(trans);
        drawPoints(trans);
    }

    bool isPointOverVertex(vec2 point, float zoomRate) {
        return nijigenerate.core.math.vertex.isPointOverVertex(vertices, point, zoomRate);
    }

    void removeVertexAt(vec2 point, float zoomRate) {
        nijigenerate.core.math.vertex.removeVertexAt!(MeshVertex*, (MeshVertex* i) { this.remove(i); })(vertices, point, zoomRate);
    }

    ulong getVertexFromPoint(vec2 point, float zoomRate) {
        return nijigenerate.core.math.vertex.getVertexFromPoint(vertices, point, zoomRate);
    }

    float[] getVerticesInBrush(vec2 point, Brush brush) {
        return nijigenerate.core.math.vertex.getVerticesInBrush(vertices, point, brush);
    }

    void remove(MeshVertex* vert) {
        import std.algorithm.searching : countUntil;
        import std.algorithm.mutation : remove;
        
        auto idx = vertices.countUntil(vert);
        if (idx != -1) {
            disconnectAll(vert);
            vertices = vertices.remove(idx);
        }
        changed = true;
    }

    vec2[] getOffsets() {
        vec2[] offsets;

        offsets.length = vertices.length;
        foreach(idx, vertex; vertices) {
            offsets[idx] = vertex.position - data.vertices[idx];
        }
        return offsets;
    }

    void applyOffsets(vec2[] offsets) {
        foreach(idx, vertex; vertices) {
            vertex.position += offsets[idx];
        }
        regen();
        regenConnections();
        changed = true;
    }

    /**
        Flips all vertices horizontally
    */
    void flipHorz() {
        foreach(ref vert; vertices) {
            vert.position.x *= -1;
        }
        refresh();
        changed = true;
    }

    /**
        Flips all vertices vertically
    */
    void flipVert() {
        foreach(ref vert; vertices) {
            vert.position.y *= -1;
        }
        refresh();
        changed = true;
    }

    void getBounds(out vec2 min, out vec2 max) {
        nijigenerate.core.math.getBounds(vertices, min, max);
    }

    ulong[] getInRect(vec2 min, vec2 max, uint groupId = 0) {
        return nijigenerate.core.math.getInRect(vertices, min, max, groupId);
    }

    void importVertsAndTris(vec2[] vtx, vec3u[] tris) {
        foreach(v; vtx) {
            this.vertices ~= new MeshVertex(v, []);
        }

        // Extract tris into connections
        foreach(tri; tris) {
            connect(this.vertices[tri.x], this.vertices[tri.y]);
            connect(this.vertices[tri.y], this.vertices[tri.z]);
            connect(this.vertices[tri.z], this.vertices[tri.x]);
        }

    }

    IncMesh autoTriangulate() {
        import std.stdio;
        debug(delaunay) writeln("==== autoTriangulate ====");
        if (vertices.length < 3) return new IncMesh(*data);

        vec2 min, max;
        getBounds(min, max);

        auto vert_ind = triangulate(vertices, vec4(min.xy, max.xy));
        auto vtx = vert_ind[0];
        auto tris = vert_ind[1];

        // Copy vertices
        IncMesh newMesh = new IncMesh(*data);
        newMesh.changed = true;
        newMesh.vertices.length = 0;
        newMesh.importVertsAndTris(vtx, tris);
        newMesh.refresh();
        debug(delaunay) writeln("==== autoTriangulate done ====");
        return newMesh;
    }

    Deformation* deformByDeformationBinding(DeformationParameterBinding binding, vec2u index, bool flipHorz = false) {
        import std.stdio;
        if (!binding) {
            return null;
        }
        Drawable part = cast(Drawable)binding.getTarget().node;
        if (!part) {
            return null;
        }
        Deformation deform = binding.getValue(index);
        return deformByDeformationBinding(part, deform, flipHorz);
    }

    Deformation* deformByDeformationBinding(Drawable part, Deformation deform, bool flipHorz = false) {

        // Check whether deform has more than 1 triangle.
        // If not, returns default Deformation which has dummpy offsets.
        if (deform.vertexOffsets.length < 3 || vertices.length < 3 || part.getMesh().vertices.length < 3) {
            vec2[] vertexOffsets = [];
            for (int i = 0; i < vertices.length; i++)
                vertexOffsets ~= vec2(0, 0);
            return new Deformation(vertexOffsets);
        }

        auto origVertices = vertices.dup;

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
        vec2[] transformMesh(ref MeshData bindingMesh, Deformation deform) {
            vec2[] result;
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
        vec2 transformPointInTriangleCoords(vec2 pt, vec2 offset, vec2[] vertices, ref int[] triangle) {
            auto p1 = vertices[triangle[0]];
            auto p2 = vertices[triangle[1]];
            auto p3 = vertices[triangle[2]];
            vec2 axis0 = p2 - p1;
            vec2 axis1 = p3 - p1;
            return p1 + axis0 * offset.x + axis1 * offset.y;
        }

        MeshData bindingMesh = part.getMesh();
        Deformation* newDeform = new Deformation([]);

        auto targetMesh = transformMesh(bindingMesh, deform);
        foreach (i, v; vertices) {
            vec2 pt = v.position;
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
            newDeform.vertexOffsets ~= newPos - origVertices[i].position;
        }
        return newDeform;
    }

    void copyFromMeshData(MeshData data) {
        mImport(data);
    }
}
