module nijigenerate.core.math.mesh;

import nijigenerate;
import nijigenerate.ext;
import nijigenerate.actions;
import nijigenerate.core.actionstack;
import nijigenerate.core.math.vertex;
import nijigenerate.core.math.triangle;
import nijigenerate.viewport.common.mesheditor.operations.impl;
import nijilive.math;
import nijilive;

private {
    struct Applier(T: Drawable) {
        static auto changeAction(T target) { return new DrawableChangeAction(target.name, target); }
        static void postApply(T target) {
            incUpdateWeldedPoints(target);
        }
        static void rebuffer(V, M)(T target, V vertices, M data = null) {
            target.rebuffer(*data);
        }
    }

    struct Applier(T: Deformable) if (!is(T: Drawable)) {
        static auto changeAction(T target)  { return new DeformableChangeAction(target.name, target); }
        static void postApply(T target) { }
        // Overload for vec2[] directly
        static void rebuffer(M)(T target, vec2[] vertices, M* data = null) {
            target.rebuffer(vertices);
        }
        // Overload for MeshVertex*[]
        static void rebuffer(M)(T target, MeshVertex*[] vertices, M* data = null) {
            target.rebuffer(vertices.toVertices);
        }
    }
}

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


void applyMeshToTarget(T, V, M)(T target, V vertices, M* mesh) {
    incActionPushGroup();
    // Apply the model
    auto action = Applier!T.changeAction(target);
    MeshData data;

    if (mesh) {
        // Export mesh
        data = (*mesh).export_();
        data.fixWinding();

        // Fix UVs
        // By dividing by width and height we should get the values in UV coordinate space.
        target.normalizeUV(&data);
    }

    DeformationParameterBinding[] deformers;

    void alterDeform(ParameterBinding binding) {
        auto deformBinding = cast(DeformationParameterBinding)binding;
        if (!deformBinding)
            return;
        foreach (uint x; 0..cast(uint)deformBinding.values.length) {
            foreach (uint y; 0..cast(uint)deformBinding.values[x].length) {
                auto deform = deformBinding.values[x][y];
                if (deformBinding.isSet(vec2u(x, y))) {
                    auto newDeform = deformByDeformationBinding(vertices, deformBinding, vec2u(x, y), false);
                    if (newDeform) 
                        deformBinding.values[x][y] = *newDeform;
                } else {
                    deformBinding.values[x][y].vertexOffsets.length = vertices.length;
                }
                deformers ~= deformBinding;
            }
        }
    }

    foreach (param; incActivePuppet().parameters) {
        if (auto group = cast(ExParameterGroup)param) {
            foreach(x, ref xparam; group.children) {
                ParameterBinding binding = xparam.getBinding(target, "deform");
                if (binding)
                    action.addAction(new ParameterChangeBindingsAction("Deformation recalculation on mesh update", xparam, null));
                alterDeform(binding);
            }
        } else {
            ParameterBinding binding = param.getBinding(target, "deform");
            if (binding)
                action.addAction(new ParameterChangeBindingsAction("Deformation recalculation on mesh update", param, null));
            alterDeform(binding);
        }
    }
    incActivePuppet().resetDrivers();

    target.clearCache();
    Applier!T.rebuffer(target, vertices, &data);

    // reInterpolate MUST be called after rebuffer is called.
    foreach (deformBinding; deformers) {
        deformBinding.reInterpolate();
    }

    target.notifyChange(target, NotifyReason.StructureChanged);

    action.updateNewState();
    incActionPush(action);

    Applier!T.postApply(target);
    incActionPopGroup();
}

// Same as applyMeshToTarget but does not record an Action to the history.
// Used to synchronize mesh topology during Undo/Redo without clearing Redo.
void applyMeshToTargetNoRecord(T, V, M)(T target, V vertices, M* mesh) {
    // Export mesh if provided
    MeshData data;
    if (mesh) {
        data = (*mesh).export_();
        data.fixWinding();
        target.normalizeUV(&data);
    }

    DeformationParameterBinding[] deformers;

    void alterDeform(ParameterBinding binding) {
        auto deformBinding = cast(DeformationParameterBinding)binding;
        if (!deformBinding)
            return;
        foreach (uint x; 0..cast(uint)deformBinding.values.length) {
            foreach (uint y; 0..cast(uint)deformBinding.values[x].length) {
                auto deform = deformBinding.values[x][y];
                if (deformBinding.isSet(vec2u(x, y))) {
                    auto newDeform = deformByDeformationBinding(vertices, deformBinding, vec2u(x, y), false);
                    if (newDeform)
                        deformBinding.values[x][y] = *newDeform;
                } else {
                    deformBinding.values[x][y].vertexOffsets.length = vertices.length;
                }
                deformers ~= deformBinding;
            }
        }
    }

    foreach (param; incActivePuppet().parameters) {
        if (auto group = cast(ExParameterGroup)param) {
            foreach(x, ref xparam; group.children) {
                ParameterBinding binding = xparam.getBinding(target, "deform");
                alterDeform(binding);
            }
        } else {
            ParameterBinding binding = param.getBinding(target, "deform");
            alterDeform(binding);
        }
    }
    incActivePuppet().resetDrivers();

    target.clearCache();
    Applier!T.rebuffer(target, vertices, &data);

    foreach (deformBinding; deformers) {
        deformBinding.reInterpolate();
    }

    target.notifyChange(target, NotifyReason.StructureChanged);
    Applier!T.postApply(target);
}
