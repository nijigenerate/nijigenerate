module nijigenerate.core.math.mesh;

import nijilive.math;
import nijilive;

struct MeshVertex {
    vec2 position;
    MeshVertex*[] connections;
    uint groupId = 1;
}