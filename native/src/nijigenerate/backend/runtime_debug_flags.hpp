#pragma once
// Original independent nijilive/core/dbg.d mutable module variables are TLS.
namespace nicxlive::core::dbg {
inline thread_local bool inDbgDrawMeshOutlines = false;
inline thread_local bool inDbgDrawMeshVertexPoints = false;
inline thread_local bool inDbgDrawMeshOrientation = false;
}
