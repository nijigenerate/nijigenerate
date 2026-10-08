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
        static void postApply(T target, const(ptrdiff_t)[] remap) {
            incUpdateWeldedPoints(target, remap);
        }
        static void rebuffer(V, M)(T target, V vertices, M data = null) {
            target.rebuffer(*data);
        }
    }

    struct Applier(T: Deformable) if (!is(T: Drawable)) {
        static auto changeAction(T target)  { return new DeformableChangeAction(target.name, target); }
        static void postApply(T target, const(ptrdiff_t)[] remap) { }
        // Overload for Vec2Array directly
        static void rebuffer(M)(T target, Vec2Array vertices, M* data = null) {
            target.rebuffer(vertices);
        }
        // Overload for AoS vec2 arrays
        static void rebuffer(M)(T target, Vector!(float, 2)[] vertices, M* data = null) {
            target.rebuffer(Vec2Array(vertices));
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
    size_t originalIndex = size_t.max;
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


void applyMeshToTarget(T, V, M)(T target, V vertices, M* mesh, bool preserveDepthBoneBindings = false) {
    incActionPushGroup();
    // Apply the model
    auto action = Applier!T.changeAction(target);
    auto weldingRemap = new ptrdiff_t[target.vertices.length];
    weldingRemap[] = -1;
    static if (is(T : Drawable)) {
        bool haveIdentity;
        static if (__traits(compiles, (*mesh).isBasedOn(target.getMesh()))) {
            haveIdentity = mesh !is null && (*mesh).isBasedOn(target.getMesh());
        }
        foreach (j, vertex; vertices) {
            static if (is(typeof(vertex) == MeshVertex*)) {
                if (haveIdentity) {
                    if (vertex.originalIndex < weldingRemap.length)
                        weldingRemap[vertex.originalIndex] = cast(ptrdiff_t)j;
                    continue;
                }
            }
            foreach (i, original; target.vertices) {
                if (weldingRemap[i] < 0 && position(vertex) == original) {
                    weldingRemap[i] = cast(ptrdiff_t)j;
                    break;
                }
            }
        }
    }
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
    struct PreservedBinding {
        DeformationParameterBinding binding;
        Deformation[][] values;
    }
    PreservedBinding[] preservedBindings;
    ptrdiff_t[] originalIndices;
    if (preserveDepthBoneBindings) {
        originalIndices.length = vertices.length;
        originalIndices[] = -1;
        foreach (i, vertex; vertices) {
            auto point = position(vertex);
            foreach (j, original; target.vertices) {
                if (point == original) { originalIndices[i] = cast(ptrdiff_t)j; break; }
            }
        }
    }

    void alterDeform(ParameterBinding binding) {
        auto deformBinding = cast(DeformationParameterBinding)binding;
        if (!deformBinding)
            return;
        if (preserveDepthBoneBindings) {
            PreservedBinding snapshot;
            snapshot.binding = deformBinding;
            snapshot.values = deformBinding.values.dup;
            foreach (x, ref row; snapshot.values) {
                row = row.dup;
                foreach (ref value; row) value.vertexOffsets = value.vertexOffsets.dup;
            }
            preservedBindings ~= snapshot;
        }
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
                if (auto deformBinding = cast(DeformationParameterBinding)binding)
                    action.addAction(new ParameterBindingAllValueChangeAction!Deformation(
                        "Deformation recalculation on mesh update", deformBinding, null, !preserveDepthBoneBindings));
                alterDeform(binding);
            }
        } else {
            ParameterBinding binding = param.getBinding(target, "deform");
            if (auto deformBinding = cast(DeformationParameterBinding)binding)
                action.addAction(new ParameterBindingAllValueChangeAction!Deformation(
                    "Deformation recalculation on mesh update", deformBinding, null, !preserveDepthBoneBindings));
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
    // Exact existing samples must not be resampled through overlapping triangles.
    foreach (snapshot; preservedBindings) {
        foreach (x, row; snapshot.values) foreach (y, value; row) {
            foreach (i, original; originalIndices) {
                if (original >= 0 && original < value.vertexOffsets.length)
                    snapshot.binding.values[x][y].vertexOffsets[i] = value.vertexOffsets[original];
            }
        }
    }

    target.notifyChange(target, NotifyReason.StructureChanged);

    action.updateNewState();
    incActionPush(action);

    Applier!T.postApply(target, weldingRemap);
    static if (__traits(compiles, (*mesh).isBasedOn(target.getMesh()))) {
        if (mesh !is null && (*mesh).isBasedOn(target.getMesh())) {
            foreach (i, vertex; (*mesh).vertices) vertex.originalIndex = i;
        }
    }
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
}
