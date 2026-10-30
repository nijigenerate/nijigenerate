#pragma once

namespace nijigenerate::core::math::mesh {

struct MeshVertex;

void connect(MeshVertex* self, MeshVertex* other);
void disconnect(MeshVertex* self, MeshVertex* other);
void disconnectAll(MeshVertex* self);
bool isConnectedTo(MeshVertex* self, MeshVertex* other);

template<class T, class V, class M>
void applyMeshToTarget(T target, V vertices, M* mesh, bool preserveDepthBoneBindings = false);

template<class T, class V, class M>
void applyMeshToTargetNoRecord(T target, V vertices, M* mesh);

} // namespace nijigenerate::core::math::mesh
