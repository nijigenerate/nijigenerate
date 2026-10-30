#pragma once

namespace nijigenerate::commands::automesh::dynamic {

// Actual public D value type; its field and methods are separate child leaves.
struct AutoMeshKey;

namespace detail {
// Actual private D reference type; no replacement object definition is supplied.
class AutoMeshApplyResult;
class AutoMeshProgressState;
} // namespace detail

} // namespace nijigenerate::commands::automesh::dynamic
