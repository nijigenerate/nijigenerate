#pragma once

// Public exports of nijigenerate.viewport.depth.mesheditor.
#include "action.hpp"
#include "editor.hpp"
#include "node.hpp"

namespace nijigenerate::viewport::depth::mesheditor {
using action::DepthOperationListChangeAction;
using action::DepthEditorDepthChangeAction;
using editor::DepthMeshEditor;
using node::DepthDisplayPlaneSize;
using node::DepthDisplayZScale;
using node::DepthMeshEditorOne;
}
