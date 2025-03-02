module nijigenerate.viewport.vertex.automesh.automesh;

import nijigenerate.viewport.common.mesh;
import nijilive.core;
import std.range;
import std.algorithm;

class AutoMeshProcessor {
public:
    abstract IncMesh autoMesh(const Drawable targets, const IncMesh meshData, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0) const;
    abstract void configure();
    abstract string icon();
};