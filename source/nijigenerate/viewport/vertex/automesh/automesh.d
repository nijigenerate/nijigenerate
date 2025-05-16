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
    IncMesh[] autoMesh(Drawable[] targets, IncMesh[] meshData, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0, bool delegate(Drawable, IncMesh) callback = null) {
        IncMesh[] result;
        foreach (ref p; zip(targets, meshData)) {
            if (callback) { 
                if (!callback(p[0], null)) return result;
            }
            IncMesh r = autoMesh(p[0], p[1], mirrorHoriz, axisHoriz, mirrorVert, axisVert );
            result ~= r;
            if (callback) { 
                if (!callback(p[0], r)) return result;
            }
        }
        return result;
    }
};