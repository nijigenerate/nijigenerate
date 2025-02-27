module nijigenerate.viewport.vertex.automesh.automesh;

import nijigenerate.viewport.common.mesh;
import nijilive.core;
import std.range;
import std.algorithm;

class AutoMeshProcessor {
public:
    abstract IncMesh autoMesh(Drawable targets, IncMesh meshData, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0);
    abstract void configure();
    abstract string icon();
    IncMesh[] autoMesh(Drawable[] targets, IncMesh[] meshData, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0, void delegate(Drawable, IncMesh) callback = null) {
        return zip(targets, meshData).map!((p) {
            if (callback) { callback(p[0], null); }
            IncMesh result = autoMesh(p[0], p[1], mirrorHoriz, axisHoriz, mirrorVert, axisVert );
            if (callback) { callback(p[0], result); }
            return result;
        }).array;
    }
};