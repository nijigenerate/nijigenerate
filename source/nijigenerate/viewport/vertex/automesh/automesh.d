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
    // Standardized identity and ordering; override via mixin/attribute if desired
    string procId() {
        // Default: unqualified type name
        auto tn = typeid(cast(Object)this).toString();
        size_t lastDot = 0; bool hasDot = false; foreach (i, ch; tn) if (ch == '.') { lastDot = i; hasDot = true; }
        return hasDot ? tn[lastDot + 1 .. $] : tn;
    }
    string displayName() { return procId(); }
    int order() { return 0; }
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
