module nijigenerate.core.math.mesh;

import nijilive.math;
import nijilive;

struct MeshVertex {
    vec2 position;
    MeshVertex*[] connections;
    uint groupId = 1;
}

void connect(MeshVertex* self, MeshVertex* other) {
    if (isConnectedTo(self, other)) return;

    self.connections ~= other;
    other.connections ~= self;
}
 
void disconnect(MeshVertex* self, MeshVertex* other) {
    import std.algorithm.searching : countUntil;
    import std.algorithm.mutation : remove;
    
    auto idx = other.connections.countUntil(self);
    if (idx != -1) other.connections = remove(other.connections, idx);

    idx = self.connections.countUntil(other);
    if (idx != -1) self.connections = remove(self.connections, idx);
}

void disconnectAll(MeshVertex* self) {
    while(self.connections.length > 0) {
        self.disconnect(self.connections[0]);
    }
}

bool isConnectedTo(MeshVertex* self, MeshVertex* other) {
    if (other == null) return false;

    foreach(conn; other.connections) {
        if (conn == self) return true;
    }
    return false;
}