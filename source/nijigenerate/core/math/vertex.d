module nijigenerate.core.math.vertex;

import std.algorithm;
import nijilive.math;
import nijilive;
import nijigenerate.viewport;
import nijigenerate.core.math.mesh;
import nijigenerate.viewport.common.mesheditor.brushes.base;

private {
    float selectRadius = 16.0f;
}

float pointDistance(T: vec2)(ref T vertex, vec2 point) { return vertex.distance(point); }
float pointDistance(T: MeshVertex)(ref T vertex, vec2 point) {
    return position(vertex).distance(point);
}
float pointDistance(T: MeshVertex*)(ref T vertex, vec2 point) {
    return position(vertex).distance(point);
}


ref vec2 position(T: vec2)(ref T vertex) {
    return vertex;
}
ref vec2 position(T: MeshVertex)(ref T vertex) {
    return vertex.position;
}
ref vec2 position(T: MeshVertex*)(ref T vertex) {
    return vertex.position;
}

bool groupIdEquals(T: vec2)(T vertex, uint groupId) { return true; }
bool groupIdEquals(T: MeshVertex)(T vertex, uint groupId) { return vertex.groupId == groupId;  }
bool groupIdEquals(T: MeshVertex*)(T vertex, uint groupId) { return vertex.groupId == groupId;  }



bool isPointOverVertex(T)(T[] vertices, vec2 point) {
    foreach(vert; vertices) {
        if (abs(pointDistance(vert, point)) < selectRadius/incViewportZoom) return true;
    }
    return false;
}

void removeVertexAt(T, alias remove)(ref T[] vertices, vec2 point) {
    foreach(i; 0..vertices.length) {
        if (abs(pointDistance(vertices[i], point)) < selectRadius/incViewportZoom) {
            remove(vertices[i]);
            return;
        }
    }
}

ulong getVertexFromPoint(T)(T[] vertices, vec2 point) {
    foreach(idx, ref vert; vertices) {
        if (abs(pointDistance(vert, point)) < selectRadius/incViewportZoom) return idx;
    }
    return -1;
}

float[] getVerticesInBrush(T)(T[] vertices, vec2 point, Brush brush) {
    float[] indices;
    foreach(idx, ref vert; vertices) {
        indices ~= brush.weightAt(point, position(vert));
    }
    return indices;
}

void getBounds(T)(T[] vertices, out vec2 min, out vec2 max) {
    min = vec2(float.infinity, float.infinity);
    max = vec2(-float.infinity, -float.infinity);

    foreach(idx, vertex; vertices) {
        if (min.x > position(vertex).x) min.x = position(vertex).x;
        if (min.y > position(vertex).y) min.y = position(vertex).y;
        if (max.x < position(vertex).x) max.x = position(vertex).x;
        if (max.y < position(vertex).y) max.y = position(vertex).y;
    }
}

ulong[] getInRect(T)(T[] vertices, vec2 min, vec2 max, uint groupId = 0) {
    if (min.x > max.x) swap(min.x, max.x);
    if (min.y > max.y) swap(min.y, max.y);

    ulong[] matching;
    foreach(idx, vertex; vertices) {
        if (min.x > position(vertex).x) continue;
        if (min.y > position(vertex).y) continue;
        if (max.x < position(vertex).x) continue;
        if (max.y < position(vertex).y) continue;
        if (groupId > 0 && groupIdEquals(vertex, groupId)) continue;
        matching ~= idx;
    }

    return matching;
}