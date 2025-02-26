module nijigenerate.viewport.vertex.automesh.automesh;

import nijigenerate.viewport.common.mesh;
import nijilive.core;

class AutoMeshProcessor {
public:
    abstract IncMesh autoMesh(Drawable targets, IncMesh meshData, bool mirrorHoriz = false, float axisHoriz = 0, bool mirrorVert = false, float axisVert = 0);
    abstract void configure();
    abstract string icon();
};