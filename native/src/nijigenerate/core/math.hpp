#pragma once

#include "math/mesh.hpp"
#include "math/vertex.hpp"
#include "math/triangle.hpp"

namespace nijigenerate::core::math {
using mesh::MeshVertex;
using mesh::connect;
using mesh::disconnect;
using mesh::disconnectAll;
using mesh::isConnectedTo;
using mesh::applyMeshToTarget;
using mesh::applyMeshToTargetNoRecord;

using vertex::pointDistance;
using vertex::position;
using vertex::groupIdEquals;
using vertex::isOverlapped;
using vertex::isPointOverVertex;
using vertex::removeVertexAt;
using vertex::getVertexFromPoint;
using vertex::getVerticesInBrush;
using vertex::getBounds;
using vertex::getInRect;
using vertex::findPoint;

using triangle::deformByDeformationBinding;
using triangle::triangulate;
using triangle::getBounds;
using triangle::fillPoly;
using triangle::pointInPolygon;
}
