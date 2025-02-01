module nijigenerate.core.math.triangle;

import nijigenerate.core.math.vertex;
import nijigenerate.viewport.common.mesh;
import nijilive.math;
import nijilive;


Deformation* deformByDeformationBinding(T)(T[] vertices, DeformationParameterBinding binding, vec2u index, bool flipHorz = false) {
    import std.stdio;
    if (!binding) {
        return null;
    }
    if (auto part = cast(Drawable)binding.getTarget().node) {
        Deformation deform = binding.getValue(index);
        return deformByDeformationBinding(vertices, part, deform, flipHorz);
    } else if (auto deformable = cast(Deformable)binding.getTarget().node) {
        Deformation deform = binding.getValue(index);
        return deformByDeformationBinding(vertices, deformable, deform, flipHorz);
    }
    return null;
}

Deformation* deformByDeformationBinding(T, S: Drawable)(T[] vertices, S part, Deformation deform, bool flipHorz = false) {

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

Deformation* deformByDeformationBinding(T, S: Deformable)(T[] vertices, S deformable, Deformation deform, bool flipHorz = false) {
    // Check whether deform has more than 1 triangle.
    // If not, returns default Deformation which has dummpy offsets.
    if (deform.vertexOffsets.length < 2 || vertices.length < 2 || deformable.vertices.length < 2) {
        vec2[] vertexOffsets = [];
        for (int i = 0; i < vertices.length; i++)
            vertexOffsets ~= vec2(0, 0);
        return new Deformation(vertexOffsets);
    }

    auto origControlPoints     = deformable.vertices.dup;
    auto deformedControlPoints = deformable.vertices.dup;
    foreach (i; 0..origControlPoints.length) {
        deformedControlPoints[i] += deform.vertexOffsets[i];
    }
    auto originalCurve = BezierCurve(origControlPoints);
    auto deformedCurve = BezierCurve(deformedControlPoints);

    vec2[] deformedVertices;
    deformedVertices.length = vertices.length;
    Deformation* newDeform = new Deformation([]);

    foreach (i, v; vertices) {
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

        deformedVertices[i] = deformedVertex;
        if (flipHorz)
            deformedVertices[i] *= -1;
        newDeform.vertexOffsets ~= deformedVertices[i] - position(v);
    }
    return newDeform;
}