// Public facade for source/nijigenerate/viewport/common/mesheditor/operations/package.d.
#pragma once

#include "base.hpp"
#include "impl.hpp"
#include "node.hpp"

namespace nijigenerate::viewport::common::mesheditor::operations {
using base::IncMeshEditorOne;
using impl::IncMeshEditorOneImpl;
using impl::IncMeshEditorOneDeformable;
using impl::IncMeshEditorOneDrawable;
using impl::getVertices;
using impl::setVertices;
using impl::toVertices;
using impl::toMVertices;
using impl::resize;
using impl::toBool;
using impl::drawPointSubset;
using impl::ngRefineWeldingSeams;
using impl::incUpdateWeldedPoints;
using impl::ngGetEditorFor;

template<class T, project::EditMode mode = project::EditMode::ModelEdit>
using IncMeshEditorOneFor = node::NodeEditorImpl<T, mode>;
}
